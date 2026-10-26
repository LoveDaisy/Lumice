#include "raypath/detail/structure/family.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::structure {
namespace {

using survey::FamilyDimKind;
using survey::FamilyDimMeasure;
using survey::FamilyRoutingMeasure;

constexpr double kDegToRad = math::kDegreeToRadD;
constexpr double kRadToDeg = math::kRadToDegreeD;

// A center normalized into its dimension's legal range (FamilySpecError's own bounds): zenith
// clamped to [0, 180] — a gauss centred past the pole reads as truncated at the pole, and the
// nearest pole is its freeze — and azimuth/roll folded into [0, 360).
double NormalizeCenter(analytic::FamilyDimension dim, double center_deg) {
  if (dim == analytic::FamilyDimension::kZenith) {
    return std::clamp(center_deg, 0.0, 180.0);
  }
  double folded = std::fmod(center_deg, 360.0);
  if (folded < 0.0) {
    folded += 360.0;
  }
  return folded;
}

// ---- the geometry helpers (the chart and the sky read) -----------------------------------------------
//
// The chart (analytic::EulerPoseDeg) and the sky read (analytic::ImageAltAzDeg) are the kernel
// header's own formulas (family_rank.hpp) — consumed, not re-derived here.

// The unit direction of an altitude/azimuth pair on the world sphere.
void DirectionOf(double alt_deg, double az_deg, double out[3]) {
  const double alt = alt_deg * kDegToRad;
  const double az = az_deg * kDegToRad;
  out[0] = std::cos(alt) * std::cos(az);
  out[1] = std::cos(alt) * std::sin(az);
  out[2] = std::sin(alt);
}

// atan2 of |a x b| and a . b — accurate at 0, where acos of the dot loses sqrt(eps).
double AngleBetweenDeg(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double norm = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return std::atan2(norm, dot) * kRadToDeg;
}

// ---- the ladder --------------------------------------------------------------------------------------

// The spec of ladder level k: the measure's fixed dimensions and the first k freeze candidates
// pinned at their centers, everything else free (a free center stays exactly 0, the ABI's own
// convention).
struct IdealFamilyScope {
  analytic::RestrictedFamilySpec spec;
};

IdealFamilyScope LevelSpec(const FamilyRoutingMeasure& measure, const std::vector<FamilyFreezeCandidate>& candidates,
                           int k) {
  const FamilyDimMeasure* dims[3] = { &measure.zenith, &measure.azimuth, &measure.roll };
  const analytic::FamilyDimension names[3] = { analytic::FamilyDimension::kZenith, analytic::FamilyDimension::kAzimuth,
                                               analytic::FamilyDimension::kRoll };
  double centers[3] = { 0.0, 0.0, 0.0 };
  bool frozen[3] = { false, false, false };
  for (int i = 0; i < 3; i++) {
    if (dims[i]->kind == FamilyDimKind::kFixed) {
      frozen[i] = true;
      centers[i] = NormalizeCenter(names[i], dims[i]->center_deg);
    }
  }
  for (int j = 0; j < k && j < static_cast<int>(candidates.size()); j++) {
    for (int i = 0; i < 3; i++) {
      if (names[i] == candidates[static_cast<size_t>(j)].dim) {
        frozen[i] = true;
        centers[i] = candidates[static_cast<size_t>(j)].center_deg;
      }
    }
  }
  analytic::RestrictedFamilySpec spec;
  analytic::FamilyDimensionSpec* slots[3] = { &spec.zenith, &spec.azimuth, &spec.roll };
  for (int i = 0; i < 3; i++) {
    slots[i]->mode = frozen[i] ? analytic::FamilyDimensionMode::kFixed : analytic::FamilyDimensionMode::kFree;
    slots[i]->center_deg = frozen[i] ? centers[i] : 0.0;
  }
  return { spec };
}

// The frozen-dimension report of one level: the measure's own fixed dimensions (width 0) then
// the ladder's freezes (the candidate's width) — the freeze value and the frozen width reported
// together, canonical dimension order.
std::vector<FamilyFrozenDim> FrozenReport(const FamilyRoutingMeasure& measure,
                                          const std::vector<FamilyFreezeCandidate>& candidates, int k) {
  const FamilyDimMeasure* dims[3] = { &measure.zenith, &measure.azimuth, &measure.roll };
  const analytic::FamilyDimension names[3] = { analytic::FamilyDimension::kZenith, analytic::FamilyDimension::kAzimuth,
                                               analytic::FamilyDimension::kRoll };
  std::vector<FamilyFrozenDim> out;
  for (int i = 0; i < 3; i++) {
    if (dims[i]->kind != FamilyDimKind::kFixed) {
      continue;
    }
    FamilyFrozenDim row;
    row.dim = names[i];
    row.center_deg = NormalizeCenter(names[i], dims[i]->center_deg);
    row.width_deg = 0.0;
    out.push_back(std::move(row));
  }
  for (int j = 0; j < k && j < static_cast<int>(candidates.size()); j++) {
    const FamilyFreezeCandidate& candidate = candidates[static_cast<size_t>(j)];
    FamilyFrozenDim row;
    row.dim = candidate.dim;
    row.center_deg = candidate.center_deg;
    row.width_deg = candidate.width_deg;
    out.push_back(std::move(row));
  }
  return out;
}

// ---- the survival filter (structure proposes, weight disposes) ----------------------------------------

// One point the filter probes from: the family parameters and the image they map to.
struct BlurAnchor {
  double params_deg[2] = { 0.0, 0.0 };
  double image_dir[3] = { 0.0, 0.0, 0.0 };  // -outgoing at the anchor
  double outgoing[3] = { 0.0, 0.0, 0.0 };   // the raw propagation direction the image came from
};

void AnchorOfOutgoing(const double params_deg[2], const double outgoing[3], BlurAnchor* anchor) {
  anchor->params_deg[0] = params_deg[0];
  anchor->params_deg[1] = params_deg[1];
  for (int c = 0; c < 3; c++) {
    anchor->image_dir[c] = -outgoing[c];
    anchor->outgoing[c] = outgoing[c];
  }
}

// The extent probe set of an extended candidate (a declared, deterministic budget of at most
// kFamilyExtentProbeCount evaluations): the one-dimensional set is the free dimension's
// midpoint axis, the two-dimensional set the 4x4 product grid — the rank grid's own axis
// conventions (zenith inclusive endpoints, azimuth/roll periodic midpoints), canonical free
// order. Only the chain gates' valid points enter; an invalid probe is data, skipped exactly
// like the rank scan skips its own samples.
std::vector<BlurAnchor> ExtentAnchors(const analytic::FaceNormalTable& table, const int* slots, int slot_count,
                                      const analytic::RestrictedFamilySpec& spec, int generic_rank,
                                      const double sun_hat[3], double refractive_index) {
  std::vector<BlurAnchor> anchors;
  const int free_count = (spec.zenith.mode == analytic::FamilyDimensionMode::kFree) +
                         (spec.azimuth.mode == analytic::FamilyDimensionMode::kFree) +
                         (spec.roll.mode == analytic::FamilyDimensionMode::kFree);
  // A one-dimensional image is probed along its own curve: the single free dimension swept
  // over its midpoint axis, or — on a two-dimensional chart whose image is one-dimensional
  // (the pole degeneracy, a roll symmetry) — a SLICE, the first free dimension swept with the
  // second pinned, which traces the whole curve (the second dimension moves nothing). Only a
  // genuinely two-dimensional image takes the 4x4 product grid.
  const bool slice = free_count == 1 || generic_rank <= 1;
  const int per_axis = slice ? kFamilyExtentProbeCount : 4;
  const bool zenith_free = spec.zenith.mode == analytic::FamilyDimensionMode::kFree;
  for (int i = 0; i < per_axis; i++) {
    for (int j = 0; j < per_axis; j++) {
      // The canonical free order is (zenith, azimuth, roll) filtered; a v1 two-dimensional
      // family is {zenith, azimuth} or {azimuth, roll}, so canonical slot 0 takes the first
      // free dimension and slot 1 the second, whatever they are.
      // Canonical slot 0 takes the outer loop's index and slot 1 the inner's, so the
      // two-dimensional set is a product grid (zenith inclusive endpoints, azimuth/roll
      // periodic midpoints) whatever the free dimensions are.
      double params[2] = { 0.0, 0.0 };
      int slot = 0;
      const bool azimuth_free = spec.azimuth.mode == analytic::FamilyDimensionMode::kFree;
      const bool roll_free = spec.roll.mode == analytic::FamilyDimensionMode::kFree;
      const auto put_axis = [&](bool is_zenith) {
        const int idx = slot == 0 ? i : j;
        params[slot] = is_zenith ? 180.0 * idx / (per_axis - 1) : 360.0 * (idx + 0.5) / per_axis;
        slot++;
      };
      if (zenith_free) {
        put_axis(true);
      }
      if (azimuth_free) {
        put_axis(false);
      }
      if (roll_free) {
        put_axis(false);
      }
      if (slice) {
        params[1] = 0.0;  // the slice pins the second free dimension at its zero
        if (j != 0) {
          continue;  // the slice runs the outer loop only
        }
      }
      const analytic::FamilyPointRecord point =
          analytic::EvaluateFamilyPoint(table, slots, slot_count, spec, sun_hat, refractive_index, params);
      if (point.status != analytic::FamilyRankStatus::kOk || !point.valid) {
        continue;
      }
      BlurAnchor anchor;
      AnchorOfOutgoing(params, point.outgoing, &anchor);
      anchors.push_back(anchor);
    }
  }
  return anchors;
}

// The object's own angular scale for the two-dimensional domain: the maximum pairwise sky
// distance over the anchor images. A curve-like object (a collapse point, a fold arc, a
// one-dimensional image track) never takes this reading — its overall length is the wrong
// dimension against a transverse blur — it declares the cross-section floor instead.
double ExtentDeg(const std::vector<BlurAnchor>& anchors) {
  double extent = 0.0;
  for (size_t i = 0; i < anchors.size(); i++) {
    for (size_t j = i + 1; j < anchors.size(); j++) {
      extent = std::max(extent, AngleBetweenDeg(anchors[i].image_dir, anchors[j].image_dir));
    }
  }
  return extent;
}

// The blur of the frozen dimensions against one candidate: each anchor's image re-evaluated
// with one frozen dimension's center at +-width/2 — the frozen width times the map's local
// sensitivity, made operational — with the maximum image displacement the blur. A side that
// clamps into the dimension's legal range is probed one-sided and the clamp is recorded (never
// a silently skipped side, never a defaulted zero). A width of zero (the measure's own fixed
// dimensions) is narrow by construction: zero width, zero blur.
struct BlurReading {
  double blur_deg = 0.0;
  bool clamped = false;
  std::vector<FamilyBandVerdict> verdicts;
};

BlurReading MeasureBlur(const analytic::FaceNormalTable& table, const int* slots, int slot_count,
                        const analytic::RestrictedFamilySpec& spec, const std::vector<FamilyFrozenDim>& frozen,
                        const std::vector<BlurAnchor>& anchors, const double sun_hat[3], double refractive_index,
                        double scale_deg) {
  BlurReading out;
  for (const FamilyFrozenDim& dim : frozen) {
    FamilyBandVerdict verdict;
    verdict.dim = DimName(dim.dim);
    if (dim.width_deg <= 0.0) {
      verdict.verdict = "narrow";
      out.verdicts.push_back(std::move(verdict));
      continue;
    }
    const analytic::FamilyDimension name = dim.dim;
    double blur = 0.0;
    for (int sign : { 1, -1 }) {
      const double wanted = dim.center_deg + sign * 0.5 * dim.width_deg;
      double probed;
      if (name == analytic::FamilyDimension::kZenith) {
        probed = std::clamp(wanted, 0.0, 180.0);
        if (probed != wanted) {
          out.clamped = true;  // the pole clamps the gauss side: a one-sided probe, recorded
        }
        if (probed == dim.center_deg) {
          continue;  // the side collapsed onto the center (a gauss centred at a pole)
        }
      } else {
        // Azimuth and roll are periodic: the wrap is exact, never a clamp.
        probed = std::fmod(wanted, 360.0);
        if (probed < 0.0) {
          probed += 360.0;
        }
      }
      analytic::RestrictedFamilySpec shifted = spec;
      analytic::FamilyDimensionSpec* slot = name == analytic::FamilyDimension::kZenith ? &shifted.zenith :
                                            name == analytic::FamilyDimension::kRoll   ? &shifted.roll :
                                                                                         &shifted.azimuth;
      slot->center_deg = probed;
      for (const BlurAnchor& anchor : anchors) {
        const analytic::FamilyPointRecord point = analytic::EvaluateFamilyPoint(
            table, slots, slot_count, shifted, sun_hat, refractive_index, anchor.params_deg);
        if (point.status != analytic::FamilyRankStatus::kOk || !point.valid) {
          continue;
        }
        double image[3];
        for (int c = 0; c < 3; c++) {
          image[c] = -point.outgoing[c];
        }
        blur = std::max(blur, AngleBetweenDeg(image, anchor.image_dir));
      }
    }
    out.blur_deg = std::max(out.blur_deg, blur);
    ILOG_VERBOSE(GetGlobalLogger(), "[raypath survey] survival dim {} blur {:.4f} scale {:.4f}", DimName(dim.dim), blur,
                 scale_deg);
    verdict.verdict = blur <= scale_deg ? "narrow" : "wide";
    out.verdicts.push_back(std::move(verdict));
  }
  return out;
}

// ---- the candidate assembly --------------------------------------------------------------------------

// One proposed object: what the rank scan found, where it sits, and the anchors the filter
// probes from. The declared per-category scale readings (the design ruling on the plan's
// decision point 1): every curve-like object — a collapse point, a fold arc, a
// one-dimensional image track — declares the structural cross-section floor (the point is the
// zero-thickness limit of the curve, so one floor serves the family); only the
// two-dimensional bright domain declares its probe set's extent.
struct FamilyCandidate {
  FamilyImageKind kind = FamilyImageKind::kCollapsePoint;
  bool viable = true;
  bool has_position = false;
  double position_alt_deg = 0.0;
  double position_az_deg = 0.0;
  double scale_deg = 0.0;
  int sample_count = 0;
  int crossing_count = 0;
  std::vector<BlurAnchor> anchors;
  // The pose the rank record actually held (collapse components and fold representatives carry
  // one): retained for the document's warm_index under the candidate's leg name. Killed
  // candidates never contribute.
  bool has_pose = false;
  double pose[9] = {};
  const char* pose_source = "";
};

std::vector<FamilyCandidate> CandidatesOf(const analytic::FaceNormalTable& table, const int* slots, int slot_count,
                                          const analytic::RestrictedFamilySpec& spec,
                                          const analytic::RestrictedFamilyRank& rank, const double sun_hat[3],
                                          double refractive_index) {
  std::vector<FamilyCandidate> out;
  for (const analytic::CollapseRecord& collapse : rank.collapse_components) {
    FamilyCandidate candidate;
    candidate.kind = FamilyImageKind::kCollapsePoint;
    candidate.has_position = true;
    candidate.position_alt_deg = collapse.image_alt_deg;
    candidate.position_az_deg = collapse.image_az_deg;
    candidate.scale_deg = kSurvivalSpotFloorDeg;
    candidate.sample_count = collapse.sample_count;
    BlurAnchor anchor;
    AnchorOfOutgoing(collapse.params_deg, collapse.outgoing, &anchor);
    candidate.anchors.push_back(anchor);
    candidate.has_pose = true;
    for (int k = 0; k < 9; k++) {
      candidate.pose[k] = collapse.pose[k];
    }
    candidate.pose_source = "family_collapse_anchor";
    out.push_back(std::move(candidate));
  }
  if (rank.free_count == 2 && rank.generic_rank == 2) {
    // The fold locus is a rank-2 family's rank-drop CURVE; on a generic-rank-1 chart (the pole
    // degeneracy) the rank-drop set is the whole domain and the sign-flip scan is noise.
    for (const analytic::FoldComponentRecord& component : rank.fold_components) {
      if (component.first_crossing < 0 || component.representative < 0 ||
          component.first_crossing + component.crossing_count > static_cast<int>(rank.crossings.size())) {
        continue;
      }
      const analytic::FoldPointRecord& representative = rank.crossings[static_cast<size_t>(component.representative)];
      FamilyCandidate candidate;
      candidate.kind = FamilyImageKind::kCausticEdge;
      candidate.has_position = true;
      candidate.position_alt_deg = representative.image_alt_deg;
      candidate.position_az_deg = representative.image_az_deg;
      candidate.crossing_count = component.crossing_count;
      candidate.scale_deg = kSurvivalSpotFloorDeg;  // the cross-section floor, never the arc's length
      BlurAnchor anchor;
      AnchorOfOutgoing(representative.params_deg, representative.outgoing, &anchor);
      candidate.anchors.push_back(anchor);
      candidate.has_pose = true;
      for (int k = 0; k < 9; k++) {
        candidate.pose[k] = representative.pose[k];
      }
      candidate.pose_source = "family_fold_anchor";
      out.push_back(std::move(candidate));
    }
  }
  if (rank.generic_rank == 1 || rank.generic_rank == 2) {
    FamilyCandidate candidate;
    candidate.kind = rank.generic_rank == 1 ? FamilyImageKind::kRegularImage : FamilyImageKind::kBrightDomain;
    candidate.anchors = ExtentAnchors(table, slots, slot_count, spec, rank.generic_rank, sun_hat, refractive_index);
    candidate.sample_count = static_cast<int>(candidate.anchors.size());
    if (candidate.anchors.size() < 2) {
      candidate.viable = false;
    } else {
      candidate.has_position = true;
      analytic::ImageAltAzDeg(candidate.anchors.front().outgoing, &candidate.position_alt_deg,
                              &candidate.position_az_deg);  // the function negates: pass the raw outgoing
      // The one-dimensional track is curve-like: the floor, not its arc's extent — only the
      // two-dimensional domain's anchors measure an extent.
      candidate.scale_deg = rank.generic_rank == 1 ? kSurvivalSpotFloorDeg : ExtentDeg(candidate.anchors);
    }
    out.push_back(std::move(candidate));
  }
  return out;
}

FamilyKilledCandidate KilledRecord(const std::vector<int>& member, const FamilyCandidate& candidate,
                                   const BlurReading* blur, const char* reason) {
  FamilyKilledCandidate killed;
  killed.member = member;
  killed.kind = candidate.kind;
  killed.has_position = candidate.has_position;
  killed.position_altitude_deg = candidate.position_alt_deg;
  killed.position_azimuth_deg = candidate.position_az_deg;
  if (blur != nullptr) {
    killed.blur_measured = true;  // the fact the serializer reports: a no_valid_image kill has no numbers
    killed.blur_deg = blur->blur_deg;
    killed.scale_deg = candidate.scale_deg;
    killed.clamped = blur->clamped;
    killed.verdicts = blur->verdicts;
  }
  killed.reason = reason;
  return killed;
}

FamilySurvivor SurvivingFact(const std::vector<int>& member, const FamilyCandidate& candidate, const BlurReading& blur,
                             int level) {
  FamilySurvivor fact;
  fact.kind = candidate.kind;
  fact.member = member;
  fact.has_position = candidate.has_position;
  fact.position_altitude_deg = candidate.position_alt_deg;
  fact.position_azimuth_deg = candidate.position_az_deg;
  fact.level = level;
  fact.frozen = blur.verdicts;
  fact.blur_deg = blur.blur_deg;
  fact.scale_deg = candidate.scale_deg;
  fact.clamped = blur.clamped;
  fact.sample_count = candidate.sample_count;
  fact.crossing_count = candidate.crossing_count;
  return fact;
}

// A surviving candidate's held pose joins the document's warm_index under its leg name (killed
// candidates never contribute — the seed retention records what the assembly kept).
void CollectWarmSeed(FamilyStructureResult* out, const FamilyCandidate& candidate) {
  if (!candidate.has_pose) {
    return;
  }
  std::array<double, 9> seed{};
  for (int k = 0; k < 9; k++) {
    seed[static_cast<size_t>(k)] = candidate.pose[k];
  }
  out->seeds.push_back({ std::move(seed), candidate.pose_source });
}

}  // namespace

