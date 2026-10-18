#include "raypath/detail/survey/survey_t1.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "analytic/dp_chromatic.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_partition.hpp"
#include "core/optics.hpp"                                       // IceRefractiveIndex: the engine's dispersion table
#include "raypath/detail/measure/measure_geometry_contract.hpp"  // PartitionContext
#include "raypath/detail/schema3/geometry_source.hpp"
#include "raypath/detail/schema3/support_block.hpp"
#include "raypath/scene_to_analytic.hpp"  // ResolveSingleLayerPath
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;
// The closed-form cross-check's acceptance: the kernel's two-face interior minimum IS the
// closed form, so the survey-side restatement (A from the face normals through acos) is the
// only error source — its measured floor is the acos-at-1 ~1.5e-8 rad, and 1e-6 rad keeps
// three decades of margin (the same ruler geometry_source.hpp names for orbit certificates).
constexpr double kClosedFormToleranceRad = 1e-6;

double DegOf(double radians) {
  return radians * kRadToDeg;
}

// The two-face chain's closed-form minimum deviation at index n: 2*asin(n*sin(A/2)) - A, with A
// the refracting angle between the entry and exit faces (pi minus the angle between their
// normals — normals 120 degrees apart make the 60-degree prism of the 22-degree halo).
bool ClosedFormMinDeviation(const analytic::FaceNormalTable& table, const std::vector<int>& representative,
                            double refractive_index, double* apex_deg, double* min_deviation_deg) {
  if (representative.size() != 2) {
    return false;
  }
  const int slot_a = table.SlotOf(representative[0]);
  const int slot_b = table.SlotOf(representative[1]);
  if (slot_a < 0 || slot_b < 0) {
    return false;
  }
  double dot = 0.0;
  for (int k = 0; k < 3; k++) {
    dot += table.normal[slot_a][k] * table.normal[slot_b][k];
  }
  dot = std::clamp(dot, -1.0, 1.0);
  const double apex = kPi - std::acos(dot);  // the refracting angle between the two faces
  const double sine = refractive_index * std::sin(0.5 * apex);
  if (!(sine <= 1.0)) {
    return false;  // no refracted minimum exists (the entry face admits only TIR)
  }
  *apex_deg = DegOf(apex);
  *min_deviation_deg = DegOf(2.0 * std::asin(sine) - apex);
  return true;
}

const analytic::CriticalOnset* MinDeviationOnset(const std::vector<analytic::CriticalOnset>& onsets) {
  const analytic::CriticalOnset* best = nullptr;
  for (const analytic::CriticalOnset& onset : onsets) {
    if (onset.source != analytic::OnsetSource::kInteriorMinimum) {
      continue;
    }
    if (best == nullptr || onset.value < best->value) {
      best = &onset;
    }
  }
  return best;
}

}  // namespace

