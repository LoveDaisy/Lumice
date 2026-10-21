#include "analytic/family_rank.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/jet.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"

namespace lumice::analytic {

namespace {

constexpr double kDegToRad = math::kDegreeToRadD;

// The pose chart (EulerPose) and the sky read (ImageAltAzDeg) live in the module header — the
// single authority both this kernel and the survey assembly consume.

// ---- the grid -----------------------------------------------------------------------------------

// One free dimension's sampling axis: the periodic midpoint grid (the orbit producer's) for
// azimuth and roll, the inclusive uniform grid over [0, 180] for zenith. count >= 2 (the entry's
// validation), so the interval spacing never divides by zero.
struct GridAxis {
  FamilyDimension dim = FamilyDimension::kAzimuth;
  int count = 0;
  bool periodic = true;
  double spacing_deg = 0.0;

  double ValueDeg(int i) const { return periodic ? 360.0 * (i + 0.5) / count : 180.0 * i / (count - 1); }
};

// The spec's free dimensions in canonical (zenith, azimuth, roll) order, with their axes.
void FreeAxes(const RestrictedFamilySpec& spec, int grid, GridAxis axes[2], int* free_count) {
  *free_count = 0;
  const FamilyDimensionSpec* dims[3] = { &spec.zenith, &spec.azimuth, &spec.roll };
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  for (int i = 0; i < 3; i++) {
    if (dims[i]->mode == FamilyDimensionMode::kFree) {
      GridAxis& axis = axes[(*free_count)++];
      axis.dim = names[i];
      axis.count = grid;
      axis.periodic = names[i] != FamilyDimension::kZenith;
      axis.spacing_deg = axis.periodic ? 360.0 / grid : 180.0 / (grid - 1);
    }
  }
}

// The value of every dimension at a scan point: the free ones from params (canonical slot
// order, degrees), the frozen ones at their centers.
void DimensionDegrees(const RestrictedFamilySpec& spec, const GridAxis axes[2], int free_count,
                      const double params_deg[2], double degrees_out[3]) {
  double degrees[3] = { spec.zenith.center_deg, spec.azimuth.center_deg, spec.roll.center_deg };
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  for (int k = 0; k < free_count; k++) {
    for (int d = 0; d < 3; d++) {
      if (axes[k].dim == names[d]) {
        degrees[d] = params_deg[k];
      }
    }
  }
  degrees_out[0] = degrees[0];
  degrees_out[1] = degrees[1];
  degrees_out[2] = degrees[2];
}

// ---- the per-sample evaluation --------------------------------------------------------------------

struct Sample {
  bool valid = false;
  double outgoing[3] = {};
  double rows[2][3] = {};  // d(outgoing)/d(free dim k), already image-tangent
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  int rank_class = 0;
  double signed_jacobian = 0.0;  // free_count == 2 only
};

// One Jet<2> chain trace at free-dimension DEGREES (canonical slot order): the free dimensions
// seeded as chart variables, the frozen ones constants. The chain runs in kFirstFailure, so a
// gate rejection returns false — the invalid region — and every accepted outgoing is finite.
bool EvaluateSample(const FaceNormalTable& normals, const int* slots, int slot_count, const double sun_hat[3],
                    double refractive_index, int free_count, const GridAxis axes[2], const RestrictedFamilySpec& spec,
                    const double params_deg[2], Sample* out) {
  double degrees[3];
  DimensionDegrees(spec, axes, free_count, params_deg, degrees);
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  Jet<2> angle[3];
  int slot = 0;
  for (int d = 0; d < 3; d++) {
    bool is_free = false;
    for (int k = 0; k < free_count; k++) {
      is_free = is_free || axes[k].dim == names[d];
    }
    if (is_free) {
      angle[d] = Jet<2>::Variable(degrees[d] * kDegToRad, slot);
      slot++;
    } else {
      angle[d] = Jet<2>(degrees[d] * kDegToRad);
    }
  }
  Jet<2> pose[9];
  EulerPose(angle[1], angle[0], angle[2], pose);
  const Jet<2> incident[3] = { Jet<2>(-sun_hat[0]), Jet<2>(-sun_hat[1]), Jet<2>(-sun_hat[2]) };
  Jet<2> outgoing[3];
  if (!TracePathChain(normals, slots, slot_count, Jet<2>(refractive_index), incident, pose, outgoing, nullptr,
                      nullptr)) {
    return false;
  }
  for (int i = 0; i < 3; i++) {
    out->outgoing[i] = outgoing[i].a;
    for (int k = 0; k < free_count; k++) {
      out->rows[k][i] = outgoing[i].v[k];
    }
  }
  if (free_count == 2) {
    // The two rows are tangent to the image sphere at the outgoing (|outgoing| = 1 by
    // construction), so the Gram-eigenvalue pair IS the restricted singular-value pair.
    const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(out->rows[0], out->rows[1]);
    out->sigma1 = svd.sigma1;
    out->sigma2 = svd.sigma2;
    // The orientation Jacobian in a tangent basis: every basis TangentBasis builds is
    // right-handed about the outgoing (basis[0] x basis[1] = d), so the SIGN is basis-choice
    // independent and its flip is the fold. |det| = sigma1 * sigma2 by Lagrange.
    double basis[2][3];
    so3::TangentBasis(out->outgoing, basis);
    const double m00 = so3::Dot3(basis[0], out->rows[0]);
    const double m01 = so3::Dot3(basis[0], out->rows[1]);
    const double m10 = so3::Dot3(basis[1], out->rows[0]);
    const double m11 = so3::Dot3(basis[1], out->rows[1]);
    out->signed_jacobian = m00 * m11 - m01 * m10;
  } else {
    out->sigma1 = so3::Norm3(out->rows[0]);
    out->sigma2 = 0.0;
  }
  out->rank_class = out->sigma2 >= kFamilyRankSigmaTolerance ? 2 : (out->sigma1 >= kFamilyRankSigmaTolerance ? 1 : 0);
  out->valid = true;
  return true;
}

// ---- the union-find of the grid structures -----------------------------------------------------------

struct DisjointSet {
  std::vector<int> parent;

