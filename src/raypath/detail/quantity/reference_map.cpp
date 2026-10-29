#include "raypath/detail/quantity/reference_map.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "analytic/path_rank.hpp"  // IsRankZeroPath — via BandSum's own arm, not re-derived here
#include "raypath/query_control.hpp"

namespace lumice::raypath::survey {

MapPixelGrid::MapPixelGrid(const ProbeFrame& frame, const MapViewDescription& view) : view_(view) {
  const int count = view.rows * view.cols;
  const double d_row = (view.delta_max_deg - view.delta_min_deg) / view.rows;
  const double d_az = (view.az_max_deg - view.az_min_deg) / view.cols;
  centres_.resize(static_cast<size_t>(count) * 3);
  corners_.resize(static_cast<size_t>(count) * 12);
  solid_angles_.resize(static_cast<size_t>(count));
  for (int r = 0; r < view.rows; r++) {
    for (int c = 0; c < view.cols; c++) {
      const size_t p = static_cast<size_t>(r) * view.cols + c;
      const double d_lo = view.delta_min_deg + r * d_row;
      const double d_hi = d_lo + d_row;
      const double a_lo = view.az_min_deg + c * d_az;
      const double a_hi = a_lo + d_az;
      const double d_c = 0.5 * (d_lo + d_hi);
      const double a_c = 0.5 * (a_lo + a_hi);
      ProbeDirection(frame, d_c * math::kDegreeToRadD, a_c * math::kDegreeToRadD, centres_.data() + 3 * p);
      double* cell = corners_.data() + 12 * p;
      ProbeDirection(frame, d_lo * math::kDegreeToRadD, a_lo * math::kDegreeToRadD, cell);
      ProbeDirection(frame, d_lo * math::kDegreeToRadD, a_hi * math::kDegreeToRadD, cell + 3);
      ProbeDirection(frame, d_hi * math::kDegreeToRadD, a_hi * math::kDegreeToRadD, cell + 6);
      ProbeDirection(frame, d_hi * math::kDegreeToRadD, a_lo * math::kDegreeToRadD, cell + 9);
      solid_angles_[p] = d_row * math::kDegreeToRadD * d_az * math::kDegreeToRadD * std::sin(d_c * math::kDegreeToRadD);
    }
  }
  pixels_.count = count;
  pixels_.centre = centres_.data();
  pixels_.corners = corners_.data();
  pixels_.solid_angle = solid_angles_.data();
}

ReferenceMapData EvaluateReferenceMap(const analytic::FaceNormalTable& table,
                                      const analytic::FacePolygonTable& polygons,
                                      const std::vector<std::vector<int>>& members,
                                      const PoseDensityConversion& density, const ProbeFrame& frame,
                                      const MapViewDescription& view, double wavelength_nm, double refractive_index,
                                      const double incident[3], int sample_count, const analytic::DiscoveryStop* stop) {
  const MapPixelGrid grid(frame, view);
  const analytic::PixelTable& pixels = grid.table();
  const size_t count = static_cast<size_t>(pixels.count);

  ReferenceMapData data;
  data.view = view;
  data.wavelength_nm = wavelength_nm;
  data.refractive_index = refractive_index;
  data.sample_count = sample_count;
  data.member_count = static_cast<int>(members.size());
  data.pixels.resize(count);
  for (int r = 0; r < view.rows; r++) {
    for (int c = 0; c < view.cols; c++) {
      MapPixelSample& sample = data.pixels[static_cast<size_t>(r) * view.cols + c];
      sample.delta_deg =
          view.delta_min_deg + (static_cast<double>(r) + 0.5) * (view.delta_max_deg - view.delta_min_deg) / view.rows;
      sample.azimuth_deg =
          view.az_min_deg + (static_cast<double>(c) + 0.5) * (view.az_max_deg - view.az_min_deg) / view.cols;
    }
  }

  // The cell statuses are view-determined (the same PixelTable for every member): the cells
  // covering the frame's incident or its antipode are singular for the band scan, so the
  // class map reads them from the grid once — NaN, never a written zero.
  {
    const double sun[3] = { -incident[0], -incident[1], -incident[2] };
    for (size_t p = 0; p < count; p++) {
      if (analytic::PixelContains(pixels.corners + 12 * p, incident) ||
          analytic::PixelContains(pixels.corners + 12 * p, sun)) {
        data.pixels[p].status = MapPixelSample::Status::kSingular;
        data.pixels[p].value = std::numeric_limits<double>::quiet_NaN();
      }
    }
  }

  const analytic::PoseDensity pose(density.spec);
  for (size_t m = 0; m < members.size(); m++) {
    if (stop != nullptr) {
      if (stop->token != nullptr) {
        stop->token->NotePoll(kPollMapMembers);
      }
      if (stop->Stopped()) {
        data.truncated = true;  // the completed prefix stays; this member never ran
        break;
      }
    }
    std::vector<int> slots;
    slots.reserve(members[m].size());
    for (const int face : members[m]) {
      slots.push_back(table.SlotOf(face));
    }
    const analytic::BandSumOutput band =
        analytic::BandSum(table, polygons, slots.data(), static_cast<int>(slots.size()), refractive_index, incident,
                          sample_count, pixels, pose, stop);
    if (band.truncated) {
      // The cut member's partial-lattice sums are discarded, never mixed with the completed
      // prefix's full-lattice values; the coverage fields name what actually ran.
      data.truncated = true;
      data.kept_count_truncated_member = band.kept_count;
      break;
    }
    data.members_evaluated++;
    data.member_kept_counts.push_back(band.kept_count);
    data.kept_count += band.kept_count;
    if (band.rank_zero) {
      // The rank-0 arm: the member's whole contribution is the point mass — summed here,
      // never written into the per-sr pixels (its own pixel array is the mass placement).
      if (!data.point_mass.has_value()) {
        data.point_mass = MapPointMass{};
      }
      data.point_mass->m += band.m;
      data.point_mass->m_error += band.m_error;
      std::string method = band.method == analytic::PointMassMethod::kLatticeMean ? "lattice_mean" : "psi_average";
      if (std::find(data.point_mass->methods.begin(), data.point_mass->methods.end(), method) ==
          data.point_mass->methods.end()) {
        data.point_mass->methods.push_back(std::move(method));
      }
    } else {
      // The band arm: the member's per-sr values sum member-by-member (the same lattice, the
      // same cells — the value formula's n / band / sin(delta) factors are member-independent,
      // so the sum of the normalized values is the class's normalized sum).
      for (size_t p = 0; p < count; p++) {
        MapPixelSample& sample = data.pixels[p];
        if (sample.status == MapPixelSample::Status::kSingular) {
          continue;  // NaN stays NaN; the member's singular cell carried no value either
        }
        const analytic::PixelValue& pixel = band.pixels[p];
        sample.value += pixel.value;
        sample.k += pixel.k;
      }
    }
  }

  // The rank-0 arm's placement: the summed mass rides the SAME PlacePointMass primitive onto
  // this view — the cell holding the frame's incident carries m_total / Omega, everything else
  // keeps its band value (the mass never smears).
  if (data.point_mass.has_value()) {
    std::vector<analytic::PixelValue> placement;
    placement.reserve(count);
    PlacePointMass(data.point_mass->m, incident, pixels, &placement, &data.point_mass->pixel);
    for (size_t p = 0; p < count; p++) {
      if (placement[p].status == analytic::PixelStatus::kPointMass) {
        data.pixels[p].status = MapPixelSample::Status::kPointMass;
        data.pixels[p].value = placement[p].value;
      }
    }
  }
  return data;
}

}  // namespace lumice::raypath::survey
