#include "raypath/detail/survey/survey_family.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"  // math::kDegreeToRadD — the unit conversions' single authority
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/detail/survey/survey_t1.hpp"
#include "raypath/detail/survey/survey_tint.hpp"
#include "raypath/detail/survey/survey_witness.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr double kDegToRad = math::kDegreeToRadD;
constexpr double kRadToDeg = math::kRadToDegreeD;

// The out-of-vocabulary distribution types' own names (the refusal modes name them).
const char* DistributionTypeSlug(DistributionType type) {
  switch (type) {
    case DistributionType::kZigzag:
      return "zigzag";
    case DistributionType::kLaplacian:
      return "laplacian";
    case DistributionType::kGaussianLegacy:
      return "gaussian_legacy";
    case DistributionType::kNoRandom:
    case DistributionType::kUniform:
    case DistributionType::kGaussian:
      return "uniform";
  }
  return "uniform";
}

// ---- the measure extraction -------------------------------------------------------------------------

// The routing-vocabulary class of one distribution slot. The two default semantics are the
// config loader's own facts, read not re-derived: kNoRandom is the fixed value, a full-turn
// kUniform (detail::IsFullTurnUniform — the same predicate the axis's own symmetry members
// read) is the free uniform. A zero-sigma gauss is read as fixed (the degenerate limit of the
// concentration language; the density side refuses it independently).
enum class DimClass { kFixed, kGauss, kUniformFull, kUniformBounded, kOutOfVocabulary };

DimClass ClassifyDim(const Distribution& dist) {
  switch (dist.type) {
    case DistributionType::kNoRandom:
      return DimClass::kFixed;
    case DistributionType::kGaussian:
      return dist.spread > 0.0f ? DimClass::kGauss : DimClass::kFixed;
    case DistributionType::kUniform:
      return detail::IsFullTurnUniform(dist.type, dist.spread) ? DimClass::kUniformFull : DimClass::kUniformBounded;
    case DistributionType::kZigzag:
    case DistributionType::kLaplacian:
    case DistributionType::kGaussianLegacy:
      return DimClass::kOutOfVocabulary;
  }
  return DimClass::kOutOfVocabulary;
}

