#include "raypath/detail/schema3/mc_attribution.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/so3.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath::schema3 {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

constexpr double kPi = 3.14159265358979323846;

// The match distance on the deviation axis: the smallest gap between the record's sky-point
// deviations and the object's image values. Both sides are delta-axis sets — the declared base
// ruler (azimuth not discriminated; the limitation rides the ruler strings).
double BaseDistance(const std::vector<double>& record_delta, const std::vector<double>& image_delta) {
  double best = kInf;
  for (const double r : record_delta) {
    for (const double o : image_delta) {
      best = std::min(best, std::fabs(r - o));
    }
  }
  return best;
}

double AngleBetween(const double a[3], const double b[3]) {
  double cross[3];
  analytic::so3::Cross3(a, b, cross);
  return std::atan2(analytic::so3::Norm3(cross), analytic::so3::Dot3(a, b));
}

// The enhanced orbit-image layer's distance: the smallest all-sky angle between the record's
// sky points and the member stream's VALID outgoing directions (world frame; invalid points
// carry a zeroed outgoing — the "no direction" spelling). Restricted objects of a member with
// a non-empty stream only; kInf otherwise (the layer then adds nothing).
double OrbitImageDistance(const StructureObjectRecord& object, const Schema3DiscoveryCore& core,
                          const std::vector<std::array<double, 3>>& sky_points, long long* orbit_index) {
  const long long member = MemberIndexOf(core, object.member);
  if (member < 0 || member >= static_cast<long long>(core.orbits.size())) {
    return kInf;
  }
  const analytic::OrbitFiberStream& stream = core.orbits[member];
  if (object.kind != ObjectKind::kKind1Restricted || stream.samples.empty()) {
    return kInf;
  }
  if (orbit_index != nullptr) {
    *orbit_index = member;
  }
  double best = kInf;
  for (const analytic::OrbitFiberPoint& point : stream.samples) {
    if (point.outgoing[0] == 0.0 && point.outgoing[1] == 0.0 && point.outgoing[2] == 0.0) {
      continue;
    }
    // The stream point's outgoing is a PROPAGATION direction; the record's sky points are
    // viewing directions (negative propagation) — the object's displayed point is -outgoing.
    const double displayed[3] = { -point.outgoing[0], -point.outgoing[1], -point.outgoing[2] };
    for (const std::array<double, 3>& sky : sky_points) {
      best = std::min(best, AngleBetween(displayed, sky.data()));
    }
  }
  return best;
}

// The per-record facts the two directions share: the kActual records' sky-point deviations and
// their own significance standing (plan A5: only kActual has witnessing qualification).
struct McRecordFacts {
  size_t index = 0;
  std::vector<double> delta_rad;
  std::vector<std::array<double, 3>> sky_points;
  double ess = 0.0;
  bool has_position = false;
};

std::vector<McRecordFacts> ActualRecordFacts(const McEvidenceBlock& mc, const McAttributionContext& geometry) {
  std::vector<McRecordFacts> facts;
  for (size_t i = 0; i < mc.discovery.features.size(); i++) {
    const DiagnosticFeatureRecord& record = mc.discovery.features[i];
    if (record.evidence != DiagnosticEvidence::kActual) {
      continue;
    }
    McRecordFacts one;
    one.index = i;
    one.ess = record.minimum_effective_samples;
    one.sky_points = record.sky_points;
    one.has_position = !record.sky_points.empty();
    for (const std::array<double, 3>& sky : record.sky_points) {
      one.delta_rad.push_back(AngleBetween(geometry.sun_dir, sky.data()));
    }
    facts.push_back(std::move(one));
  }
  return facts;
}

// The object's best match under the declared two-layer ruler: the min of the base delta-axis
// distance and (restricted, streamed members) the orbit-image distance. `orbit_used` reports
// whether the enhanced layer actually fired (the ruler string's input).
struct ObjectMatch {
  double distance = kInf;
  long long record_index = -1;
  double record_ess = std::nan("");
  bool orbit_used = false;
};