std::vector<FamilyFreezeCandidate> FreezeCandidates(const FamilyRoutingMeasure& measure) {
  std::vector<FamilyFreezeCandidate> out;
  const struct {
    analytic::FamilyDimension dim;
    const FamilyDimMeasure* measure;
  } dims[2] = { { analytic::FamilyDimension::kZenith, &measure.zenith },
                { analytic::FamilyDimension::kRoll, &measure.roll } };
  for (const auto& dim : dims) {
    if (dim.measure->kind != FamilyDimKind::kGauss) {
      continue;  // uniform-full never freezes (AC1); fixed is frozen from level 0 on
    }
    FamilyFreezeCandidate candidate;
    candidate.dim = dim.dim;
    candidate.center_deg = NormalizeCenter(dim.dim, dim.measure->center_deg);
    candidate.width_deg = dim.measure->width_deg;
    out.push_back(candidate);
  }
  std::stable_sort(out.begin(), out.end(), [](const FamilyFreezeCandidate& a, const FamilyFreezeCandidate& b) {
    if (a.width_deg != b.width_deg) {
      return a.width_deg < b.width_deg;  // tightest first
    }
    return a.dim == analytic::FamilyDimension::kZenith && b.dim == analytic::FamilyDimension::kRoll;
  });
  return out;
}