  explicit DisjointSet(int count) : parent(static_cast<size_t>(count)) {
    for (int i = 0; i < count; i++) {
      parent[static_cast<size_t>(i)] = i;
    }
  }

  int Find(int x) {
    while (parent[static_cast<size_t>(x)] != x) {
      parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
      x = parent[static_cast<size_t>(x)];
    }
    return x;
  }

  void Union(int a, int b) {
    a = Find(a);
    b = Find(b);
    if (a != b) {
      parent[static_cast<size_t>(b)] = a;
    }
  }
};

// The next index along `axis` in `direction`, or -1 at a non-periodic bound.
int StepIndex(const GridAxis& axis, int i, int direction) {
  int j = i + direction;
  if (axis.periodic) {
    if (j < 0) {
      j += axis.count;
    } else if (j >= axis.count) {
      j -= axis.count;
    }
    return j;
  }
  return (j < 0 || j >= axis.count) ? -1 : j;
}

}  // namespace

// ---- validation and routing ------------------------------------------------------------------------

const char* FamilySpecError(const RestrictedFamilySpec& spec) {
  static const char* const kNotFinite[3] = { "the fixed zenith center must be finite",
                                             "the fixed azimuth center must be finite",
                                             "the fixed roll center must be finite" };
  static const char* const kOutOfRange[3] = { "the fixed zenith center is outside [0, 180]",
                                              "the fixed azimuth center is outside [0, 360)",
                                              "the fixed roll center is outside [0, 360)" };
  static const char* const kFreeCenter[3] = { "a free zenith takes no center: it must be exactly 0",
                                              "a free azimuth takes no center: it must be exactly 0",
                                              "a free roll takes no center: it must be exactly 0" };
  const FamilyDimensionSpec* const dims[3] = { &spec.zenith, &spec.azimuth, &spec.roll };
  const double max_center[3] = { 180.0, 360.0, 360.0 };  // exclusive: zenith [0, 180], others [0, 360)
  for (int i = 0; i < 3; i++) {
    if (dims[i]->mode == FamilyDimensionMode::kFixed) {
      if (!std::isfinite(dims[i]->center_deg)) {
        return kNotFinite[i];
      }
      if (dims[i]->center_deg < 0.0 || dims[i]->center_deg >= max_center[i]) {
        return kOutOfRange[i];
      }
    } else if (dims[i]->center_deg != 0.0) {
      return kFreeCenter[i];
    }
  }
  return "";
}

const char* FamilyRankStatusName(FamilyRankStatus status) {
  switch (status) {
    case FamilyRankStatus::kOk:
      return "ok";
    case FamilyRankStatus::kRefusedSinglePose:
      return "refused: all dimensions fixed (a single pose — EvaluatePath is the entry)";
    case FamilyRankStatus::kRefusedFullSo3:
      return "refused: all dimensions free (full SO(3) — Classify is the entry)";
    case FamilyRankStatus::kRefusedAzimuthFrozen:
      return "refused: azimuth fixed while another dimension is free (outside the routing v1 domain)";
    case FamilyRankStatus::kRefusedInvalidSpec:
      return "refused: malformed family spec";
    case FamilyRankStatus::kRefusedGridOutOfBounds:
      return "refused: the grid count is outside its bounds";
  }
  return "unknown";
}

// The shared refusal routing of both entries; kOk or the refusal.
FamilyRankStatus RouteFamilySpec(const RestrictedFamilySpec& spec, int* free_count) {
  *free_count = (spec.zenith.mode == FamilyDimensionMode::kFree ? 1 : 0) +
                (spec.azimuth.mode == FamilyDimensionMode::kFree ? 1 : 0) +
                (spec.roll.mode == FamilyDimensionMode::kFree ? 1 : 0);
  if (FamilySpecError(spec)[0] != '\0') {
    return FamilyRankStatus::kRefusedInvalidSpec;
  }
  if (*free_count == 0) {
    return FamilyRankStatus::kRefusedSinglePose;
  }
  if (*free_count == 3) {
    return FamilyRankStatus::kRefusedFullSo3;
  }
  if (spec.azimuth.mode == FamilyDimensionMode::kFixed) {
    // *free_count is 1 or 2 here, so another dimension is free: outside the routing v1 domain.
    return FamilyRankStatus::kRefusedAzimuthFrozen;
  }
  return FamilyRankStatus::kOk;
}

FamilyPointRecord EvaluateFamilyPoint(const FaceNormalTable& normals, const int* slots, int slot_count,
                                      const RestrictedFamilySpec& spec, const double sun_hat[3],
                                      double refractive_index, const double params_deg[2]) {
  FamilyPointRecord out;
  int free_count = 0;
  out.status = RouteFamilySpec(spec, &free_count);
  if (out.status != FamilyRankStatus::kOk) {
    return out;
  }
  GridAxis axes[2];
  int axes_count = 0;
  FreeAxes(spec, 2, axes, &axes_count);
  Sample sample;
  if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, free_count, axes, spec, params_deg,
                      &sample)) {
    return out;  // the chain gates' verdict: invalid data, status stays kOk
  }
  out.valid = true;
  for (int i = 0; i < 3; i++) {
    out.outgoing[i] = sample.outgoing[i];
  }
  out.sigma1 = sample.sigma1;
  out.sigma2 = sample.sigma2;
  out.rank_class = sample.rank_class;
  out.signed_jacobian = sample.signed_jacobian;
  return out;
}