ObjectMatch MatchObject(const StructureObjectRecord& object, const ObjectDeltaImage& image,
                        const std::vector<McRecordFacts>& records, const Schema3DiscoveryCore& core,
                        const McAttributionInput& input) {
  ObjectMatch best;
  const double tolerance = std::max(input.h_rad, DeclaredWidthOf(object));
  for (const McRecordFacts& record : records) {
    double distance = BaseDistance(record.delta_rad, image.delta_rad);
    bool orbit_used = false;
    if (record.has_position) {
      long long orbit_index = -1;
      const double orbit = OrbitImageDistance(object, core, record.sky_points, &orbit_index);
      if (orbit < distance) {
        distance = orbit;
        orbit_used = true;
      }
    }
    if (distance < best.distance) {
      best.distance = distance;
      best.record_index = static_cast<long long>(record.index);
      best.record_ess = record.ess;
      best.orbit_used = orbit_used;
    }
  }
  return best;
}

std::string RulerString(const McAttributionInput& input, bool orbit_used) {
  std::string ruler = "delta-axis base match, tolerance = max(h, declared width)";
  if (orbit_used) {
    ruler += "; orbit-image enhanced layer (all-sky)";
  }
  ruler += "; h = " + std::to_string(input.h_rad) + " rad, min_ess = " + std::to_string(input.thresholds.min_ess);
  ruler += "; azimuth not discriminated (the base ruler's declared limitation)";
  return ruler;
}

std::string PresenceReadingNote() {
  return "presence ESS weights the corroboration: it counts the MC's RECORDED observation "
         "(light-bearing rows, grouped by outer draw) in the window — the v2 measure drops "
         "zero-weight draws, so this is the standing of what the MC recorded, not of its raw "
         "sampling; an insufficient standing leaves the corroboration itself undeclared";
}

// The judgment table's lit/unlit arms, shared shape: presence decides, the near-miss note rides.
CorroborationAnnotation AnnotateNoQualifyingMatch(const StructureObjectRecord& object, const McAttributionInput& input,
                                                  const ObjectDeltaImage& image, const ObjectMatch& best,
                                                  McAttributionCounts* counts) {
  CorroborationAnnotation note;
  note.tolerance_rad = std::max(input.h_rad, DeclaredWidthOf(object));
  if (best.record_index >= 0) {
    note.match_distance = best.distance;
    note.matched_record = best.record_index;
    note.matched_record_ess = best.record_ess;
  }
  const bool matched_but_below =
      best.record_index >= 0 && best.distance <= note.tolerance_rad && best.record_ess < input.thresholds.min_ess;
  const bool lit =
      object.visibility.state == VisibilityState::kCertified || object.visibility.state == VisibilityState::kPartial;
  note.presence_ess =
      PresenceEss(input.mc->discovery.measure.components, image.delta_rad, input.geometry.sun_dir, input.h_rad, counts);
  note.ruler = RulerString(input, best.orbit_used);
  if (lit) {
    if (note.presence_ess < input.thresholds.min_ess) {
      note.state = CorroborationState::kNotObservedInsufficientEss;
      note.reason = "lit object unobserved; regional presence ESS " + std::to_string(note.presence_ess) +
                    " below the floor " + std::to_string(input.thresholds.min_ess);
    } else {
      note.state = CorroborationState::kNotObservedDespiteSufficientEss;
      note.reason =
          "red flag: a lit object with no qualifying MC record at sufficient regional "
          "presence ESS " +
          std::to_string(note.presence_ess);
    }
  } else if (note.presence_ess >= input.thresholds.min_ess) {
    note.state = CorroborationState::kConsistent;
    note.reason = object.visibility.state == VisibilityState::kUnproven ?
                      "unproven object asserts nothing the MC could contradict; presence ESS " +
                          std::to_string(note.presence_ess) :
                      "unlit object unobserved at presence ESS " + std::to_string(note.presence_ess);
  } else {
    note.state = CorroborationState::kNotObservedInsufficientEss;
    note.reason = std::string(object.visibility.state == VisibilityState::kUnproven ? "unproven object unobserved" :
                                                                                      "unlit object unobserved") +
                  "; presence ESS " + std::to_string(note.presence_ess) + " below the floor " +
                  std::to_string(input.thresholds.min_ess) + "; " + PresenceReadingNote();
  }
  if (matched_but_below) {
    note.reason +=
        "; matched-but-below-threshold: an MC record sits within tolerance but its own "
        "ESS is below the floor, so it does not qualify as the observation";
  }
  return note;
}

}  // namespace