FamilyStructureResult EvaluateFamilyStructure(const FamilyStructureInput& input, const ClassSupport& support) {
  FamilyStructureResult result;
  double sun_hat[3];
  for (int c = 0; c < 3; c++) {
    sun_hat[c] = -input.incident[c];
  }
  // The member slots (the T1 arm's own resolution discipline: an unresolvable member is an
  // internal inconsistency, refused at arm level, never silently skipped).
  std::vector<std::vector<int>> member_slots;
  member_slots.reserve(input.members->size());
  for (const std::vector<int>& member : *input.members) {
    std::vector<int> slots;
    if (const Error slot_error = ResolveSingleLayerPath({ member }, *input.table, &slots); !slot_error.Ok()) {
      result.refusal_mode = "member_resolution";
      return result;
    }
    member_slots.push_back(std::move(slots));
  }

  if (input.single_pose) {
    // The 0D arm: the all-fixed axis. The image is EvaluatePath at the one pose (the production
    // Euler chart), per member — the members are relabelings, each with its own image at the
    // shared pose. Every dimension is the measure's own fixed freeze (width 0, blur 0), so the
    // survival question is degenerate by construction and the level carries the cards.
    FamilyLadderLevel level;
    level.level = 0;
    level.frozen_dims = FrozenReport(input.measure, {}, 0);
    double* pose = result.single_pose.data();
    analytic::EulerPoseDeg(NormalizeCenter(analytic::FamilyDimension::kAzimuth, input.measure.azimuth.center_deg),
                           NormalizeCenter(analytic::FamilyDimension::kZenith, input.measure.zenith.center_deg),
                           NormalizeCenter(analytic::FamilyDimension::kRoll, input.measure.roll.center_deg), pose);
    size_t member_index = 0;
    for (const std::vector<int>& member : *input.members) {
      const std::vector<int>& slots = member_slots[member_index++];
      FamilyCandidate candidate;
      candidate.kind = FamilyImageKind::kCollapsePoint;
      candidate.scale_deg = kSurvivalSpotFloorDeg;
      candidate.sample_count = 1;
      std::vector<double> segments(3 * (slots.size() + 1), 0.0);
      std::vector<double> interfaces(slots.size(), 0.0);
      analytic::PathOutputs outputs;
      outputs.segment_directions = segments.data();
      outputs.interface_transmittances = interfaces.data();
      const bool valid = analytic::EvaluatePath(*input.table, slots.data(), static_cast<int>(slots.size()),
                                                input.refractive_index, input.incident, pose, &outputs);
      level.objects_proposed += 1;
      if (!valid) {
        level.objects_killed += 1;
        level.killed.push_back(KilledRecord(member, candidate, nullptr, "no_valid_image"));
        continue;
      }
      candidate.has_position = true;
      analytic::ImageAltAzDeg(outputs.outgoing_direction, &candidate.position_alt_deg,
                              &candidate.position_az_deg);  // the function negates: the image is -outgoing
      BlurAnchor anchor;
      DirectionOf(candidate.position_alt_deg, candidate.position_az_deg, anchor.image_dir);
      candidate.anchors.push_back(anchor);
      candidate.has_pose = true;  // the 0D arm's one held pose (the production Euler chart's)
      for (int k = 0; k < 9; k++) {
        candidate.pose[k] = pose[k];
      }
      candidate.pose_source = "single_pose_nominal";
      FamilySurvivor fact = SurvivingFact(member, candidate, BlurReading{}, 0);
      for (const FamilyFrozenDim& dim : level.frozen_dims) {
        fact.frozen.push_back({ DimName(dim.dim), "narrow" });
      }
      CollectWarmSeed(&result, candidate);
      result.survivors.push_back(std::move(fact));
    }
    result.ladder.push_back(std::move(level));

    return result;
  }

  // The ladder proper. Level 0 is the actual support (already assembled above); it asks the
  // rank question only when the measure's fixed dimensions leave at most two free. Levels
  // 1..K freeze the k tightest candidates and always ask. Every level reports its proposals —
  // survivors as band-labelled cards, the killed in the level's own record.
  const std::vector<FamilyFreezeCandidate> candidates = FreezeCandidates(input.measure);
  const int level_count = static_cast<int>(candidates.size()) + 1;
  bool& rank_ran = result.rank_ran;  // any level's rank question answered on any member (the note's honesty)
  for (int k = 0; k < level_count; k++) {
    FamilyLadderLevel level;
    level.level = k;
    level.frozen_dims = FrozenReport(input.measure, candidates, k);
    const IdealFamilyScope ideal = LevelSpec(input.measure, candidates, k);
    const analytic::RestrictedFamilySpec& spec = ideal.spec;
    if (spec.zenith.mode == analytic::FamilyDimensionMode::kFree) {
      level.free_dims.push_back(analytic::FamilyDimension::kZenith);
    }
    if (spec.azimuth.mode == analytic::FamilyDimensionMode::kFree) {
      level.free_dims.push_back(analytic::FamilyDimension::kAzimuth);
    }
    if (spec.roll.mode == analytic::FamilyDimensionMode::kFree) {
      level.free_dims.push_back(analytic::FamilyDimension::kRoll);
    }
    level.rank_asked = static_cast<int>(level.free_dims.size()) <= 2;
    if (level.rank_asked) {
      level.grid = kFamilyLadderGrid;
      size_t member_index = 0;
      bool coverage_recorded = false;
      for (const std::vector<int>& member : *input.members) {
        const std::vector<int>& slots = member_slots[member_index++];
        const analytic::RestrictedFamilyRank rank =
            analytic::RankRestrictedFamily(*input.table, slots.data(), static_cast<int>(slots.size()), spec, sun_hat,
                                           input.refractive_index, kFamilyLadderGrid);
        if (rank.status != analytic::FamilyRankStatus::kOk) {
          // The gate guarantees this cannot happen (azimuth free, one or two free dimensions,
          // the declared grid); it is recorded as data rather than guessed into a structure.
          level.rank_refused_members += 1;
          ILOG_WARN(GetGlobalLogger(), "[raypath survey] family ladder level {} refused: {}", k,
                    analytic::FamilyRankStatusName(rank.status));
          continue;
        }
        if (!coverage_recorded) {
          // The rank coverage echoes the first member (relabelings share the rank structure —
          // the same physical-class fact the support's SameSupport rests on).
          rank_ran = true;
          level.generic_rank = rank.generic_rank;
          level.samples = rank.samples;
          level.valid_samples = rank.valid_samples;
          level.grid_spacing_deg[0] = rank.grid_spacing_deg[0];
          level.grid_spacing_deg[1] = rank.grid_spacing_deg[1];
          coverage_recorded = true;
        }
        for (FamilyCandidate& candidate : CandidatesOf(*input.table, slots.data(), static_cast<int>(slots.size()), spec,
                                                       rank, sun_hat, input.refractive_index)) {
          level.objects_proposed += 1;
          if (!candidate.viable) {
            level.objects_killed += 1;
            level.killed.push_back(KilledRecord(member, candidate, nullptr, "no_valid_image"));
            continue;
          }
          const BlurReading blur =
              MeasureBlur(*input.table, slots.data(), static_cast<int>(slots.size()), spec, level.frozen_dims,
                          candidate.anchors, sun_hat, input.refractive_index, candidate.scale_deg);
          if (blur.blur_deg <= candidate.scale_deg) {
            FamilySurvivor fact = SurvivingFact(member, candidate, blur, k);
            if (fact.kind == FamilyImageKind::kBrightDomain && support.ok && !support.intervals.empty()) {
              fact.has_domain = true;
              fact.domain_lower_deg = support.intervals.front().lower * kRadToDeg;
              fact.domain_upper_deg = support.intervals.back().upper * kRadToDeg;
            }
            CollectWarmSeed(&result, candidate);
            result.survivors.push_back(std::move(fact));
          } else {
            level.objects_killed += 1;
            level.killed.push_back(KilledRecord(member, candidate, &blur, "survival_filter"));
          }
        }
      }
    }
    result.ladder.push_back(std::move(level));
  }

  return result;
}

}  // namespace lumice::raypath::structure
