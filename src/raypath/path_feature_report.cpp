#include "raypath/path_feature_report.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <utility>

#include "analytic/discovery.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/optics.hpp"
#include "raypath/feature_discovery_adapter.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/illuminant.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {

const char* CoverageStatusName(CoverageStatus status) {
  switch (status) {
    case CoverageStatus::kSupported:
      return "supported";
    case CoverageStatus::kNotSupported:
      return "not_supported";
    case CoverageStatus::kNotDetectedAtResolution:
      return "not_detected_at_resolution";
    case CoverageStatus::kNumericalIncomplete:
      return "numerical_incomplete";
    case CoverageStatus::kPhysicallyUnreachable:
      return "physically_unreachable";
  }
  return "not_supported";
}

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad2Deg = 180.0 / kPi;
constexpr double kReferenceRedNm = 694.3628981235904;   // IceRefractiveIndex::Get == 1.307
constexpr double kReferenceBlueNm = 430.0197374077313;  // IceRefractiveIndex::Get == 1.317
constexpr int kMinimumSampleCount = 64;
constexpr double kRelativeConvergenceTolerance = 0.25;

bool ExceedsSampleEvaluationBudget(size_t member_count, size_t wavelength_count, int sample_count) {
  if (member_count == 0 || wavelength_count == 0) {
    return false;
  }
  const uint64_t fine_and_coarse = static_cast<uint64_t>(sample_count) + static_cast<uint64_t>(sample_count / 2);
  const uint64_t max = kMaxFeatureReportSampleEvaluations;
  return member_count > max || wavelength_count > max / member_count ||
         fine_and_coarse > max / (static_cast<uint64_t>(member_count) * wavelength_count);
}

std::vector<std::vector<int>> ExpandPhysicalMembers(const CrystalConfig& crystal, const std::vector<int>& faces) {
  const GeometricSymmetry shape =
      std::visit([](const auto& param) { return DeriveGeometricSymmetry(param); }, crystal.param_);
  const SymmetryGating gating = DeriveSymmetryGating(SymmetrySemantics::kPhysical, shape, crystal.axis_);
  const auto d = detail::DeriveDSymmetryParams(crystal.axis_);
  std::vector<IdType> path;
  path.reserve(faces.size());
  for (int face : faces) {
    path.push_back(static_cast<IdType>(face));
  }
  const uint8_t symmetry = kSymmetryPrism | kSymmetryBasal | kSymmetryDirection;
  std::vector<std::vector<int>> out;
  for (const auto& member : ExpandRaypathByPeriod(path, symmetry, d.sigma_a, d.d_applicable, gating.p_applicable,
                                                  gating.b_applicable, kHexagonalFnPeriod, gating.geom)) {
    std::vector<int> converted;
    converted.reserve(member.size());
    for (IdType face : member) {
      converted.push_back(static_cast<int>(face));
    }
    if (std::find(out.begin(), out.end(), converted) == out.end()) {
      out.push_back(std::move(converted));
    }
  }
  if (out.empty()) {
    out.push_back(faces);
  }
  return out;
}

double Clamp1(double x) {
  return std::max(-1.0, std::min(1.0, x));
}

double AngleBetween(const double a[3], const double b[3]) {
  return std::acos(Clamp1(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]));
}

double DirectionDifference(const double a[3], const double b[3]) {
  const double x = a[0] - b[0];
  const double y = a[1] - b[1];
  const double z = a[2] - b[2];
  return std::sqrt(x * x + y * y + z * z);
}

double WrapDeg(double angle) {
  angle = std::remainder(angle, 360.0);
  return angle <= -180.0 ? angle + 360.0 : angle;
}

bool SameFaces(const std::vector<int>& faces, std::initializer_list<int> expected) {
  return faces.size() == expected.size() && std::equal(faces.begin(), faces.end(), expected.begin());
}

bool IsRegularPrism(const CrystalConversion& crystal) {
  if (crystal.shape.kind != analytic::CrystalShapeKind::kPrism) {
    return false;
  }
  for (int i = 1; i < 6; i++) {
    if (std::fabs(crystal.shape.face_distance[i] - crystal.shape.face_distance[0]) > 1e-6) {
      return false;
    }
  }
  return true;
}

bool IsReferenceRegularPrism(const CrystalConversion& crystal) {
  if (!IsRegularPrism(crystal) || crystal.shape_is_nominal || std::fabs(crystal.shape.height - 1.0) > 1e-6) {
    return false;
  }
  return std::all_of(std::begin(crystal.shape.face_distance), std::end(crystal.shape.face_distance),
                     [](double distance) { return std::fabs(distance - 1.0) <= 1e-6; });
}

bool IsReferenceRhombicPlate(const CrystalConversion& crystal, const FeatureReportMetadata& meta) {
  constexpr double kDistances[6] = { 1.5, 1.0, 1.0, 1.5, 1.0, 1.0 };
  if (crystal.shape.kind != analytic::CrystalShapeKind::kPrism || crystal.shape_is_nominal ||
      std::fabs(crystal.shape.height - 1.0) > 1e-6 || std::fabs(meta.sun_altitude_deg - 9.0) > 1e-6 ||
      std::fabs(WrapDeg(meta.sun_azimuth_deg - 180.0)) > 1e-6) {
    return false;
  }
  for (int i = 0; i < 6; i++) {
    if (std::fabs(crystal.shape.face_distance[i] - kDistances[i]) > 1e-6) {
      return false;
    }
  }
  return SameFaces(meta.requested_faces, { 1, 3, 4, 2 }) || SameFaces(meta.requested_faces, { 1, 3, 5, 2 });
}

bool IsExactHorizontalFamily(const AxisDistribution& axis) {
  return axis.latitude_dist.type == DistributionType::kNoRandom &&
         std::fabs(axis.latitude_dist.center - 90.0f) < 1e-6f && axis.IsAzRotationallySymmetric() &&
         axis.roll_dist.type == DistributionType::kNoRandom;
}