RestrictedFamilyRank RankRestrictedFamily(const FaceNormalTable& normals, const int* slots, int slot_count,
                                          const RestrictedFamilySpec& spec, const double sun_hat[3],
                                          double refractive_index, int grid) {
  RestrictedFamilyRank out;
  out.grid = grid;
  int free_count = 0;
  out.status = RouteFamilySpec(spec, &free_count);
  if (out.status != FamilyRankStatus::kOk) {
    return out;
  }
  if (grid < kFamilyMinGridPerDimension || grid > kFamilyMaxGridPerDimension) {
    out.status = FamilyRankStatus::kRefusedGridOutOfBounds;
    return out;
  }

  GridAxis axes[2];
  FreeAxes(spec, grid, axes, &out.free_count);
  for (int k = 0; k < out.free_count; k++) {
    out.free_dimensions[k] = axes[k].dim;
    out.grid_spacing_deg[k] = axes[k].spacing_deg;
  }
  const int n0 = axes[0].count;
  const int n1 = out.free_count == 2 ? axes[1].count : 1;
  out.samples = n0 * n1;

  // The product scan.
  std::vector<Sample> samples(static_cast<size_t>(out.samples));
  for (int i0 = 0; i0 < n0; i0++) {
    for (int i1 = 0; i1 < n1; i1++) {
      const double params[2] = { axes[0].ValueDeg(i0), axes[1].ValueDeg(i1) };
      EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, out.free_count, axes, spec, params,
                     &samples[static_cast<size_t>(i0 * n1 + i1)]);
    }
  }
  bool have_sigma = false;
  for (const Sample& sample : samples) {
    if (!sample.valid) {
      out.invalid_samples++;
      continue;
    }
    out.valid_samples++;
    out.generic_rank = std::max(out.generic_rank, sample.rank_class);
    if (!have_sigma) {
      out.min_sigma1 = sample.sigma1;
      out.min_sigma2 = sample.sigma2;
      have_sigma = true;
    } else {
      out.min_sigma1 = std::min(out.min_sigma1, sample.sigma1);
      out.min_sigma2 = std::min(out.min_sigma2, sample.sigma2);
    }
    if (sample.rank_class == 2) {
      out.rank2_samples++;
    } else if (sample.rank_class == 1) {
      out.rank1_samples++;
    } else {
      out.rank0_samples++;
    }
  }
  if (out.free_count == 1) {
    out.min_sigma2 = 0.0;  // not a quantity of a one-dimensional family
  }

  // The fold locus (two-dimensional families only): the orientation Jacobian's sign change
  // across a grid edge, bisected along the edge. An edge's parameter runs FORWARD from its first
  // vertex's value by one spacing — a periodic wrap just passes through 360, which the chart's
  // trigonometry absorbs; the record's params fold back into [0, 360).
  if (out.free_count == 2) {
    struct EdgeRef {
      int dim;  // which dimension varies along the edge
      int i0;
      int i1;
    };
    const int dim0_edges = axes[0].periodic ? n0 : n0 - 1;
    const int total_edges = dim0_edges * n1 + n0 * n1;  // + the dim1 edges (always periodic)
    std::vector<int> edge_crossing(static_cast<size_t>(total_edges), -1);
    const auto edge_key = [&](const EdgeRef& edge) {
      return edge.dim == 0 ? edge.i0 * n1 + edge.i1 : dim0_edges * n1 + edge.i0 * n1 + edge.i1;
    };

    // Locate this edge's crossing and append its record, or do nothing. Returns the crossing
    // index (>= 0) or -1.
    const auto locate_crossing = [&](const EdgeRef& edge) -> int {
      const int j0 = edge.dim == 0 ? StepIndex(axes[0], edge.i0, 1) : edge.i0;
      const int j1 = edge.dim == 1 ? StepIndex(axes[1], edge.i1, 1) : edge.i1;
      if (j0 < 0 || j1 < 0) {
        return -1;
      }
      const Sample& a = samples[static_cast<size_t>(edge.i0 * n1 + edge.i1)];
      const Sample& b = samples[static_cast<size_t>(j0 * n1 + j1)];
      if (!a.valid || !b.valid || a.signed_jacobian * b.signed_jacobian >= 0.0) {
        return -1;
      }
      double base[2] = { axes[0].ValueDeg(edge.i0), axes[1].ValueDeg(edge.i1) };
      const double lo = base[edge.dim];
      const double hi = lo + axes[edge.dim].spacing_deg;
      double t_lo = 0.0;
      double t_hi = 1.0;
      for (int step = 0; step < kFoldBisectionSteps; step++) {
        const double t = 0.5 * (t_lo + t_hi);
        double probe[2] = { base[0], base[1] };
        probe[edge.dim] = lo + t * (hi - lo);
        Sample mid;
        if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, probe, &mid)) {
          return -1;  // the refinement fell into the invalid region: no located crossing
        }
        if (mid.signed_jacobian * a.signed_jacobian > 0.0) {
          t_lo = t;
        } else {
          t_hi = t;
        }
      }
      const double t = 0.5 * (t_lo + t_hi);
      double refined[2] = { base[0], base[1] };
      refined[edge.dim] = lo + t * (hi - lo);
      Sample located;
      if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, refined, &located)) {
        return -1;
      }
      FoldPointRecord record;
      for (int k = 0; k < 2; k++) {
        record.params_deg[k] = axes[k].periodic ? std::fmod(refined[k], 360.0) : refined[k];
      }
      double degrees[3];
      DimensionDegrees(spec, axes, 2, refined, degrees);
      EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, record.pose);
      for (int i = 0; i < 3; i++) {
        record.outgoing[i] = located.outgoing[i];
      }
      ImageAltAzDeg(record.outgoing, &record.image_alt_deg, &record.image_az_deg);
      record.sigma1 = located.sigma1;
      record.sigma2 = located.sigma2;
      record.signed_jacobian = located.signed_jacobian;
      out.crossings.push_back(record);
      return static_cast<int>(out.crossings.size()) - 1;
    };

    for (int i0 = 0; i0 < dim0_edges; i0++) {
      for (int i1 = 0; i1 < n1; i1++) {
        const EdgeRef edge = { 0, i0, i1 };
        const int crossing = locate_crossing(edge);
        if (crossing >= 0) {
          edge_crossing[static_cast<size_t>(edge_key(edge))] = crossing;
        }
      }
    }
    for (int i0 = 0; i0 < n0; i0++) {
      for (int i1 = 0; i1 < n1; i1++) {
        const EdgeRef edge = { 1, i0, i1 };
        const int crossing = locate_crossing(edge);
        if (crossing >= 0) {
          edge_crossing[static_cast<size_t>(edge_key(edge))] = crossing;
        }
      }
    }

    // Connectivity at grid resolution: crossings on the four edges of a common cell are one
    // component (edges sharing only a vertex always bound a common cell, so cells suffice).
    const int crossing_count = static_cast<int>(out.crossings.size());
    if (crossing_count > 0) {
      DisjointSet set(crossing_count);
      for (int i0 = 0; i0 < n0; i0++) {
        for (int i1 = 0; i1 < n1; i1++) {
          const int i0_up = StepIndex(axes[0], i0, 1);
          const int i1_up = StepIndex(axes[1], i1, 1);
          if (i0_up < 0 || i1_up < 0) {
            continue;  // the zenith interval's last row has no upper cells
          }
          const EdgeRef cell[4] = { { 0, i0, i1 }, { 0, i0, i1_up }, { 1, i0, i1 }, { 1, i0_up, i1 } };
          int members[4];
          int member_count = 0;
          for (const EdgeRef& edge : cell) {
            const int crossing = edge_crossing[static_cast<size_t>(edge_key(edge))];
            if (crossing >= 0) {
              members[member_count++] = crossing;
            }
          }
          for (int m = 1; m < member_count; m++) {
            set.Union(members[0], members[m]);
          }
        }
      }
      // Components in first-crossing order, each with its smallest-sigma2 representative.
      std::vector<int> first_of_root(static_cast<size_t>(crossing_count), -1);
      for (int c = 0; c < crossing_count; c++) {
        const int root = set.Find(c);
        if (first_of_root[static_cast<size_t>(root)] < 0) {
          first_of_root[static_cast<size_t>(root)] = c;
        }
      }
      for (int c = 0; c < crossing_count; c++) {
        if (first_of_root[static_cast<size_t>(set.Find(c))] != c) {
          continue;
        }
        FoldComponentRecord component;
        component.first_crossing = c;
        component.representative = c;
        for (int d = c; d < crossing_count; d++) {
          if (set.Find(d) == set.Find(c)) {
            component.crossing_count++;
            if (out.crossings[static_cast<size_t>(d)].sigma2 <
                out.crossings[static_cast<size_t>(component.representative)].sigma2) {
              component.representative = d;
            }
          }
        }
        out.fold_components.push_back(component);
      }
    }
  }

  // The collapse components: four-connected class-0 samples over the grid (a periodic
  // dimension's wrap connects through it).
  if (out.rank0_samples > 0) {
    DisjointSet set(out.samples);
    for (int i0 = 0; i0 < n0; i0++) {
      for (int i1 = 0; i1 < n1; i1++) {
        const Sample& here = samples[static_cast<size_t>(i0 * n1 + i1)];
        if (!here.valid || here.rank_class != 0) {
          continue;
        }
        const int j0 = StepIndex(axes[0], i0, 1);
        const int j1 = out.free_count == 2 ? StepIndex(axes[1], i1, 1) : -1;
        const int neighbors[2][2] = { { j0, i1 }, { i0, j1 } };
        for (const auto& neighbor : neighbors) {
          if (neighbor[0] < 0 || neighbor[1] < 0) {
            continue;
          }
          const Sample& there = samples[static_cast<size_t>(neighbor[0] * n1 + neighbor[1])];
          if (there.valid && there.rank_class == 0) {
            set.Union(i0 * n1 + i1, neighbor[0] * n1 + neighbor[1]);
          }
        }
      }
    }
    std::vector<int> first_of_root(static_cast<size_t>(out.samples), -1);
    for (int s = 0; s < out.samples; s++) {
      const Sample& sample = samples[static_cast<size_t>(s)];
      if (!sample.valid || sample.rank_class != 0) {
        continue;
      }
      const int root = set.Find(s);
      if (first_of_root[static_cast<size_t>(root)] < 0) {
        first_of_root[static_cast<size_t>(root)] = s;
      }
    }
    for (int s = 0; s < out.samples; s++) {
      const Sample& sample = samples[static_cast<size_t>(s)];
      if (!sample.valid || sample.rank_class != 0 || first_of_root[static_cast<size_t>(set.Find(s))] != s) {
        continue;
      }
      // One component: its size and its smallest-sigma1 representative.
      CollapseRecord record;
      int representative = s;
      for (int d = s; d < out.samples; d++) {
        const Sample& other = samples[static_cast<size_t>(d)];
        if (!other.valid || other.rank_class != 0 || set.Find(d) != set.Find(s)) {
          continue;
        }
        record.sample_count++;
        if (other.sigma1 < samples[static_cast<size_t>(representative)].sigma1) {
          representative = d;
        }
      }
      const double params[2] = { axes[0].ValueDeg(representative / n1),
                                 out.free_count == 2 ? axes[1].ValueDeg(representative % n1) : 0.0 };
      for (int k = 0; k < out.free_count; k++) {
        record.params_deg[k] = axes[k].periodic ? std::fmod(params[k], 360.0) : params[k];
      }
      double degrees[3];
      DimensionDegrees(spec, axes, out.free_count, params, degrees);
      EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, record.pose);
      const Sample& rep = samples[static_cast<size_t>(representative)];
      for (int i = 0; i < 3; i++) {
        record.outgoing[i] = rep.outgoing[i];
      }
      ImageAltAzDeg(record.outgoing, &record.image_alt_deg, &record.image_az_deg);
      record.sigma1 = rep.sigma1;
      out.collapse_components.push_back(record);
    }
  }

  return out;
}

}  // namespace lumice::analytic