T1Outcome AssembleT1Objects(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                            const std::vector<std::vector<int>>& members, const std::vector<int>& representative,
                            double refractive_index, double wavelength_nm, SurveyDocument* out) {
  // One axis assembly per member; the support row is the schema3 machine's own (the same
  // assembly feeds the objects and the row — no second partition anywhere).
  std::vector<schema3::AxisAssembly> assemblies;
  std::vector<schema3::MemberSupport> rows;
  assemblies.reserve(members.size());
  rows.reserve(members.size());
  for (const std::vector<int>& member : members) {
    std::vector<int> slots;
    if (const Error slot_error = ResolveSingleLayerPath({ member }, table, &slots); !slot_error.Ok()) {
      return { false, "member_resolution" };
    }
    const analytic::DeviationField field(table, polygons, slots.data(), static_cast<int>(slots.size()),
                                         refractive_index);
    assemblies.emplace_back(schema3::AssembleAxis(field));
    rows.push_back(schema3::SupportRowOf(assemblies.back(), table, polygons, slots.data(),
                                         static_cast<int>(slots.size()), refractive_index, member, {}, {}));
  }
  // Fail closed: a walk refusal or a partition escape leaves the class support unknown.
  for (const schema3::MemberSupport& row : rows) {
    if (row.axis.walk_closed && row.axis.context.coverage == PartitionContext::Coverage::kComplete) {
      continue;
    }
    const char* mode =
        row.axis.walk_closed ? row.axis.regime_slug.c_str() : analytic::WalkStatusName(row.axis.walk_status);
    return { false, mode == nullptr ? "walk_truncated" : mode };
  }
  // The class-level support: the members are relabelings of one physical class, so their
  // partitions must sit on the same delta axis (SameSupport — the one ruler; a disagreement is
  // not a union to fabricate but a refusal).
  const std::vector<analytic::DeviationInterval>& class_support = rows.front().axis.intervals;
  for (const schema3::MemberSupport& row : rows) {
    if (!schema3::SameSupport(class_support, row.axis.intervals)) {
      return { false, "member_supports_disagree" };
    }
  }

  // The support_set card: the class's delta-axis support with the endpoint mechanism labels
  // (the first member's onsets at an interval endpoint — the members share the axis) and the
  // caliber the partition ran on.
  SurveyObject support;
  support.kind = SurveyObjectKind::kSupportSet;
  support.members = members;
  support.evidence.state = EvidenceState::kFinite;
  support.evidence.coverage.members_evaluated = static_cast<int>(members.size());
  support.evidence.coverage.wavelength_nm = wavelength_nm;
  SurveyObject::SupportSetPayload support_payload;
  support_payload.symmetry_bits = out->request.symmetry_bits;
  support_payload.member_count = static_cast<int>(members.size());
  for (const analytic::DeviationInterval& interval : class_support) {
    SurveyObject::Interval out_interval;
    out_interval.lower_deg = DegOf(interval.lower);
    out_interval.upper_deg = DegOf(interval.upper);
    out_interval.n_components = interval.n_components;
    out_interval.n_closed = interval.n_closed;
    out_interval.n_open = interval.n_open;
    support_payload.intervals.push_back(out_interval);
  }
  for (const analytic::CriticalOnset& onset : rows.front().endpoint_onsets) {
    SurveyObject::EndpointLabel label;
    label.delta_deg = DegOf(onset.value);
    label.source = analytic::OnsetSourceName(onset.source);
    label.profile = analytic::OnsetProfileName(onset.profile);
    label.multiplicity = onset.multiplicity;
    support_payload.endpoints.push_back(std::move(label));
  }
  support.support_set = std::move(support_payload);
  out->objects.push_back(std::move(support));

  // The caustic_edge card: the class's minimum-deviation fold. On a two-face chain the
  // position certifies closed-form (the kernel's slab interior minimum is the closed form; the
  // survey restates it and cross-checks — a mismatch refuses, it never picks one silently).
  const analytic::CriticalOnset* onset = MinDeviationOnset(assemblies.front().onsets);
  if (onset != nullptr) {
    SurveyObject edge;
    edge.kind = SurveyObjectKind::kCausticEdge;
    edge.members = members;
    double apex_deg = 0.0;
    double closed_form_deg = 0.0;
    const bool closed_form =
        ClosedFormMinDeviation(table, representative, refractive_index, &apex_deg, &closed_form_deg);
    if (closed_form && std::fabs(DegOf(onset->value) - closed_form_deg) > DegOf(kClosedFormToleranceRad)) {
      // The two readings of one quantity disagree beyond the restatement's own error: refuse.
      edge.evidence.state = EvidenceState::kRefused;
      edge.evidence.mode = "closed_form_mismatch";
    } else {
      edge.evidence.state = EvidenceState::kFinite;
      edge.evidence.coverage.members_evaluated = static_cast<int>(members.size());
      edge.evidence.coverage.wavelength_nm = wavelength_nm;
      SurveyObject::CausticEdgePayload payload;
      payload.position_deg = DegOf(onset->value);
      payload.profile = analytic::OnsetProfileName(onset->profile);
      payload.has_measure_limit = onset->has_measure_limit;
      payload.measure_limit = onset->measure_limit;
      payload.coincident_with_support =
          std::fabs(onset->value - class_support.front().lower) <= analytic::kExtremumAtol;
      if (closed_form) {
        edge.evidence.state = EvidenceState::kCertified;
        edge.evidence.method = kCertifiedMethodSlabMinDeviationClosedForm;
        SurveyObject::EdgeCertificate certificate;
        certificate.formula = "2*asin(n*sin(A/2)) - A";
        certificate.apex_angle_deg = apex_deg;
        certificate.cross_check_deg = std::fabs(DegOf(onset->value) - closed_form_deg);
        certificate.tolerance_deg = DegOf(kClosedFormToleranceRad);
        payload.certificate = std::move(certificate);
      }
      edge.caustic_edge = std::move(payload);
    }
    out->objects.push_back(std::move(edge));
  }

  // The bright_domain skeleton card: the support's interior span, evidence finite. The probe
  // leg fills the peak/edge readings (the sharp edge lives on the caustic card, certified;
  // the brightness peak lives here, finite — different quantities, different states).
  SurveyObject domain;
  domain.kind = SurveyObjectKind::kBrightDomain;
  domain.members = members;
  domain.evidence.state = EvidenceState::kFinite;
  domain.evidence.coverage.members_evaluated = static_cast<int>(members.size());
  domain.evidence.coverage.wavelength_nm = wavelength_nm;
  SurveyObject::BrightDomainPayload domain_payload;
  domain_payload.domain_lower_deg = DegOf(class_support.front().lower);
  domain_payload.domain_upper_deg = DegOf(class_support.back().upper);
  domain.bright_domain = std::move(domain_payload);
  out->objects.push_back(std::move(domain));

  return { true, {} };
}