std::vector<ReportWavelength> ResolveWavelengths(const LightSourceConfig& light,
                                                 const PathFeatureReportRequest& request, Error* error) {
  std::vector<double> wavelengths = request.wavelengths_nm;
  std::vector<double> weights = request.wavelength_weights;
  if (wavelengths.empty() && request.scene_spectrum_source == SceneSpectrumSource::kLegacyReferenceEndpoints) {
    wavelengths = { kReferenceRedNm, kReferenceBlueNm };
    weights = { 1.0, 1.0 };
  }
  if (wavelengths.empty() && request.scene_spectrum_source == SceneSpectrumSource::kDiagnostic) {
    *error = { ErrorCode::kInvalidArgument, "diagnostic spectrum requires at least one wavelength" };
    return {};
  }
  if (wavelengths.empty() && request.scene_spectrum_source == SceneSpectrumSource::kScene) {
    if (const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_); discrete != nullptr) {
      wavelengths.reserve(discrete->size());
      weights.reserve(discrete->size());
      for (const WlParam& node : *discrete) {
        wavelengths.push_back(node.wl_);
        weights.push_back(node.weight_);
      }
    } else {
      if (request.illuminant_node_count < 1 || request.illuminant_node_count > 256) {
        *error = { ErrorCode::kInvalidArgument, "illuminant_node_count must be in [1, 256]" };
        return {};
      }
      const IlluminantType illuminant = std::get<IlluminantType>(light.spectrum_);
      wavelengths.reserve(static_cast<size_t>(request.illuminant_node_count));
      weights.reserve(static_cast<size_t>(request.illuminant_node_count));
      for (int i = 0; i < request.illuminant_node_count; i++) {
        const double wavelength = 380.0 + (static_cast<double>(i) + 0.5) * 400.0 / request.illuminant_node_count;
        wavelengths.push_back(wavelength);
        weights.push_back(GetIlluminantSpd(illuminant, static_cast<float>(wavelength)) / request.illuminant_node_count);
      }
    }
  }
  if (wavelengths.empty()) {
    *error = { ErrorCode::kInvalidArgument, "scene spectrum has no wavelength nodes" };
    return {};
  }
  if (weights.empty()) {
    weights.assign(wavelengths.size(), 1.0);
  }
  if (weights.size() != wavelengths.size()) {
    *error = { ErrorCode::kInvalidArgument, "wavelength_weights must be empty or match wavelengths_nm" };
    return {};
  }
  std::vector<ReportWavelength> out;
  out.reserve(wavelengths.size());
  for (size_t i = 0; i < wavelengths.size(); i++) {
    if (!std::isfinite(weights[i]) || weights[i] < 0.0) {
      *error = { ErrorCode::kInvalidArgument, "wavelength weights must be finite and non-negative" };
      return {};
    }
    WavelengthChoice choice;
    if (const Error e = ResolveWavelength(light, wavelengths[i], &choice); !e.Ok()) {
      *error = e;
      return {};
    }
    out.push_back({ choice.wavelength_nm, weights[i], choice.refractive_index });
  }
  return out;
}

struct SampleSummary {
  int valid_count = 0;
  int positive_count = 0;
  double mean_at = 0.0;
  bool has_direction = false;
  double direction[3]{};
  double direction_residual_max = 0.0;
};

SampleSummary SampleRandom(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                           const std::vector<int>& slots, double refractive_index, const double incident[3], int n) {
  analytic::IceDiscovery discovery(normals, polygons, slots.data(), static_cast<int>(slots.size()), refractive_index,
                                   incident);
  SampleSummary out;
  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    analytic::BandEvent event;
    double transmission = 0.0;
    if (!discovery.EvaluateLatticePoint(n, i, &event, &transmission)) {
      continue;
    }
    out.valid_count++;
    const double area = discovery.EntryMeasureAt(event);
    const double at = analytic::kLiAreaPerEngineArea * area * transmission;
    if (at > 0.0) {
      out.positive_count++;
      sum += at;
    }
  }
  out.mean_at = sum / static_cast<double>(n);
  return out;
}

void Rz(double theta, double pose[9]) {
  const double c = std::cos(theta);
  const double s = std::sin(theta);
  const double value[9] = { c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0 };
  std::copy(value, value + 9, pose);
}

SampleSummary SampleHorizontal(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                               const std::vector<int>& slots, double refractive_index, const double incident[3],
                               int n) {
  analytic::Corridor corridor(normals, polygons, slots.data(), static_cast<int>(slots.size()));
  std::vector<double> segments(3 * (slots.size() + 1));
  std::vector<double> transmittances(slots.size());
  SampleSummary out;
  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    const double theta = 2.0 * kPi * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
    double pose[9];
    Rz(theta, pose);
    analytic::PathOutputs detail{};
    detail.segment_directions = segments.data();
    detail.interface_transmittances = transmittances.data();
    if (!analytic::EvaluatePath(normals, slots.data(), static_cast<int>(slots.size()), refractive_index, incident, pose,
                                &detail)) {
      continue;
    }
    out.valid_count++;
    double incident_body[3]{};
    for (int k = 0; k < 3; k++) {
      incident_body[k] = pose[0 * 3 + k] * incident[0] + pose[1 * 3 + k] * incident[1] + pose[2 * 3 + k] * incident[2];
    }
    const double area = corridor.Evaluate(incident_body, refractive_index).value;
    const double at = analytic::kLiAreaPerEngineArea * area * detail.fresnel_transmission;
    if (at > 0.0) {
      out.positive_count++;
      sum += at;
      if (!out.has_direction) {
        out.has_direction = true;
        std::copy(detail.outgoing_direction, detail.outgoing_direction + 3, out.direction);
      } else {
        out.direction_residual_max =
            std::max(out.direction_residual_max, DirectionDifference(out.direction, detail.outgoing_direction));
      }
    }
  }
  out.mean_at = sum / static_cast<double>(n);
  return out;
}

