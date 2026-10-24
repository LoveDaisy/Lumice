#include "raypath/detail/survey/survey_tir_share.hpp"

#include <cmath>
#include <vector>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/discovery.hpp"
#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

void RefuseTirShare(SurveyResult* out, const char* mode) {
  SurveyTirShare& tir = out->tir_share.emplace();
  tir.evidence.state = EvidenceState::kRefused;
  tir.evidence.mode = mode;
}

}  // namespace

void AssembleTirShare(const TirShareInput& input, SurveyResult* out) {
  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution, but the refusal stays honest —
    // the same discipline as the sibling legs.
    RefuseTirShare(out, "member_resolution");
    return;
  }
  // Internal interfaces only: the chain's entry and exit faces are transmission gates, not TIR
  // candidates, so a two-face slab (one entry, one exit) leaves the leg no question to ask.
  const int internal_count = static_cast<int>(slots.size()) - 2;
  if (internal_count < 1) {
    RefuseTirShare(out, "no_internal_interfaces");
    return;
  }

  const double n_red = IceRefractiveIndex::Get(kChromaticRedNm);
  const double n_blue = IceRefractiveIndex::Get(kChromaticBlueNm);

  double sum_weight[2] = { 0.0, 0.0 };
  double sum_all_tir[2] = { 0.0, 0.0 };
  long long chart_degenerate[2] = { 0, 0 };
  std::vector<double> sum_interface_tir[2] = { std::vector<double>(static_cast<size_t>(internal_count), 0.0),
                                               std::vector<double>(static_cast<size_t>(internal_count), 0.0) };
  long long kept_count[2] = { 0, 0 };

  for (int lambda = 0; lambda < 2; lambda++) {
    const double refractive_index = lambda == 0 ? n_red : n_blue;
    // One discovery per pass (the corridor keeps clipping scratch; the type is neither copyable
    // nor movable), the tint's own two-pass shape at one shared lattice size.
    analytic::IceDiscovery discovery(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()),
                                     refractive_index, input.incident);
    const std::vector<analytic::SampleEvent> events = discovery.BuildEvents(input.sample_count);
    // Every kept event re-posed and re-derived in one batch call; the batch is the classifier,
    // the shares' numerator and denominator both read the discovery-side w = A*T.
    std::vector<analytic::DiagnosticInputRow> rows;
    rows.reserve(events.size());
    for (const analytic::SampleEvent& event : events) {
      double pose[9];
      analytic::CandidatePose(input.incident, event.event.phi, event.event, pose);
      analytic::DiagnosticInputRow row;
      row.crystal = *input.crystal;
      for (int k = 0; k < 9; k++) {
        row.pose[static_cast<size_t>(k)] = pose[k];
      }
      for (int k = 0; k < 3; k++) {
        row.incident[static_cast<size_t>(k)] = input.incident[k];
      }
      row.refractive_index = refractive_index;
      row.source_token = 0;
      rows.push_back(std::move(row));
    }
    std::vector<analytic::DiagnosticOutputRow> values;
    if (!analytic::EvaluateDiagnosticBatch(*input.representative, rows, {}, &values) || values.size() != rows.size()) {
      RefuseTirShare(out, "batch_read_unavailable");
      return;
    }
    double sum = 0.0;
    double sum_all = 0.0;
    long long degenerate = 0;
    std::vector<double> sum_iface(static_cast<size_t>(internal_count), 0.0);
    for (size_t i = 0; i < events.size(); i++) {
      const analytic::DiagnosticOutputRow& value = values[i];
      if (!analytic::ValidateRotation(rows[i].pose.data())) {
        // The candidate-pose chart's zero-deviation degeneracy: at deviation exactly 0 the
        // event's outgoing direction is antiparallel to its incoming axis, the chart's F triad
        // degenerates and the pose's azimuthal freedom is unconstrained — no pose is THE
        // event's, so no TIR classification exists and the event sits outside the estimator's
        // declared support (excluded from both numerator and denominator below, its count
        // reported in the leg's log line and its caliber prose; the witness leg's argmax never
        // lands here, which is why the sibling never saw it). A validated pose the batch still
        // cannot trace is a different thing: a reader disagreement, refused below.
        degenerate++;
        continue;
      }
      if (!value.path_valid || static_cast<int>(value.interfaces.size()) != internal_count + 2) {
        // The lattice kept this pose (w = A*T > 0 in its own read) and the batch does not
        // confirm it: a reader disagreement is a defect to investigate, never a row to drop
        // from an estimator.
        RefuseTirShare(out, "batch_read_unavailable");
        return;
      }
      const double weight = events[i].weight;
      bool all_tir = true;
      for (int j = 0; j < internal_count; j++) {
        if (value.interfaces[static_cast<size_t>(j) + 1].discriminant < 0.0) {
          sum_iface[static_cast<size_t>(j)] += weight;
        } else {
          all_tir = false;
        }
      }
      sum += weight;
      if (all_tir) {
        sum_all += weight;
      }
    }
    sum_weight[lambda] = sum;
    sum_all_tir[lambda] = sum_all;
    sum_interface_tir[lambda] = std::move(sum_iface);
    chart_degenerate[lambda] = degenerate;
    kept_count[lambda] = static_cast<long long>(events.size());
  }

  if (!(sum_weight[0] > 0.0) || !(sum_weight[1] > 0.0) || !std::isfinite(sum_weight[0]) ||
      !std::isfinite(sum_weight[1])) {
    // A degenerate band makes the shares no number; the document must not carry a silent inf.
    RefuseTirShare(out, "zero_band_weight");
    return;
  }

  SurveyTirShare& tir = out->tir_share.emplace();
  tir.n_red = n_red;
  tir.n_blue = n_blue;
  tir.interfaces.resize(static_cast<size_t>(internal_count));
  for (int j = 0; j < internal_count; j++) {
    SurveyTirShareInterface& row = tir.interfaces[static_cast<size_t>(j)];
    row.face = (*input.representative)[static_cast<size_t>(j) + 1];
    row.share_red = sum_interface_tir[0][static_cast<size_t>(j)] / sum_weight[0];
    row.share_blue = sum_interface_tir[1][static_cast<size_t>(j)] / sum_weight[1];
  }
  tir.share_all_tir_red = sum_all_tir[0] / sum_weight[0];
  tir.share_all_tir_blue = sum_all_tir[1] / sum_weight[1];
  tir.sample_count = input.sample_count;
  tir.lattice = "antipodal_fibonacci";
  tir.deterministic = true;
  tir.representative = *input.representative;
  tir.member_count = input.member_count;
  tir.kept_count_red = kept_count[0];
  tir.kept_count_blue = kept_count[1];
  tir.caliber = std::string(
                    "share of kept-event transmission weight (w = A*T) on poses with every internal "
                    "interface TIR, per lambda: the declared antipodal Fibonacci lattice x the class "
                    "representative at the fixed ") +
                ChromaticPairProseRedFirst() +
                " pair; kept events whose candidate-pose chart degenerates at exactly-zero deviation (the "
                "pose's azimuthal freedom is unconstrained there) sit outside the estimator's declared "
                "support, excluded from both numerator and denominator; a class-level estimate of the "
                "near-TIR weight mechanism, NOT the C15 corpus's actual-distribution positive-A caliber "
                "and not a cross-lambda ratio";
  tir.evidence.state = EvidenceState::kFinite;
  tir.evidence.coverage.members_evaluated = 1;  // the representative; the class reads xN
  tir.evidence.coverage.geometry_basis =
      "the class power-sum lattice kept events, re-classified per internal interface by the "
      "diagnostic batch on the nominal crystal shape at the fixed " +
      ChromaticPairProseRedFirst() + " pair";
  tir.evidence.coverage.wavelength_nm = kChromaticBlueNm;

  ILOG_INFO(GetGlobalLogger(),
            "[raypath survey] tir share: all-TIR share {:.4f} (red) / {:.4f} (blue), {} interfaces, "
            "{} samples per lambda, chart-degenerate events {}/{} (red), {}/{} (blue)",
            tir.share_all_tir_red, tir.share_all_tir_blue, internal_count, input.sample_count, chart_degenerate[0],
            kept_count[0], chart_degenerate[1], kept_count[1]);
}

}  // namespace lumice::raypath::survey