void AssembleChromaticLayer(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                            const std::vector<int>& representative, const analytic::PoseDensitySpec& density,
                            int class_size, SurveyDocument* out) {
  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ representative }, table, &slots); !slot_error.Ok()) {
    // Unreachable past the T1 arm (it resolved the same sequence), but the refusal stays honest.
    out->chromatic.state = EvidenceState::kRefused;
    out->chromatic.mode = "member_resolution";
    return;
  }
  // The fixed index pair (plan A2): the pool's red and blue ends through the engine's own
  // dispersion table; the document echoes exactly these n values.
  const std::vector<std::string> labels{ "650nm", "450nm" };
  const std::vector<double> indices{ IceRefractiveIndex::Get(650.0), IceRefractiveIndex::Get(450.0) };

  // Leg 1: the positional dispersion — the min-deviation edge followed across the pair.
  const analytic::WavelengthCriticalTable critical = analytic::WavelengthCriticalTableOf(
      table, polygons, slots.data(), static_cast<int>(slots.size()), density, labels, indices);
  if (critical.escaped) {
    out->chromatic.state = EvidenceState::kRefused;
    out->chromatic.mode = "wavelength_critical_table";
    return;
  }
  // The min-deviation row: the interior-minimum finite-jump onset with the smallest red value,
  // picked explicitly (the same shape as the support's MinDeviationOnset) rather than by row
  // order — the table's value ordering stays the kernel's fact, not this loop's premise.
  const analytic::WavelengthOnsetShift* min_row = nullptr;
  for (const analytic::WavelengthOnsetShift& row : critical.onsets) {
    if (row.source != analytic::OnsetSource::kInteriorMinimum || row.profile != analytic::OnsetProfile::kFiniteJump) {
      continue;
    }
    if (min_row == nullptr || row.values_deg[0] < min_row->values_deg[0]) {
      min_row = &row;
    }
  }
  if (min_row == nullptr || min_row->values_deg.size() < 2) {
    out->chromatic.state = EvidenceState::kRefused;
    out->chromatic.mode = "min_deviation_row_not_found";
    return;
  }
  out->positional_dispersion = SurveyPositionalDispersion{};
  SurveyPositionalDispersion& dispersion = *out->positional_dispersion;
  dispersion.red_label = labels[0];
  dispersion.blue_label = labels[1];
  dispersion.n_red = indices[0];
  dispersion.n_blue = indices[1];
  dispersion.value_red_deg = min_row->values_deg[0];
  dispersion.value_blue_deg = min_row->values_deg[1];
  dispersion.displacement_deg = min_row->displacement_deg;
  dispersion.direction =
      dispersion.value_red_deg < dispersion.value_blue_deg ? "red_inner_blue_outer" : "blue_inner_red_outer";
  dispersion.evidence.state = EvidenceState::kFinite;
  dispersion.evidence.coverage.members_evaluated = 1;  // the representative; the class reads xN
  dispersion.evidence.coverage.geometry_basis = "the kernel's wavelength-critical table at the 650/450 index pair";
  dispersion.evidence.coverage.wavelength_nm = 450.0;

  // Leg 2: the diagnose — the weight-kink and gate-edge features as their own sub-block (a
  // different kernel path than the dispersion table; the two mechanisms never merge fields).
  const analytic::ChromaticVerdict verdict =
      analytic::Diagnose(table, polygons, slots.data(), static_cast<int>(slots.size()), indices[0], indices[1]);
  SurveyDiagnoseBlock& block = out->diagnose.emplace();
  block.verdict_kind = analytic::ChromaticVerdictKindName(verdict.kind);
  block.color = analytic::ChromaticColorName(verdict.color);
  block.visible = verdict.visible;
  block.has_position = verdict.has_position;
  block.position_deg = DegOf(verdict.position);
  block.coverage_complete = verdict.coverage_complete;
  block.notes = verdict.notes;
  block.n_red = verdict.n_red;
  block.n_blue = verdict.n_blue;
  for (const analytic::ChromaticFeature& feature : verdict.features) {
    SurveyDiagnoseFeature out_feature;
    out_feature.kind = analytic::ChromaticFeatureKindName(feature.kind);
    out_feature.source = feature.source;
    out_feature.color = analytic::ChromaticColorName(feature.color);
    out_feature.visible = feature.visible;
    out_feature.delta_red_deg = DegOf(feature.delta_red);
    out_feature.delta_blue_deg = DegOf(feature.delta_blue);
    out_feature.shift_deg = DegOf(feature.shift);
    block.features.push_back(std::move(out_feature));
  }
  block.evidence.state = EvidenceState::kFinite;
  block.evidence.coverage.members_evaluated = 1;
  block.evidence.coverage.geometry_basis = "the kernel's Diagnose at the 650/450 index pair";
  block.evidence.coverage.wavelength_nm = 450.0;

  // The weight-tint leg stays unwired (the C15 slice); the sub-block says so.
  out->weight_tint = SurveyEvidence{};
  out->weight_tint->state = EvidenceState::kUnevaluated;
  out->weight_tint->reason = "class tint not wired (C15 slice)";

  out->chromatic.state = EvidenceState::kFinite;
  out->chromatic.coverage.members_evaluated = class_size;
  out->chromatic.coverage.wavelength_nm = 450.0;
  out->chromatic.coverage.geometry_basis = "the chromatic legs ran on the class representative at the 650/450 pair";
}

}  // namespace lumice::raypath::survey