BrightnessEstimate MeasureMember(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                                 const std::vector<int>& slots, const ReportWavelength& wavelength,
                                 const double incident[3], int sample_count, bool random, bool horizontal) {
  BrightnessEstimate out;
  out.coarse_sample_count = sample_count / 2;
  out.fine_sample_count = sample_count;
  SampleSummary coarse;
  SampleSummary fine;
  if (random) {
    out.measure = "normalised Haar on SO(3)";
    coarse = SampleRandom(normals, polygons, slots, wavelength.refractive_index, incident, out.coarse_sample_count);
    fine = SampleRandom(normals, polygons, slots, wavelength.refractive_index, incident, out.fine_sample_count);
  } else if (horizontal) {
    out.measure = "Rz(theta), theta uniform under dtheta/(2*pi); c axis exactly vertical";
    coarse = SampleHorizontal(normals, polygons, slots, wavelength.refractive_index, incident, out.coarse_sample_count);
    fine = SampleHorizontal(normals, polygons, slots, wavelength.refractive_index, incident, out.fine_sample_count);
  } else {
    out.status = CoverageStatus::kNotSupported;
    out.reason = "brightness integration currently supports only the random Haar and exact horizontal Rz families";
    return out;
  }
  out.fine_valid_count = fine.valid_count;
  out.fine_positive_count = fine.positive_count;
  out.coarse_mean_at = coarse.mean_at;
  out.fine_mean_at = fine.mean_at;
  out.absolute_difference = std::fabs(fine.mean_at - coarse.mean_at);
  out.weighted_mean_at = wavelength.weight * fine.mean_at;
  out.has_fixed_direction = fine.has_direction && fine.direction_residual_max <= 1e-10;
  std::copy(fine.direction, fine.direction + 3, out.fixed_direction);
  out.direction_residual_max = fine.direction_residual_max;
  if (fine.positive_count == 0) {
    out.status = CoverageStatus::kNotDetectedAtResolution;
    out.reason = "no positive finite-crystal A*T sample was found at the requested resolution";
  } else if (out.absolute_difference >
             kRelativeConvergenceTolerance * std::max(std::fabs(fine.mean_at), std::fabs(coarse.mean_at))) {
    out.status = CoverageStatus::kNumericalIncomplete;
    out.reason = "coarse and fine finite-crystal A*T estimates did not satisfy the convergence tolerance";
  } else {
    out.status = CoverageStatus::kSupported;
  }
  return out;
}

CoverageStatus AggregateFeatureEvidence(const PathFeatureReport& report) {
  bool has_not_supported = false;
  bool has_not_detected = false;
  bool has_numerical_incomplete = false;
  for (const PhysicalMemberReport& member : report.members) {
    for (const MemberWavelengthReport& row : member.wavelengths) {
      switch (row.brightness.status) {
        case CoverageStatus::kSupported:
          break;
        case CoverageStatus::kNotSupported:
          has_not_supported = true;
          break;
        case CoverageStatus::kNotDetectedAtResolution:
          has_not_detected = true;
          break;
        case CoverageStatus::kNumericalIncomplete:
          has_numerical_incomplete = true;
          break;
        case CoverageStatus::kPhysicallyUnreachable:
          return CoverageStatus::kPhysicallyUnreachable;
      }
    }
  }
  if (has_numerical_incomplete) {
    return CoverageStatus::kNumericalIncomplete;
  }
  if (has_not_detected) {
    return CoverageStatus::kNotDetectedAtResolution;
  }
  if (has_not_supported) {
    return CoverageStatus::kNotSupported;
  }
  return CoverageStatus::kSupported;
}

CoverageStatus MeasureCoverage(SceneMeasureStatus status) {
  switch (status) {
    case SceneMeasureStatus::kConfirmed:
    case SceneMeasureStatus::kZeroWeight:
      return CoverageStatus::kSupported;
    case SceneMeasureStatus::kPhysicallyUnreachable:
      return CoverageStatus::kPhysicallyUnreachable;
    case SceneMeasureStatus::kNumericalIncomplete:
      return CoverageStatus::kNumericalIncomplete;
    case SceneMeasureStatus::kNotSupported:
      return CoverageStatus::kNotSupported;
  }
  return CoverageStatus::kNotSupported;
}

CoverageStatus DiscoveryCoverage(analytic::FeatureEvidenceStatus status) {
  switch (status) {
    case analytic::FeatureEvidenceStatus::kConfirmed:
    case analytic::FeatureEvidenceStatus::kCandidate:
      return CoverageStatus::kSupported;
    case analytic::FeatureEvidenceStatus::kNotDetectedAtResolution:
      return CoverageStatus::kNotDetectedAtResolution;
    case analytic::FeatureEvidenceStatus::kNumericalIncomplete:
      return CoverageStatus::kNumericalIncomplete;
    case analytic::FeatureEvidenceStatus::kPhysicallyUnreachable:
      return CoverageStatus::kPhysicallyUnreachable;
    case analytic::FeatureEvidenceStatus::kNotSupported:
      return CoverageStatus::kNotSupported;
  }
  return CoverageStatus::kNotSupported;
}

const SpectrumMeasureNode* SpectrumNode(const SceneMeasureResult& measure, int node_id) {
  const auto found = std::find_if(measure.spectrum_nodes.begin(), measure.spectrum_nodes.end(),
                                  [node_id](const SpectrumMeasureNode& node) { return node.node_id == node_id; });
  return found == measure.spectrum_nodes.end() ? nullptr : &*found;
}