long long MemberIndexOf(const Schema3DiscoveryCore& core, const std::vector<int>& member) {
  for (size_t i = 0; i < core.support.members.size(); i++) {
    if (core.support.members[i].member == member) {
      return static_cast<long long>(i);
    }
  }
  return -1;
}

double DeclaredWidthOf(const StructureObjectRecord& object) {
  if (!object.chromatic_assessed || object.chromatic.features.empty()) {
    return 0.0;
  }
  // The kernel's own dominant rule (visible first, then the largest score, first maximal wins)
  // — the registered gap's local restatement, see the header.
  const analytic::ChromaticFeature* top = nullptr;
  for (const analytic::ChromaticFeature& feature : object.chromatic.features) {
    if (top == nullptr || (feature.visible && !top->visible) ||
        (feature.visible == top->visible && feature.Score() > top->Score())) {
      top = &feature;
    }
  }
  return top->spread;
}

ObjectDeltaImage ObjectDeltaImageOf(const StructureObjectRecord& object, const Schema3DiscoveryCore& core,
                                    const McAttributionContext& context, McAttributionCounts* counts) {
  ObjectDeltaImage image;
  if (object.existence != ExistenceState::kComputed) {
    image.unavailable_reason = "object_not_computed";
    return image;
  }
  std::vector<double> values;
  if (object.kind == ObjectKind::kKind1) {
    // v1 does not trace the kind-1 curve body: the image is the member's support-row critical
    // structure — interval endpoints, the endpoint onsets' values, the constant circles.
    const long long member = MemberIndexOf(core, object.member);
    if (member < 0) {
      image.unavailable_reason = "support_row_missing";
      return image;
    }
    const MemberSupport& row = core.support.members[member];
    for (const analytic::DeviationInterval& interval : row.axis.intervals) {
      values.push_back(interval.lower);
      values.push_back(interval.upper);
    }
    for (const analytic::CriticalOnset& onset : row.endpoint_onsets) {
      values.push_back(onset.value);
    }
    for (const ConstantDeltaCurve& curve : row.constant_curves) {
      values.push_back(curve.d_p);
      for (const double critical : curve.critical_d_p) {
        if (std::isfinite(critical)) {
          values.push_back(critical);
        }
      }
    }
  } else if (!object.u.empty()) {
    // The u-preimage rule: one SampleOptical per point through the closure routing (the single
    // D_P evaluation entry; a non-finite routed value is skipped — a point off the closure of
    // U_P claims no deviation, it does not claim zero).
    int slots[analytic::kMaxFaceCount];
    if (analytic::ResolveFaceSequence(*context.normals, object.member.data(), static_cast<int>(object.member.size()),
                                      slots) != analytic::Status::kOk) {
      image.unavailable_reason = "member_unresolved";
      return image;
    }
    const analytic::DeviationField field(*context.normals, *context.polygons, slots,
                                         static_cast<int>(object.member.size()), context.base_index);
    const bool slab = field.fold().degenerate;
    for (size_t i = 0; i + 2 < object.u.size(); i += 3) {
      const double u[3] = { object.u[i], object.u[i + 1], object.u[i + 2] };
      const analytic::FieldSample sample = field.SampleOptical(u);
      double routed = 0.0;
      if (analytic::RoutedDeviation(sample, slab, &routed) == analytic::RoutedDeviationStatus::kOk &&
          std::isfinite(routed)) {
        values.push_back(routed);
      }
      if (counts != nullptr) {
        counts->dp_evaluations++;
      }
    }
  } else {
    image.unavailable_reason = "no_delta_image";
    return image;
  }
  std::sort(values.begin(), values.end());
  for (const double value : values) {
    if (image.delta_rad.empty() || std::fabs(value - image.delta_rad.back()) > analytic::kExtremumAtol) {
      image.delta_rad.push_back(value);
    }
  }
  if (image.delta_rad.empty()) {
    image.unavailable_reason = "no_delta_image";
    return image;
  }
  image.available = true;
  return image;
}