FamilyDimMeasure MeasureOf(DimClass cls, const Distribution& dist, bool latitude_slot) {
  FamilyDimMeasure out;
  const double center = latitude_slot ? 90.0 - static_cast<double>(dist.center) : static_cast<double>(dist.center);
  switch (cls) {
    case DimClass::kFixed:
      out.kind = FamilyDimKind::kFixed;
      out.center_deg = center;
      out.width_deg = 0.0;
      break;
    case DimClass::kGauss:
      out.kind = FamilyDimKind::kGauss;
      out.center_deg = center;
      out.width_deg = static_cast<double>(dist.spread);
      break;
    case DimClass::kUniformFull:
    case DimClass::kUniformBounded:
    case DimClass::kOutOfVocabulary:
      out.kind = FamilyDimKind::kUniformFull;
      out.center_deg = center;                           // the raw midpoint echo
      out.width_deg = static_cast<double>(dist.spread);  // the raw range, display only
      break;
  }
  return out;
}

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
analytic::RestrictedFamilySpec LevelSpec(const FamilyRoutingMeasure& measure,
                                         const std::vector<FamilyFreezeCandidate>& candidates, int k) {
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
  return spec;
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
  SurveyObjectKind kind = SurveyObjectKind::kFamilyCollapsePoint;
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
    candidate.kind = SurveyObjectKind::kFamilyCollapsePoint;
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
      candidate.kind = SurveyObjectKind::kCausticEdge;
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
    candidate.kind = rank.generic_rank == 1 ? SurveyObjectKind::kRegularImage : SurveyObjectKind::kBrightDomain;
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
  killed.kind = SurveyObjectKindName(candidate.kind);
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

SurveyObject SurvivingCard(const std::vector<int>& member, const FamilyCandidate& candidate, const BlurReading& blur,
                           int level, double wavelength_nm) {
  SurveyObject card;
  card.kind = candidate.kind;
  card.members = { member };
  card.has_position = candidate.has_position;
  card.position_altitude_deg = candidate.position_alt_deg;
  card.position_azimuth_deg = candidate.position_az_deg;
  card.evidence.state = EvidenceState::kFinite;
  card.evidence.coverage.members_evaluated = 1;
  card.evidence.coverage.wavelength_nm = wavelength_nm;
  SurveyObject::BandPayload band;
  band.level = level;
  band.frozen = blur.verdicts;
  band.blur_deg = blur.blur_deg;
  band.scale_deg = candidate.scale_deg;
  band.clamped = blur.clamped;
  card.band = std::move(band);
  SurveyObject::FamilyOriginPayload origin;
  origin.sample_count = candidate.sample_count;
  origin.crossing_count = candidate.crossing_count;
  card.family_origin = origin;
  return card;
}

// The document-level escape rule's authority: the arm's document carries a finite structural
// reading iff some card says so itself. The witness is a payload on its card with its own
// state — a refused card carrying a finite witness is still not a finite structural object.
bool AnyFiniteObject(const SurveyDocument& document) {
  for (const SurveyObject& card : document.objects) {
    if (card.evidence.state == EvidenceState::kFinite) {
      return true;
    }
  }
  return false;
}

// A surviving candidate's held pose joins the document's warm_index under its leg name (killed
// candidates never contribute — the seed retention records what the assembly kept).
void CollectWarmSeed(SurveyDocument* out, const FamilyCandidate& candidate) {
  if (!candidate.has_pose) {
    return;
  }
  std::array<double, 9> seed{};
  for (int k = 0; k < 9; k++) {
    seed[static_cast<size_t>(k)] = candidate.pose[k];
  }
  out->warm_index.seeds.push_back(std::move(seed));
  out->warm_index.seed_sources.emplace_back(candidate.pose_source);
}

}  // namespace

// ---- the public entries -------------------------------------------------------------------------------

FamilyMeasureReading ReadFamilyMeasure(const AxisDistribution& axis) {
  FamilyMeasureReading out;
  const Distribution* dims[3] = { &axis.latitude_dist, &axis.azimuth_dist, &axis.roll_dist };
  const char* names[3] = { "zenith", "azimuth", "roll" };
  DimClass classes[3] = {};
  for (int i = 0; i < 3; i++) {
    classes[i] = ClassifyDim(*dims[i]);
  }

  // The all-fixed axis is the single pose — the one legal shape with a frozen azimuth (the 0D
  // arm; the ladder never freezes azimuth, the rank entry's kRefusedAzimuthFrozen semantics).
  out.single_pose = classes[0] == DimClass::kFixed && classes[1] == DimClass::kFixed && classes[2] == DimClass::kFixed;

  // The v1 domain gate (AC3): azimuth full-uniform, zenith {fixed, gauss}, roll
  // {fixed, gauss, uniform-full}; the first offender in canonical order is named, and nothing
  // is approximated into the domain (a bounded uniform is never swapped for a matching gauss).
  for (int i = 0; i < 3; i++) {
    std::string shape;
    if (classes[i] == DimClass::kOutOfVocabulary) {
      shape = std::string(names[i]) + "_" + DistributionTypeSlug(dims[i]->type);
    } else if (i == 0 /* zenith */) {
      if (classes[i] == DimClass::kUniformBounded) {
        shape = "zenith_bounded_uniform";
      } else if (classes[i] == DimClass::kUniformFull) {
        // A Haar axis routed to the full-dimensional arm before this gate; any other
        // full-uniform zenith has no kernel family to carry it.
        shape = "zenith_full_uniform";
      }
    } else if (i == 1 /* azimuth */) {
      if (out.single_pose) {
        continue;
      }
      if (classes[i] == DimClass::kFixed) {
        shape = "azimuth_fixed";
      } else if (classes[i] == DimClass::kGauss) {
        shape = "azimuth_gaussian";
      } else if (classes[i] == DimClass::kUniformBounded) {
        shape = "azimuth_bounded_uniform";
      }
    } else /* roll */ {
      if (classes[i] == DimClass::kUniformBounded) {
        shape = "roll_bounded_uniform";
      }
    }
    if (!shape.empty()) {
      out.refusal_mode = std::string(kMeasureOutsideDomainPrefix) + shape;
      return out;
    }
  }

  out.measure.zenith = MeasureOf(classes[0], *dims[0], true);
  out.measure.azimuth = MeasureOf(classes[1], *dims[1], false);
  out.measure.roll = MeasureOf(classes[2], *dims[2], false);
  out.ok = true;
  return out;
}

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