void ProjectGeneralDiscovery(PathFeatureReport* report) {
  report->wavelengths.reserve(report->scene_measure.spectrum_nodes.size());
  for (const SpectrumMeasureNode& node : report->scene_measure.spectrum_nodes) {
    report->wavelengths.push_back({ node.wavelength_nm, node.weight, node.refractive_index });
  }
  report->coverage.push_back({ "general_feature_discovery", CoverageStatus::kSupported,
                               "all mechanism records come from the support-driven analytic kernel" });
  for (const analytic::FeatureMechanismRecord& mechanism : report->discovery.mechanisms) {
    report->coverage.push_back(
        { "feature_mechanism." + std::string(analytic::FeatureMechanismName(mechanism.mechanism)),
          DiscoveryCoverage(mechanism.status), mechanism.reason });
  }
  for (size_t index = 0; index < report->discovery.candidates.size(); ++index) {
    const analytic::FeatureCandidate& candidate = report->discovery.candidates[index];
    PathFeature feature;
    feature.id =
        "general." + std::string(analytic::FeatureMechanismName(candidate.mechanism)) + "." + std::to_string(index);
    feature.kind = "feature_candidate";
    feature.evidence_status = analytic::FeatureEvidenceStatusName(candidate.status);
    feature.mechanism = analytic::FeatureMechanismName(candidate.mechanism);
    feature.location = "computed sky position";
    feature.interpretation = candidate.reason;
    FeaturePosition position;
    position.wavelength_nm = std::numeric_limits<double>::quiet_NaN();
    position.refractive_index = std::numeric_limits<double>::quiet_NaN();
    if (const SpectrumMeasureNode* spectrum =
            SpectrumNode(report->scene_measure, candidate.provenance.spectrum_node_id);
        spectrum != nullptr) {
      position.wavelength_nm = spectrum->wavelength_nm;
      position.refractive_index = spectrum->refractive_index;
    }
    double altitude = 0.0;
    double azimuth = 0.0;
    DirToAltAz(candidate.direction, &altitude, &azimuth);
    position.altitude_deg = altitude;
    position.azimuth_deg = azimuth;
    position.relative_solar_azimuth_deg = WrapDeg(azimuth - report->meta.sun_azimuth_deg);
    position.spherical_separation_deg = AngleBetween(report->meta.incident_direction, candidate.direction) * kRad2Deg;
    feature.positions.push_back(position);
    feature.metrics = {
      { "support_dimension", static_cast<double>(candidate.support_dimension) },
      { "mapping_rank", static_cast<double>(candidate.mapping_rank) },
      { "singular_value_0", candidate.singular_values[0] },
      { "singular_value_1", candidate.singular_values[1] },
      { "weighted_mass", candidate.weighted_mass },
      { "residual", candidate.residual },
      { "resolution", candidate.resolution },
    };
    report->features.push_back(std::move(feature));
  }
}

bool HasDistinctRefractiveIndices(const std::vector<ReportWavelength>& wavelengths) {
  for (size_t first = 0; first < wavelengths.size(); first++) {
    for (size_t second = first + 1; second < wavelengths.size(); second++) {
      if (std::fabs(wavelengths[first].refractive_index - wavelengths[second].refractive_index) > 1e-12) {
        return true;
      }
    }
  }
  return false;
}

PathFeature OrdinaryEdge(const std::vector<ReportWavelength>& wavelengths, const std::string& id,
                         const std::string& interpretation) {
  PathFeature feature;
  feature.id = id;
  feature.kind = "dispersion_edge";
  feature.evidence_status = "confirmed";
  feature.mechanism = "ordinary minimum-deviation dispersion";
  feature.location = "solar side";
  feature.interpretation = interpretation;
  for (const ReportWavelength& wavelength : wavelengths) {
    FeaturePosition position;
    position.wavelength_nm = wavelength.wavelength_nm;
    position.refractive_index = wavelength.refractive_index;
    position.deviation_deg = (2.0 * std::asin(0.5 * wavelength.refractive_index) - kPi / 3.0) * kRad2Deg;
    feature.positions.push_back(position);
  }
  return feature;
}

struct KinkSample {
  double deviation[2]{};
  double at[2]{};
  double without_internal_r[2]{};
};

bool EvaluateKinkPoint(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                       const std::vector<int>& slots, const ReportWavelength wavelengths[2], const double u[3],
                       KinkSample* sample) {
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  for (int color = 0; color < 2; color++) {
    std::vector<double> segments(3 * (slots.size() + 1));
    std::vector<double> transmittances(slots.size());
    analytic::PathOutputs detail{};
    detail.segment_directions = segments.data();
    detail.interface_transmittances = transmittances.data();
    if (!analytic::EvaluatePath(normals, slots.data(), static_cast<int>(slots.size()),
                                wavelengths[color].refractive_index, incident, identity, &detail)) {
      return false;
    }
    analytic::Corridor corridor(normals, polygons, slots.data(), static_cast<int>(slots.size()));
    const double area = corridor.Evaluate(incident, wavelengths[color].refractive_index).value;
    if (!(area > 0.0) || !(transmittances[1] > 0.0)) {
      return false;
    }
    sample->deviation[color] = AngleBetween(detail.outgoing_direction, incident) * kRad2Deg;
    sample->at[color] = analytic::kLiAreaPerEngineArea * area * detail.fresnel_transmission;
    sample->without_internal_r[color] = sample->at[color] / transmittances[1];
  }
  return sample->deviation[1] > 90.0;
}

bool InternalDiscriminant(const analytic::FaceNormalTable& normals, const std::vector<int>& slots,
                          double refractive_index, const double u[3], double* discriminant) {
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  std::vector<double> margins(slots.size() + 2);
  analytic::ChainDomain domain;
  domain.margins = margins.data();
  double outgoing[3]{};
  analytic::TracePathChain<double>(normals, slots.data(), static_cast<int>(slots.size()), refractive_index, incident,
                                   identity, outgoing, nullptr, &domain);
  if (domain.margin_count < 3 || !(margins[2] > 0.0)) {
    return false;
  }
  const double cosine = margins[2];
  *discriminant = 1.0 - refractive_index * refractive_index * (1.0 - cosine * cosine);
  return true;
}