double PresenceEss(const std::vector<analytic::WeightedSkySample>& components,
                   const std::vector<double>& image_delta_rad, const double sun_dir[3], double h_rad,
                   McAttributionCounts* counts) {
  if (image_delta_rad.empty() || !(h_rad > 0.0) || !std::isfinite(h_rad)) {
    return 0.0;
  }
  if (counts != nullptr) {
    counts->presence_ess_queries++;
  }
  const double kappa = 1.0 / (h_rad * h_rad);
  // Any common scale cancels in the Kish ratio, so the kernel value runs unnormalized — the
  // normalized authority (and its orbit branch) stays in analytic/path_feature_discovery.hpp.
  // Rows are grouped by outer draw (sample_index, the builder's emission order): one draw is
  // one sampling act even when it contributed several member/spectral rows — the grouping rule
  // mirrors analytic/path_feature_discovery's own EffectiveCount discipline, and it keeps the
  // count conservative (a red flag must rest on genuinely independent observations).
  double sum = 0.0;
  double sum_squared = 0.0;
  uint64_t current_index = 0;
  bool first = true;
  double group = 0.0;
  auto close_group = [&] {
    if (!first) {
      sum += group;
      sum_squared += group * group;
    }
  };
  for (const analytic::WeightedSkySample& component : components) {
    if (first || component.sample_index != current_index) {
      close_group();
      group = 0.0;
      current_index = component.sample_index;
      first = false;
    }
    const double delta = AngleBetween(sun_dir, component.direction.data());
    double best = -kInf;
    for (const double image : image_delta_rad) {
      best = std::max(best, std::cos(delta - image));
    }
    if (best <= -1.0) {
      continue;  // exp(kappa * -2) underflow territory: no presence contribution
    }
    group += std::exp(kappa * (best - 1.0));
  }
  close_group();
  return sum_squared > 0.0 ? sum * sum / sum_squared : 0.0;
}

namespace {

// The shared entry validation (both directions refuse identically — fail-visible, never a
// silent re-interpretation).
std::string ValidateInput(const McAttributionInput& input) {
  if (input.core == nullptr || input.mc == nullptr) {
    return "null core or evidence";
  }
  if (input.geometry.normals == nullptr || input.geometry.polygons == nullptr) {
    return "null crystal tables";
  }
  if (!(input.h_rad > 0.0) || !std::isfinite(input.h_rad)) {
    return "invalid observation bandwidth";
  }
  if (input.h_rad != input.mc->observation_options.bandwidth_rad) {
    return "observation bandwidth mismatch: the matching ruler must be the observation the "
           "evidence was taken under";
  }
  return "";
}

}  // namespace