FamilyArmOutcome AssembleFamilyRouting(const FamilyArmInput& input, SurveyDocument* out) {
  FamilyRoutingBlock& block = out->family_routing.emplace();
  block.measure = input.measure;
  double sun_hat[3];
  for (int c = 0; c < 3; c++) {
    sun_hat[c] = -input.incident[c];  // family_rank.hpp's convention: sun_hat points AT the sun
  }
  const int member_count = static_cast<int>(input.members->size());

  // Level 0's support: the delta machine's own (orientation-distribution independent), with the
  // density-support tail rows the measure's gauss zenith contributes.
  const ClassSupport support = AssembleClassSupport(*input.table, *input.polygons, *input.members,
                                                    input.refractive_index, input.wavelength_nm, out);
  // The partition refusal's scope is the level-0 card itself: the delta partition is a geometry
  // fact about the support, the orientation ladder below a measure fact that runs regardless
  // (its only partition read is bright_domain's domain bounds, gated at the card). The refused
  // card keeps the witness leg's positive fact — the support is unknown, not zero, and never a
  // fabricated union.
  std::string level0_refusal_mode;
  if (!support.ok) {
    level0_refusal_mode = support.refusal_mode;
    block.level0_refusal_mode = support.refusal_mode;
    WitnessInput witness_input;
    witness_input.table = input.table;
    witness_input.polygons = input.polygons;
    witness_input.crystal = input.crystal;
    witness_input.representative = input.representative;
    for (int c = 0; c < 3; c++) {
      witness_input.incident[c] = input.incident[c];
    }
    witness_input.refractive_index = input.refractive_index;
    witness_input.wavelength_nm = input.wavelength_nm;
    witness_input.member_count = member_count;
    // Slice A's single refused-card entry, carrying the gate-order ruling's own scope: the
    // card joins the ladder's cards (clear_objects=false), the chromatic/tint stamp below is
    // overwritten whenever the density-expressible legs run, and it survives only in the
    // refused x density-invalid corner, where its reason names the density cause.
    const std::string corner_chromatic_reason = "chromatic legs not run: the axis maps to no pose density (" +
                                                input.density->unsupported +
                                                "); the legs follow objects, not the partition";
    const std::string corner_budget_note =
        "the family arm's legs are the T0 routing verdict, the per-member delta partition "
        "attempt (refused: " +
        support.refusal_mode +
        "), the ladder's per-level rank scans "
        "and the " +
        std::to_string(kSurveyWitnessSampleCount) +
        "-sample witness lattice on the refused level-0 card; none is budget-enforced yet; "
        "enforcement lands with the quantification legs";
    AssembleRefusedSupport(witness_input, *input.members, support.refusal_mode, corner_chromatic_reason,
                           corner_budget_note, RefusedSupportObjects::kAppendToObjects, out);
  }
  if (support.ok && input.density->Ok() && input.measure.zenith.kind == FamilyDimKind::kGauss) {
    // AC5's boundedness-role separation: the width went into the ladder; the support's own tail
    // bound (the band sum's +-12 sigma window floor) is a row on the support card.
    const analytic::PoseDensity density(input.density->spec);
    double lower = 0.0;
    double upper = 0.0;
    density.ZenithWindow(&lower, &upper);
    FamilySupportTail tail;
    tail.dim = "zenith";
    tail.lower_deg = lower * kRadToDeg;
    tail.upper_deg = upper * kRadToDeg;
    out->objects.back().support_set->density_tails.push_back(std::move(tail));
  }

  // The member slots (the T1 arm's own resolution discipline: an unresolvable member is an
  // internal inconsistency, refused at arm level, never silently skipped).
  std::vector<std::vector<int>> member_slots;
  member_slots.reserve(input.members->size());
  for (const std::vector<int>& member : *input.members) {
    std::vector<int> slots;
    if (const Error slot_error = ResolveSingleLayerPath({ member }, *input.table, &slots); !slot_error.Ok()) {
      return { false, "member_resolution" };
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
    double pose[9];
    analytic::EulerPoseDeg(NormalizeCenter(analytic::FamilyDimension::kAzimuth, input.measure.azimuth.center_deg),
                           NormalizeCenter(analytic::FamilyDimension::kZenith, input.measure.zenith.center_deg),
                           NormalizeCenter(analytic::FamilyDimension::kRoll, input.measure.roll.center_deg), pose);
    size_t member_index = 0;
    for (const std::vector<int>& member : *input.members) {
      const std::vector<int>& slots = member_slots[member_index++];
      FamilyCandidate candidate;
      candidate.kind = SurveyObjectKind::kFamilyCollapsePoint;
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
      SurveyObject card = SurvivingCard(member, candidate, BlurReading{}, 0, input.wavelength_nm);
      card.band->frozen.clear();
      for (const FamilyFrozenDim& dim : level.frozen_dims) {
        card.band->frozen.push_back({ DimName(dim.dim), "narrow" });
      }
      CollectWarmSeed(out, candidate);
      out->objects.push_back(std::move(card));
    }
    block.ladder.push_back(std::move(level));

    // The 0D weight tint: the single-pose direct read, never a band sum (the design's own
    // sentence) — the tint module's own entry. The positional chromatic legs say they have no
    // structure to follow.
    SinglePoseTintInput tint_input;
    tint_input.crystal = input.crystal;
    tint_input.representative = input.representative;
    for (int c = 0; c < 9; c++) {
      tint_input.pose[c] = pose[c];
    }
    for (int c = 0; c < 3; c++) {
      tint_input.incident[c] = input.incident[c];
    }
    tint_input.member_count = member_count;
    AssembleSinglePoseTint(tint_input, out);
    const std::string positional_reason =
        "positional chromatic legs not run: a single pose has no positional structure to follow";
    out->positional_dispersion = SurveyPositionalDispersion{};
    out->positional_dispersion->evidence.state = EvidenceState::kUnevaluated;
    out->positional_dispersion->evidence.reason = positional_reason;
    out->diagnose = SurveyDiagnoseBlock{};
    out->diagnose->evidence.state = EvidenceState::kUnevaluated;
    out->diagnose->evidence.reason = positional_reason;
    block.evidence.state = EvidenceState::kFinite;
    block.evidence.coverage.members_evaluated = member_count;
    block.evidence.coverage.geometry_basis =
        level0_refusal_mode.empty() ?
            std::string("the single pose's direct image evaluation and the delta partition") :
            "the single pose's direct image evaluation (the level-0 delta partition refused: " + level0_refusal_mode +
                ")";
    block.evidence.coverage.wavelength_nm = input.wavelength_nm;
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the 0D family arm's legs are the T0 routing verdict, the per-member delta partition, "
          "the single-pose image evaluation and the single-pose weight at the " +
          ChromaticPairProseRedFirst() +
          " pair; none is budget-enforced yet; enforcement lands with the quantification legs";
    } else {
      out->budget_note =
          "the 0D family arm's legs are the T0 routing verdict, the per-member delta partition "
          "attempt (refused: " +
          level0_refusal_mode + "), the " + std::to_string(kSurveyWitnessSampleCount) +
          "-sample witness lattice on the refused level-0 card, the single-pose image evaluation "
          "and the single-pose weight at the " +
          ChromaticPairProseRedFirst() +
          " pair; none is budget-enforced yet; enforcement lands with the quantification legs";
    }
    return { AnyFiniteObject(*out), level0_refusal_mode };
  }

  // The ladder proper. Level 0 is the actual support (already assembled above); it asks the
  // rank question only when the measure's fixed dimensions leave at most two free. Levels
  // 1..K freeze the k tightest candidates and always ask. Every level reports its proposals —
  // survivors as band-labelled cards, the killed in the level's own record.
  const std::vector<FamilyFreezeCandidate> candidates = FreezeCandidates(input.measure);
  const int level_count = static_cast<int>(candidates.size()) + 1;
  bool rank_ran = false;  // any level's rank question answered on any member (the note's honesty)
  for (int k = 0; k < level_count; k++) {
    FamilyLadderLevel level;
    level.level = k;
    level.frozen_dims = FrozenReport(input.measure, candidates, k);
    const analytic::RestrictedFamilySpec spec = LevelSpec(input.measure, candidates, k);
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
            SurveyObject card = SurvivingCard(member, candidate, blur, k, input.wavelength_nm);
            if (card.kind == SurveyObjectKind::kBrightDomain && support.ok && !support.intervals.empty()) {
              // The bright_domain skeleton (the probe leg fills the profile readings): the
              // domain span is the class support's own — the same delta axis the level-0 card
              // declared. support.ok is the trust gate, not emptiness: the
              // member_supports_disagree refusal fills intervals (the front member's own)
              // before refusing. A degenerate empty support carries no span; the card survives
              // without one.
              SurveyObject::BrightDomainPayload domain;
              domain.domain_lower_deg = support.intervals.front().lower * kRadToDeg;
              domain.domain_upper_deg = support.intervals.back().upper * kRadToDeg;
              card.bright_domain = std::move(domain);
            }
            CollectWarmSeed(out, candidate);
            out->objects.push_back(std::move(card));
          } else {
            level.objects_killed += 1;
            level.killed.push_back(KilledRecord(member, candidate, &blur, "survival_filter"));
          }
        }
      }
    }
    block.ladder.push_back(std::move(level));
  }

  // The weight legs: the three kernel shapes the density conversion carries run the probe, the
  // chromatic pair and the class tint (the probe on the representative, its readings landing on
  // the representative's own bright_domain card); an axis the conversion cannot carry keeps
  // every leg's named refusal — a fixed zenith or roll is legal structure with no density, and
  // the legs say so instead of silently skipping. Context-only (the query face's no-warm mode)
  // keeps the structure alone: every T3 leg is skipped, the budget note says so.
  if (input.context_only) {
    out->budget_note =
        "the context-only assembly's legs are the T0 routing verdict and the T0-T2 structure legs "
        "(the delta partition and the freezing ladder); the T3 quantification legs (the probe, "
        "the chromatic pair, the tint, the witness lattice) are skipped";
  } else if (input.density->Ok()) {
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition, the "
          "ladder's per-member per-level rank scans (" +
          std::to_string(kFamilyLadderGrid) + " samples per free dimension), the band-sum probe, the " +
          "chromatic pair and the class tint's two-lambda power sum (" + std::to_string(kSurveyClassTintSampleCount) +
          " samples per lambda); none is budget-enforced yet; enforcement lands with the "
          "quantification legs";
    } else {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition "
          "attempt (refused: " +
          level0_refusal_mode + "), the ladder's per-member per-level rank scans (" +
          std::to_string(kFamilyLadderGrid) + " samples per free dimension), the " +
          std::to_string(kSurveyWitnessSampleCount) +
          "-sample witness lattice on the refused level-0 card, the band-sum probe, the chromatic "
          "pair and the class tint's two-lambda power sum (" +
          std::to_string(kSurveyClassTintSampleCount) +
          " samples per lambda); none is budget-enforced yet; enforcement lands with the "
          "quantification legs";
    }
    ProbeInput probe_input;
    probe_input.table = input.table;
    probe_input.polygons = input.polygons;
    probe_input.representative = input.representative;
    probe_input.density = input.density;
    probe_input.refractive_index = input.refractive_index;
    probe_input.wavelength_nm = input.wavelength_nm;
    for (int c = 0; c < 3; c++) {
      probe_input.incident[c] = input.incident[c];
    }
    probe_input.member_count = member_count;
    probe_input.fill_member = input.representative;
    RunSolarRadialProbe(probe_input, out);
    AssembleChromaticLayer(*input.table, *input.polygons, *input.representative, input.density->spec, member_count,
                           out);
    ClassTintInput class_tint_input;
    class_tint_input.table = input.table;
    class_tint_input.polygons = input.polygons;
    class_tint_input.representative = input.representative;
    for (int c = 0; c < 3; c++) {
      class_tint_input.incident[c] = input.incident[c];
    }
    class_tint_input.member_count = member_count;
    AssembleClassTint(class_tint_input, out);
  } else {
    const std::string density_reason = "no quantification legs ran: the axis maps to no pose density (" +
                                       input.density->unsupported +
                                       "), and the probe/chromatic/tint legs sit downstream of the density conversion";
    out->chromatic.state = EvidenceState::kUnevaluated;
    out->chromatic.reason = density_reason;
    out->weight_tint = SurveyWeightTint{};
    out->weight_tint->evidence.state = EvidenceState::kUnevaluated;
    out->weight_tint->evidence.reason = density_reason;
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition and "
          "the ladder's per-member per-level rank scans (" +
          std::to_string(kFamilyLadderGrid) +
          " samples per free dimension); no quantification leg ran (the axis maps to no pose "
          "density); none is budget-enforced yet; enforcement lands with the quantification legs";
    } else {
      out->budget_note = std::string(
                             "the family arm's legs are the T0 routing verdict, the per-member delta "
                             "partition attempt (refused: ") +
                         level0_refusal_mode + "), " +
                         (rank_ran ? "the ladder's per-member per-level rank scans (" +
                                         std::to_string(kFamilyLadderGrid) + " samples per free dimension)" :
                                     "the ladder's level enumeration (no level got a rank answer)") +
                         ", and the " + std::to_string(kSurveyWitnessSampleCount) +
                         "-sample witness lattice on the refused level-0 card; no quantification leg ran (the "
                         "axis maps to no pose density); none is budget-enforced yet; enforcement lands with "
                         "the quantification legs";
    }
  }
  // The block's state reads the legs' own outputs: finite once any leg produced one. The one
  // non-finite corner (the gate-order ruling's own definition): a refused level-0 partition AND
  // a density the conversion cannot carry — every quantification leg silent (the ladder's rank
  // scans are density-independent geometry and may still have produced readings), and the block
  // says so instead of claiming a reading nothing produced.
  const bool legs_all_silent = out->chromatic.state == EvidenceState::kUnevaluated &&
                               out->weight_tint->evidence.state == EvidenceState::kUnevaluated;
  if (!level0_refusal_mode.empty() && legs_all_silent) {
    block.evidence.state = EvidenceState::kUnevaluated;
    block.evidence.reason =
        "the level-0 delta partition refused (" + level0_refusal_mode + ") and the axis maps to no pose density (" +
        input.density->unsupported +
        "): " + (rank_ran ? "no quantification leg produced a reading" : "no family-routing leg produced a reading");
  } else {
    block.evidence.state = EvidenceState::kFinite;
    block.evidence.coverage.members_evaluated = member_count;
    block.evidence.coverage.geometry_basis =
        (!level0_refusal_mode.empty() && !rank_ran) ?
            "the ladder's level enumeration on the nominal crystal shape (no level got a rank answer)" :
            "the per-member restricted-family rank scans at " + std::to_string(kFamilyLadderGrid) +
                " samples per free dimension on the nominal crystal shape";
    block.evidence.coverage.wavelength_nm = input.wavelength_nm;
  }
  ILOG_INFO(GetGlobalLogger(), "[raypath survey] family routing: {} ladder level(s), {} object card(s)",
            block.ladder.size(), out->objects.size());
  return { AnyFiniteObject(*out), level0_refusal_mode };
}

}  // namespace lumice::raypath::survey
