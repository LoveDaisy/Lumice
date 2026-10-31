#include "raypath/detail/quantity/tir_share.hpp"

#include <cmath>
#include <vector>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/discovery.hpp"
#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table
#include "raypath/query_control.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::quantity {
using survey::EvidenceState;
namespace {

// The TIR leg's in-phase pipeline runs in fixed row chunks — row rebuild, batch evaluation and
// classification for one chunk, then the chunk-boundary poll. 1024 rows per poll keeps the
// lattice polls' cadence discipline (one poll per fixed-size work unit) against rows that cost
// orders of magnitude more each (one full diagnostic evaluation per row), so the poll's
// relative cost stays noise-level while the bounded-return window stays a fraction of a second.
constexpr size_t kTirRowChunkSize = 1024;

}  // namespace

TirShareResult EvaluateTirShare(const TirShareInput& input) {
  TirShareResult tir;
  tir.SetState(EvidenceState::kFinite);
  tir.Value().quantity =
      DescribeQuantity(QuantityKind::kTirShare, QuantityUnit::kDimensionless, QuantityMethod::kDiagnosticTirLattice,
                       ReferenceMeasure::kHaar, DensityWeighting::kNone);
  tir.Value().quantity.spectrum = SpectralObservation::kFixedChromaticPair;
  tir.Value().quantity.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  DeclareQuantity(&tir, tir.Value().quantity, { *input.representative }, std::nullopt);
  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution, but the refusal stays honest —
    // the same discipline as the sibling legs.
    tir.SetState(EvidenceState::kRefused);
    tir.Mode() = "member_resolution";
    return CompleteQuantity(std::move(tir));
  }
  // Internal interfaces only: the chain's entry and exit faces are transmission gates, not TIR
  // candidates, so a two-face slab (one entry, one exit) leaves the leg no question to ask.
  const int internal_count = static_cast<int>(slots.size()) - 2;
  if (internal_count < 1) {
    tir.SetState(EvidenceState::kRefused);
    tir.Mode() = "no_internal_interfaces";
    return CompleteQuantity(std::move(tir));
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
    if (input.stop != nullptr) {
      if (input.stop->token != nullptr) {
        input.stop->token->NotePoll(raypath::kPollSurveyTirPass);  // the leg's pass boundary
      }
      if (input.stop->Stopped()) {
        // The shares need both passes over the full lattice: a cut pass claims nothing.
        tir.SetState(EvidenceState::kRefused);
        tir.Mode() = "stop_requested";
        return CompleteQuantity(std::move(tir));
      }
    }
    const double refractive_index = lambda == 0 ? n_red : n_blue;
    // One discovery per pass (the corridor keeps clipping scratch; the type is neither copyable
    // nor movable), the tint's own two-pass shape at one shared lattice size.
    analytic::IceDiscovery discovery(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()),
                                     refractive_index, input.incident);
    const std::vector<analytic::SampleEvent> events = discovery.BuildEvents(input.sample_count, input.stop);
    // The in-phase pipeline, chunk by chunk: rebuild the kept events' diagnostic rows, evaluate
    // the chunk as one batch, classify it, accumulate. The batch is a pure per-row map and the
    // classification accumulates in event order, so the chunked pass performs the same row
    // evaluations and the same additions in the same order as the unchunked one — an unstopped
    // run's shares are bit-identical (the byte-identity pin and the corpus regression hold
    // that). The poll sits BEFORE each chunk's work: a stop observed at a boundary costs the
    // run nothing past it, including a cut lattice's kept prefix (refused before its first
    // chunk does any work).
    double sum = 0.0;
    double sum_all = 0.0;
    long long degenerate = 0;
    std::vector<double> sum_iface(static_cast<size_t>(internal_count), 0.0);
    for (size_t begin = 0; begin < events.size(); begin += kTirRowChunkSize) {
      if (input.stop != nullptr) {
        if (input.stop->token != nullptr) {
          input.stop->token->NotePoll(raypath::kPollSurveyTirRows);  // the in-phase chunk boundary
        }
        if (input.stop->Stopped()) {
          // A cut pipeline claims nothing: the whole-leg refusal, the pass boundary's own shape.
          tir.SetState(EvidenceState::kRefused);
          tir.Mode() = "stop_requested";
          return CompleteQuantity(std::move(tir));
        }
      }
      const size_t end = std::min(begin + kTirRowChunkSize, events.size());
      std::vector<analytic::DiagnosticInputRow> rows;
      rows.reserve(end - begin);
      for (size_t i = begin; i < end; i++) {
        double pose[9];
        analytic::CandidatePose(input.incident, events[i].event.phi, events[i].event, pose);
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
      if (!analytic::EvaluateDiagnosticBatch(*input.representative, rows, {}, &values) ||
          values.size() != rows.size()) {
        tir.SetState(EvidenceState::kRefused);
        tir.Mode() = "batch_read_unavailable";
        return CompleteQuantity(std::move(tir));
      }
      for (size_t i = begin; i < end; i++) {
        const analytic::DiagnosticOutputRow& value = values[i - begin];
        if (!analytic::ValidateRotation(rows[i - begin].pose.data())) {
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
          tir.SetState(EvidenceState::kRefused);
          tir.Mode() = "batch_read_unavailable";
          return CompleteQuantity(std::move(tir));
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
    tir.SetState(EvidenceState::kRefused);
    tir.Mode() = "zero_band_weight";
    return CompleteQuantity(std::move(tir));
  }

  tir.Value().total_weight_red = sum_weight[0];
  tir.Value().total_weight_blue = sum_weight[1];
  tir.Value().all_tir_weight_red = sum_all_tir[0];
  tir.Value().all_tir_weight_blue = sum_all_tir[1];
  tir.Value().chart_degenerate_red = chart_degenerate[0];
  tir.Value().chart_degenerate_blue = chart_degenerate[1];
  tir.Value().n_red = n_red;
  tir.Value().n_blue = n_blue;
  tir.Value().interfaces.resize(static_cast<size_t>(internal_count));
  for (int j = 0; j < internal_count; j++) {
    TirShareInterface& row = tir.Value().interfaces[static_cast<size_t>(j)];
    row.face = (*input.representative)[static_cast<size_t>(j) + 1];
    row.tir_weight_red = sum_interface_tir[0][static_cast<size_t>(j)];
    row.tir_weight_blue = sum_interface_tir[1][static_cast<size_t>(j)];
    row.share_red = sum_interface_tir[0][static_cast<size_t>(j)] / sum_weight[0];
    row.share_blue = sum_interface_tir[1][static_cast<size_t>(j)] / sum_weight[1];
  }
  tir.Value().share_all_tir_red = sum_all_tir[0] / sum_weight[0];
  tir.Value().share_all_tir_blue = sum_all_tir[1] / sum_weight[1];
  tir.Value().sample_count = input.sample_count;
  tir.Value().lattice = "antipodal_fibonacci";
  tir.Value().deterministic = true;
  tir.Value().representative = *input.representative;
  tir.Value().member_count = input.member_count;
  tir.Value().kept_count_red = kept_count[0];
  tir.Value().kept_count_blue = kept_count[1];
  tir.Value().caliber = std::string(
                            "share of kept-event transmission weight (w = A*T) on poses with every internal "
                            "interface TIR, per lambda: the declared antipodal Fibonacci lattice x the class "
                            "representative at the fixed ") +
                        ChromaticPairProseRedFirst() +
                        " pair; kept events whose candidate-pose chart degenerates at exactly-zero deviation (the "
                        "pose's azimuthal freedom is unconstrained there) sit outside the estimator's declared "
                        "support, excluded from both numerator and denominator; a class-level estimate of the "
                        "near-TIR weight mechanism, NOT the C15 corpus's actual-distribution positive-A caliber "
                        "and not a cross-lambda ratio";
  tir.SetState(EvidenceState::kFinite);
  tir.Coverage().members_evaluated = 1;  // the representative; the class reads xN
  tir.Coverage().geometry_basis =
      "the class power-sum lattice kept events, re-classified per internal interface by the "
      "diagnostic batch on the nominal crystal shape at the fixed " +
      ChromaticPairProseRedFirst() + " pair";
  tir.Coverage().wavelength_nm = kChromaticBlueNm;

  ILOG_INFO(GetGlobalLogger(),
            "[raypath survey] tir share: all-TIR share {:.4f} (red) / {:.4f} (blue), {} interfaces, "
            "{} samples per lambda, chart-degenerate events {}/{} (red), {}/{} (blue)",
            tir.Value().share_all_tir_red, tir.Value().share_all_tir_blue, internal_count, input.sample_count,
            chart_degenerate[0], kept_count[0], chart_degenerate[1], kept_count[1]);
  return CompleteQuantity(std::move(tir));
}

}  // namespace lumice::raypath::quantity