void Normalize(double u[3]) {
  const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  u[0] /= norm;
  u[1] /= norm;
  u[2] /= norm;
}

std::vector<std::array<double, 3>> WalkInternalTirBoundary(const analytic::FaceNormalTable& normals,
                                                           const std::vector<int>& slots, double refractive_index,
                                                           int sample_count) {
  const int rows = std::max(16, static_cast<int>(std::sqrt(2.0 * static_cast<double>(sample_count))));
  const int cols = 2 * rows;
  const size_t count = static_cast<size_t>(rows) * static_cast<size_t>(cols);
  std::vector<std::array<double, 3>> points(count);
  std::vector<double> discriminants(count, std::numeric_limits<double>::quiet_NaN());
  std::vector<uint8_t> valid(count, 0);
  for (int i = 0; i < rows; i++) {
    const double latitude = -0.5 * kPi + (static_cast<double>(i) + 0.5) * kPi / rows;
    for (int j = 0; j < cols; j++) {
      const double longitude = -kPi + (static_cast<double>(j) + 0.5) * 2.0 * kPi / cols;
      const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(cols) + static_cast<size_t>(j);
      points[cell] = { std::cos(latitude) * std::cos(longitude), std::cos(latitude) * std::sin(longitude),
                       std::sin(latitude) };
      valid[cell] = InternalDiscriminant(normals, slots, refractive_index, points[cell].data(), &discriminants[cell]);
    }
  }
  std::vector<std::array<double, 3>> roots;
  auto edge = [&](size_t a, size_t b) {
    if (!valid[a] || !valid[b] || std::signbit(discriminants[a]) == std::signbit(discriminants[b])) {
      return;
    }
    std::array<double, 3> lo = points[a];
    std::array<double, 3> hi = points[b];
    double lo_discriminant = discriminants[a];
    for (int step = 0; step < 40; step++) {
      std::array<double, 3> mid = { lo[0] + hi[0], lo[1] + hi[1], lo[2] + hi[2] };
      Normalize(mid.data());
      double mid_discriminant = 0.0;
      if (!InternalDiscriminant(normals, slots, refractive_index, mid.data(), &mid_discriminant)) {
        return;
      }
      if (std::signbit(mid_discriminant) == std::signbit(lo_discriminant)) {
        lo = mid;
        lo_discriminant = mid_discriminant;
      } else {
        hi = mid;
      }
    }
    std::array<double, 3> root = { lo[0] + hi[0], lo[1] + hi[1], lo[2] + hi[2] };
    Normalize(root.data());
    roots.push_back(root);
  };
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(cols) + static_cast<size_t>(j);
      edge(cell, static_cast<size_t>(i) * static_cast<size_t>(cols) + static_cast<size_t>((j + 1) % cols));
      if (i + 1 < rows) {
        edge(cell, static_cast<size_t>(i + 1) * static_cast<size_t>(cols) + static_cast<size_t>(j));
      }
    }
  }
  return roots;
}

