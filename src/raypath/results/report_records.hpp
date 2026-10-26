#ifndef LUMICE_RAYPATH_RESULTS_REPORT_RECORDS_HPP_
#define LUMICE_RAYPATH_RESULTS_REPORT_RECORDS_HPP_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "analytic/diagnostic_types.hpp"
#include "analytic/field_types.hpp"
namespace lumice::raypath {

struct DiagnosticSourceIdentity {
  uint64_t sample_index = 0;
  size_t member_index = 0;
  size_t spectral_row = 0;
};

struct DiagnosticMeasureFacts {
  std::vector<analytic::WeightedSkySample> components;
  // components[i].source_token indexes this table. Actual shapes/poses/source
  // are replayed through run, which owns the snapshot, seed AND spectrum request.
  // A source token is local to this measure, not a cross-run identifier.
  std::vector<DiagnosticSourceIdentity> sources;
  uint64_t completed_samples = 0;
  uint64_t optical_evaluations = 0;
  bool budget_exhausted = false;
};


enum class DiagnosticEvidence { kActual, kCandidate, kUnfinished };

enum class DiagnosticGeometry { kPoint, kPolyline, kAtom, kSourceRange, kBand };

struct PairedInterfaceEvidence {
  std::array<double, 3> actual_xyz{};
  std::array<double, 3> without_slot_xyz{};
  std::array<double, 2> xy_difference{};
  bool chromaticity_available = false;
  bool complete = false;
};

struct DiagnosticFeatureRecord {
  DiagnosticEvidence evidence = DiagnosticEvidence::kUnfinished;
  DiagnosticGeometry geometry = DiagnosticGeometry::kPoint;
  std::string kind;
  std::string reason;
  std::vector<std::array<double, 3>> sky_points;
  std::vector<analytic::FieldStationaryPoint> field_points;
  std::optional<analytic::FieldBand> band;
  analytic::FieldEquation equation = analytic::FieldEquation::kLogYPeak;
  double level = 0;
  double bandwidth_rad = 0;
  double prefix_movement_rad = 0;
  std::optional<double> replicate_movement_rad;
  std::optional<double> scale_movement_rad;
  analytic::FieldSolveStatus scale_status = analytic::FieldSolveStatus::kInvalidInput;
  double minimum_effective_samples = 0;
  double transverse_contrast = 0;
  analytic::FieldWalkStop walk_stop = analytic::FieldWalkStop::kInvalidInput;
  // Local source token into result.measure; not a unique-cause assertion.
  std::optional<uint64_t> source_token;
  std::optional<double> contributor_fraction_of_estimated_y;
  int internal_slot = -1;
  std::vector<std::pair<std::string, double>> source_parameters;
  std::optional<analytic::DiagnosticInputRow> boundary_source;
  std::optional<analytic::DiagnosticOutputRow> boundary_value;
  std::optional<analytic::InterfaceStationaryPoint> interface_event;
  std::optional<PairedInterfaceEvidence> paired_interface;
  std::vector<analytic::InterfaceCurve> interface_curves;
  std::optional<analytic::InterfaceEventBracket> source_event;
  std::optional<size_t> source_connected_feature;
  std::optional<analytic::DeviationStationaryPoint> deviation_minimum;
  std::optional<std::array<double, 3>> orbit_axis;
  double orbit_begin_rad = 0;
  double orbit_end_rad = 0;
  double observation_contrast_error = 0;
  std::array<double, 3> atom_xyz_mass{};
};

struct DiscoveryRecords {
  std::vector<DiagnosticFeatureRecord> features;
  std::vector<std::string> unfinished;
  std::vector<std::string> limitations;
  uint64_t event_path_evaluations = 0;
  uint64_t replicate_path_evaluations = 0;
  uint64_t replicate_samples = 0;
  uint64_t field_component_evaluations = 0;
  double assembly_seconds = 0;
  double event_seconds = 0;
  double field_seconds = 0;
  bool budget_exhausted = false;
};

struct PositiveSourceWitness {
  uint64_t token = 0;
  DiagnosticSourceIdentity source;
  std::optional<analytic::DiagnosticInputRow> coordinates;
};
struct CompletedDiscovery : DiscoveryRecords {
  DiagnosticMeasureFacts measure;
  std::vector<PositiveSourceWitness> positive_sources;
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_RESULTS_REPORT_RECORDS_HPP_