CorroborationOutcome DeriveCorroboration(const McAttributionInput& input) {
  CorroborationOutcome outcome;
  outcome.error = ValidateInput(input);
  if (!outcome.error.empty()) {
    return outcome;
  }
  outcome.core = *input.core;
  const std::vector<McRecordFacts> records = ActualRecordFacts(*input.mc, input.geometry);
  for (StructureObjectRecord& object : outcome.core.objects) {
    CorroborationAnnotation note;
    const ObjectDeltaImage image = ObjectDeltaImageOf(object, *input.core, input.geometry, &outcome.counts);
    if (object.existence != ExistenceState::kComputed) {
      note.state = CorroborationState::kUnchecked;
      note.reason = "object_not_computed";
    } else if (!image.available) {
      note.state = CorroborationState::kUnchecked;
      note.reason = image.unavailable_reason;
    } else {
      const ObjectMatch best = MatchObject(object, image, records, *input.core, input);
      if (best.record_index >= 0 && best.distance <= std::max(input.h_rad, DeclaredWidthOf(object)) &&
          best.record_ess >= input.thresholds.min_ess) {
        note.state = CorroborationState::kObserved;
        note.match_distance = best.distance;
        note.tolerance_rad = std::max(input.h_rad, DeclaredWidthOf(object));
        note.matched_record = best.record_index;
        note.matched_record_ess = best.record_ess;
        note.ruler = RulerString(input, best.orbit_used);
        note.reason = "MC record within tolerance at standing";
      } else {
        note = AnnotateNoQualifyingMatch(object, input, image, best, &outcome.counts);
      }
    }
    object.corroboration = note.state;
    outcome.annotations.push_back(std::move(note));
  }
  return outcome;
}

UnattributedOutcome DeriveUnattributed(const McAttributionInput& input) {
  UnattributedOutcome outcome;
  outcome.error = ValidateInput(input);
  if (!outcome.error.empty()) {
    return outcome;
  }
  const std::vector<McRecordFacts> records = ActualRecordFacts(*input.mc, input.geometry);
  for (const McRecordFacts& record : records) {
    if (!record.has_position) {
      outcome.skipped_no_position++;
      continue;
    }
    if (record.ess < input.thresholds.min_ess) {
      outcome.skipped_below_ess++;
      continue;  // the AC2 gate: a starved MC has no witnessing qualification
    }
    double min_margin = kInf;
    std::array<double, 3> closest = record.sky_points.front();
    double closest_delta = record.delta_rad.front();
    bool orbit_used = false;
    for (const StructureObjectRecord& object : input.core->objects) {
      const ObjectDeltaImage image = ObjectDeltaImageOf(object, *input.core, input.geometry, &outcome.counts);
      if (!image.available) {
        continue;
      }
      const double tolerance = std::max(input.h_rad, DeclaredWidthOf(object));
      double distance = BaseDistance(record.delta_rad, image.delta_rad);
      long long orbit_index = -1;
      const double orbit = OrbitImageDistance(object, *input.core, record.sky_points, &orbit_index);
      if (orbit < distance) {
        distance = orbit;
      }
      const double margin = distance - tolerance;
      if (margin < min_margin) {
        min_margin = margin;
        // The sky point of closest approach for the reporting position.
        double best_point_distance = kInf;
        for (size_t p = 0; p < record.sky_points.size(); p++) {
          const double point_delta = record.delta_rad[p];
          for (const double image_delta : image.delta_rad) {
            const double gap = std::fabs(point_delta - image_delta);
            if (gap < best_point_distance) {
              best_point_distance = gap;
              closest = record.sky_points[p];
              closest_delta = point_delta;
            }
          }
        }
        orbit_used = orbit < BaseDistance(record.delta_rad, image.delta_rad);
      }
    }
    if (min_margin > 0.0) {
      UnattributedStructure structure;
      structure.record_index = record.index;
      structure.position = closest;
      structure.delta_rad = closest_delta;
      structure.record_ess = record.ess;
      structure.min_margin_rad = min_margin;
      structure.ruler = RulerString(input, orbit_used);
      outcome.structures.push_back(std::move(structure));
    }
  }
  return outcome;
}

}  // namespace lumice::raypath::schema3