double Median(std::vector<double> values) {
  if (values.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const size_t middle = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
  return values[middle];
}

std::optional<PathFeature> TirFeature(const analytic::FaceNormalTable& normals,
                                      const analytic::FacePolygonTable& polygons, const std::vector<int>& slots,
                                      const std::vector<ReportWavelength>& all_wavelengths, int sample_count) {
  if (slots.size() != 3 || all_wavelengths.size() < 2) {
    return std::nullopt;
  }
  std::array<ReportWavelength, 2> wavelengths = { all_wavelengths.front(), all_wavelengths.front() };
  for (const ReportWavelength& wavelength : all_wavelengths) {
    if (wavelength.refractive_index < wavelengths[0].refractive_index) {
      wavelengths[0] = wavelength;
    }
    if (wavelength.refractive_index > wavelengths[1].refractive_index) {
      wavelengths[1] = wavelength;
    }
  }
  const auto red_roots = WalkInternalTirBoundary(normals, slots, wavelengths[0].refractive_index, sample_count);
  const auto blue_roots = WalkInternalTirBoundary(normals, slots, wavelengths[1].refractive_index, sample_count);
  std::vector<KinkSample> candidates;
  candidates.reserve(blue_roots.size());
  for (const auto& u : blue_roots) {
    KinkSample sample;
    if (EvaluateKinkPoint(normals, polygons, slots, wavelengths.data(), u.data(), &sample)) {
      candidates.push_back(sample);
    }
  }
  std::vector<double> red_deviations;
  red_deviations.reserve(red_roots.size());
  for (const auto& u : red_roots) {
    KinkSample sample;
    if (EvaluateKinkPoint(normals, polygons, slots, wavelengths.data(), u.data(), &sample) &&
        sample.deviation[0] > 90.0) {
      red_deviations.push_back(sample.deviation[0]);
    }
  }
  if (candidates.empty() || red_deviations.empty()) {
    return std::nullopt;
  }
  const size_t count = std::min<size_t>(200, candidates.size());
  double production[2]{};
  double counterfactual[2]{};
  for (size_t i = 0; i < count; i++) {
    for (int color = 0; color < 2; color++) {
      const size_t index = i * candidates.size() / count;
      production[color] += candidates[index].at[color];
      counterfactual[color] += candidates[index].without_internal_r[color];
    }
  }
  if (!(production[0] > 0.0) || !(counterfactual[0] > 0.0)) {
    return std::nullopt;
  }
  PathFeature feature;
  feature.id = "random_regular.3-1-5.antisolar_tir_blue_band";
  feature.kind = "edge";
  feature.evidence_status = "confirmed";
  feature.mechanism = "internal-reflection Fresnel TIR kink";
  feature.location = "antisolar side";
  feature.interpretation =
      "the same sampled poses and finite-crystal geometry are compared with only the internal reflectance removed";
  feature.visible = true;
  std::vector<double> blue_deviations;
  blue_deviations.reserve(candidates.size());
  for (const KinkSample& sample : candidates) {
    blue_deviations.push_back(sample.deviation[1]);
  }
  for (int color = 0; color < 2; color++) {
    FeaturePosition position;
    position.wavelength_nm = wavelengths[color].wavelength_nm;
    position.refractive_index = wavelengths[color].refractive_index;
    position.deviation_deg = color == 0 ? Median(red_deviations) : Median(blue_deviations);
    feature.positions.push_back(position);
  }
  feature.metrics.push_back({ "production_blue_red_ratio", production[1] / production[0] });
  feature.metrics.push_back({ "without_internal_R_blue_red_ratio", counterfactual[1] / counterfactual[0] });
  feature.metrics.push_back({ "sample_count", static_cast<double>(count) });
  feature.metrics.push_back({ "boundary_curve_points", static_cast<double>(candidates.size()) });
  return feature;
}

void AddPlateFeatures(PathFeatureReport* report) {
  struct LocatedMember {
    double altitude = 0.0;
    double azimuth = 0.0;
    size_t member = 0;
  };
  std::vector<LocatedMember> located;
  for (size_t m = 0; m < report->members.size(); m++) {
    const auto& rows = report->members[m].wavelengths;
    const bool all_wavelengths_located =
        rows.size() == report->wavelengths.size() &&
        std::all_of(rows.begin(), rows.end(), [](const MemberWavelengthReport& row) {
          return row.brightness.status == CoverageStatus::kSupported && row.brightness.has_fixed_direction;
        });
    if (all_wavelengths_located) {
      double altitude = 0.0;
      double azimuth = 0.0;
      DirToAltAz(rows.front().brightness.fixed_direction, &altitude, &azimuth);
      located.push_back({ altitude, azimuth, m });
    }
  }
  std::vector<std::vector<LocatedMember>> groups;
  for (const LocatedMember& member : located) {
    auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& existing) {
      return std::fabs(existing.front().altitude - member.altitude) < 1e-8 &&
             std::fabs(WrapDeg(existing.front().azimuth - member.azimuth)) < 1e-8;
    });
    if (group == groups.end()) {
      groups.push_back({ member });
    } else {
      group->push_back(member);
    }
  }
  for (size_t g = 0; g < groups.size(); g++) {
    const LocatedMember& first = groups[g].front();
    PathFeature feature;
    feature.id = "horizontal_physical_l2.location_" + std::to_string(g);
    feature.kind = "constant_direction_lobe";
    feature.evidence_status = "confirmed";
    feature.mechanism = "constant-direction horizontal-family branch with finite-crystal A*T support";
    feature.location = "computed sky position";
    feature.interpretation =
        "relative solar azimuth and spherical separation are different quantities and are both reported";
    for (size_t w = 0; w < report->wavelengths.size(); w++) {
      FeaturePosition position;
      position.wavelength_nm = report->wavelengths[w].wavelength_nm;
      position.refractive_index = report->wavelengths[w].refractive_index;
      const auto& brightness = report->members[first.member].wavelengths[w].brightness;
      double altitude = 0.0;
      double azimuth = 0.0;
      DirToAltAz(brightness.fixed_direction, &altitude, &azimuth);
      position.altitude_deg = altitude;
      position.azimuth_deg = azimuth;
      position.relative_solar_azimuth_deg = WrapDeg(azimuth - report->meta.sun_azimuth_deg);
      position.spherical_separation_deg =
          AngleBetween(report->meta.incident_direction, brightness.fixed_direction) * kRad2Deg;
      feature.positions.push_back(position);
      double energy = 0.0;
      for (const LocatedMember& member : groups[g]) {
        energy += report->members[member.member].wavelengths[w].brightness.fine_mean_at;
      }
      feature.metrics.push_back({ "member_sum_energy_wavelength_" + std::to_string(w), energy });
    }
    feature.metrics.push_back({ "physical_member_count", static_cast<double>(groups[g].size()) });
    report->features.push_back(std::move(feature));
  }
}

}  // namespace

Error AnalyzePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                               PathFeatureReport* out) {
  *out = PathFeatureReport{};
  if (request.schema_version != 1 && request.schema_version != kFeatureReportSchemaVersion) {
    return { ErrorCode::kInvalidArgument, "unsupported path feature report schema version" };
  }
  const bool legacy_schema = request.schema_version == 1;
  if (request.sample_count < kMinimumSampleCount || request.sample_count > kMaxFeatureReportSampleCount ||
      request.sample_count % 2 != 0) {
    return { ErrorCode::kInvalidArgument, "sample_count must be an even integer in [" +
                                              std::to_string(kMinimumSampleCount) + ", " +
                                              std::to_string(kMaxFeatureReportSampleCount) + "]" };
  }
  if (request.wavelengths_nm.size() > static_cast<size_t>(kMaxFeatureReportWavelengthCount)) {
    return { ErrorCode::kInvalidArgument,
             "at most " + std::to_string(kMaxFeatureReportWavelengthCount) + " wavelengths may be requested" };
  }
  if (request.path_layers.empty()) {
    return { ErrorCode::kInvalidPath, "a path feature report requires at least one non-empty layer" };
  }
  if (legacy_schema && request.path_layers.size() != 1) {
    return { ErrorCode::kInvalidPath, "schema 1 path feature reports require exactly one path layer" };
  }
  std::vector<IdType> layer_crystal_ids = request.layer_crystal_ids;
  if (layer_crystal_ids.empty() && request.path_layers.size() == 1) {
    layer_crystal_ids.push_back(request.crystal_id);
  }
  if (layer_crystal_ids.size() != request.path_layers.size()) {
    return { ErrorCode::kInvalidPath,
             "layer_crystal_ids must identify every requested path layer (legacy crystal_id covers one layer)" };
  }
  const IdType primary_crystal_id = layer_crystal_ids.front();
  const auto crystal_it = config.crystals_.find(primary_crystal_id);
  if (crystal_it == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(primary_crystal_id) };
  }

  SceneMeasureResult scene_measure;
  analytic::FeatureSupportBatch feature_support;
  if (!legacy_schema) {
    SceneMeasureRequest measure_request;
    measure_request.layer_crystal_ids = layer_crystal_ids;
    measure_request.path_layers = request.path_layers;
    measure_request.member_selection = request.member_selection;
    measure_request.physical_member_mask = request.physical_member_mask;
    measure_request.physical_member_masks = request.physical_member_masks;
    measure_request.explicit_member_chains = request.explicit_member_chains;
    measure_request.spectrum_source =
        request.wavelengths_nm.empty() ? request.scene_spectrum_source : SceneSpectrumSource::kDiagnostic;
    measure_request.diagnostic_wavelengths_nm = request.wavelengths_nm;
    measure_request.diagnostic_wavelength_weights = request.wavelength_weights;
    measure_request.sample_count = request.scene_measure_sample_count;
    measure_request.sun_node_count = request.sun_node_count;
    measure_request.illuminant_node_count = request.illuminant_node_count;
    measure_request.seed = request.seed;
    if (const Error error = BuildFeatureSupportBatch(config, measure_request, &feature_support, &scene_measure);
        !error.Ok()) {
      return error;
    }
  }

  const CrystalConfig& crystal_config = crystal_it->second;
  const CrystalConversion crystal = ConvertCrystal(crystal_config.param_);
  PathFeatureReport result;
  result.scene_measure = std::move(scene_measure);
  result.meta.schema_version = request.schema_version;
  result.meta.analytic_api_version = analytic::kApiVersion;
  result.meta.crystal_id = primary_crystal_id;
  result.meta.layer_crystal_ids = layer_crystal_ids;
  result.meta.crystal_kind = crystal.kind;
  result.meta.shape = crystal.scalars;
  result.meta.shape_is_nominal = crystal.shape_is_nominal;
  result.meta.requested_faces = request.path_layers.front();
  result.meta.requested_path_layers = request.path_layers;
  result.meta.sun_altitude_deg = config.scene_.light_source_.param_.altitude_;
  result.meta.sun_azimuth_deg = config.scene_.light_source_.param_.azimuth_;
  SunIncidentDirection(config.scene_.light_source_.param_, result.meta.incident_direction);
  result.meta.sample_count = request.sample_count;
  if (!legacy_schema) {
    if (ExceedsSampleEvaluationBudget(result.scene_measure.member_chains.size(),
                                      result.scene_measure.spectrum_nodes.size(), request.sample_count)) {
      return { ErrorCode::kInvalidArgument, "physical-L2 members × wavelengths × (fine + coarse) exceeds the " +
                                                std::to_string(kMaxFeatureReportSampleEvaluations) +
                                                " sample-evaluation budget" };
    }
    result.coverage.push_back({ "actual_scene_measure", MeasureCoverage(result.scene_measure.status),
                                result.scene_measure.reason.empty() ?
                                    "actual shape, pose, sun, spectrum and selected physical members were assembled" :
                                    result.scene_measure.reason });
    analytic::FeatureDiscoveryOptions discovery_options;
    discovery_options.sky_merge_tolerance =
        std::sqrt(4.0 * kPi / (discovery_options.sky_z_bins * discovery_options.sky_azimuth_bins));
    result.discovery = analytic::DiscoverFeatures(feature_support, discovery_options);
    result.meta.orientation_measure = "actual configured scene measure; see scene_measure.factors";
    ProjectGeneralDiscovery(&result);
    result.limitations = {
      "candidate and confirmed are local numerical evidence, not unconditional global completeness",
      "finite solar-disc and spectral contributions are marginalized into the sky field; color causality and visual "
      "prominence are not classified",
      "constraint crossings without successful same-branch callback refinement remain candidates",
      "coarse/fine differences and boundary residuals are convergence evidence, not exact-error certificates",
    };
    *out = std::move(result);
    return {};
  }
  if (request.path_layers.front().size() == 1) {
    return { ErrorCode::kInvalidPath, "schema 1 path feature reports require 2 to 64 faces" };
  }

  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polygons;
  if (analytic::BuildFaceNormals(crystal.shape, &normals, &polygons) != analytic::Status::kOk) {
    return { ErrorCode::kCrystalRejected, "the nominal crystal is rejected by the closed-form geometry gate" };
  }
  std::vector<int> requested_slots;
  if (const Error e = ResolveSingleLayerPath(request.path_layers, normals, &requested_slots); !e.Ok()) {
    return e;
  }
  Error wavelength_error;
  std::vector<ReportWavelength> wavelengths =
      ResolveWavelengths(config.scene_.light_source_, request, &wavelength_error);
  if (!wavelength_error.Ok()) {
    return wavelength_error;
  }
  std::vector<std::vector<int>> members;
  if (legacy_schema) {
    members = ExpandPhysicalMembers(crystal_config, request.path_layers.front());
  } else {
    members.reserve(result.scene_measure.member_chains.size());
    for (const auto& chain : result.scene_measure.member_chains) {
      if (!chain.empty()) {
        members.push_back(chain.front());
      }
    }
  }
  if (ExceedsSampleEvaluationBudget(members.size(), wavelengths.size(), request.sample_count)) {
    return { ErrorCode::kInvalidArgument, "physical-L2 members × wavelengths × (fine + coarse) exceeds the " +
                                              std::to_string(kMaxFeatureReportSampleEvaluations) +
                                              " sample-evaluation budget" };
  }

  result.wavelengths = wavelengths;

  const bool random = crystal_config.axis_.IsFullSphereUniform();
  const bool horizontal = IsExactHorizontalFamily(crystal_config.axis_);
  result.meta.orientation_measure = random ? "normalised Haar on SO(3)" :
                                    horizontal ?
                                             "Rz(theta), theta uniform under dtheta/(2*pi); c axis exactly vertical" :
                                             "unsupported";
  result.coverage.push_back(
      { "orientation_measure", random || horizontal ? CoverageStatus::kSupported : CoverageStatus::kNotSupported,
        random || horizontal ? "the configured orientation family has an implemented measure" :
                               "only random Haar and exact horizontal Rz families are implemented" });
  result.coverage.push_back({ "physical_l2_expansion", CoverageStatus::kSupported,
                              "members use the crystal shape and orientation ensemble's physical P/B/D gating; no L1 "
                              "label equivalence is claimed" });

  for (const std::vector<int>& faces : members) {
    if (faces.size() == 1) {
      result.coverage.push_back(
          { "member " + std::to_string(result.members.size()), CoverageStatus::kNotSupported,
            "one-face external reflection is present in scene_measure but has no legacy positioned-feature fixture" });
      continue;
    }
    std::vector<int> slots;
    const Error path_error = ResolveSingleLayerPath({ faces }, normals, &slots);
    if (!path_error.Ok()) {
      result.coverage.push_back({ "member " + std::to_string(result.members.size()),
                                  CoverageStatus::kPhysicallyUnreachable, path_error.message });
      continue;
    }
    PhysicalMemberReport member;
    member.faces = faces;
    for (const ReportWavelength& wavelength : wavelengths) {
      member.wavelengths.push_back(
          { wavelength, MeasureMember(normals, polygons, slots, wavelength, result.meta.incident_direction,
                                      request.sample_count, random, horizontal) });
    }
    result.members.push_back(std::move(member));
  }

  const CoverageStatus feature_evidence = AggregateFeatureEvidence(result);
  if (feature_evidence != CoverageStatus::kSupported) {
    result.coverage.push_back({ "positioned_features", feature_evidence,
                                "member/wavelength finite-crystal evidence is insufficient for confirmed positioned "
                                "features at the requested resolution" });
  } else if (random && IsReferenceRegularPrism(crystal) && SameFaces(result.meta.requested_faces, { 3, 5 })) {
    result.features.push_back(
        OrdinaryEdge(wavelengths, "random_regular.3-5.inner_edge",
                     "finite jump at a non-degenerate minimum; not a divergent Jacobian caustic"));
    result.coverage.push_back({ "3-5 minimum-deviation edge", CoverageStatus::kSupported,
                                "closed-form prism deviation evaluated at every requested wavelength" });
  } else if (random && IsReferenceRegularPrism(crystal) && SameFaces(result.meta.requested_faces, { 3, 1, 5 })) {
    result.features.push_back(OrdinaryEdge(
        wavelengths, "random_regular.3-1-5.solar_dispersion_edge",
        "the path retains a solar-side red-to-blue dispersion edge, separate from the antisolar TIR band"));
    PathFeature caustic = OrdinaryEdge(wavelengths, "random_regular.3-1-5.solar_caustic_candidate",
                                       "boundary degeneracy alone is insufficient evidence of a visible caustic");
    caustic.kind = "caustic_candidate";
    caustic.evidence_status = "candidate";
    caustic.mechanism = "DPField boundary critical record";
    result.features.push_back(std::move(caustic));
    if (!HasDistinctRefractiveIndices(wavelengths)) {
      result.coverage.push_back({ "3-1-5 antisolar TIR band", CoverageStatus::kNotSupported,
                                  "the TIR blue-band counterfactual requires at least two distinct refractive-index "
                                  "wavelengths" });
    } else if (const auto tir = TirFeature(normals, polygons, requested_slots, wavelengths, request.sample_count);
               tir.has_value()) {
      result.features.push_back(*tir);
      result.coverage.push_back({ "3-1-5 antisolar TIR band", CoverageStatus::kSupported,
                                  "internal-reflectance counterfactual evaluated on the same sampled poses" });
    } else {
      result.coverage.push_back({ "3-1-5 antisolar TIR band", CoverageStatus::kNotDetectedAtResolution,
                                  "no finite-crystal sample near the internal TIR boundary survived all gates" });
    }
    PathFeature exit_gate;
    exit_gate.id = "random_regular.3-1-5.exit_gate";
    exit_gate.kind = "gate_edge";
    exit_gate.evidence_status = "confirmed";
    exit_gate.mechanism = "exit Snell gate";
    exit_gate.location = "moving exit gate";
    exit_gate.interpretation = "assessed as not visible: the direction spread dominates the red/blue gate displacement";
    exit_gate.visible = false;
    result.features.push_back(std::move(exit_gate));
    result.coverage.push_back({ "3-1-5 moving exit gate", CoverageStatus::kSupported,
                                "reported as a non-visible assessed gate, not collapsed into no feature" });
  } else if (horizontal && IsReferenceRhombicPlate(crystal, result.meta)) {
    AddPlateFeatures(&result);
    result.coverage.push_back(
        { "horizontal constant-direction locations",
          result.features.empty() ? CoverageStatus::kNotDetectedAtResolution : CoverageStatus::kSupported,
          result.features.empty() ? "no physical member had a stable fixed direction at the requested resolution" :
                                    "locations and energy are computed per physical member and wavelength" });
  } else {
    result.coverage.push_back({ "positioned_features", CoverageStatus::kNotSupported,
                                "this path/shape/orientation combination has no implemented fixture-backed detector" });
  }

  result.limitations = {
    "not an all-sky feature enumerator",
    "finite solar disc convolution and relative prominence against other paths are not evaluated",
    "open or multiple components, general oriented kink curves, cone-crystal empty results and rank-0 feature "
    "discovery are not covered",
    "coarse/fine differences and boundary residuals are convergence evidence, not exact-error certificates",
  };
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
