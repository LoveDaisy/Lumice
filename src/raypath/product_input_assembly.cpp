#include "raypath/product_input_assembly.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "analytic/entry_measure.hpp"
#include "analytic/so3.hpp"
#include "core/color_util.hpp"
#include "core/lat_lut.hpp"
#include "core/optics.hpp"
#include "core/shared/lat_path_selection.hpp"
#include "core/shared/pcg_shared.h"
#include "core/simulator.hpp"
#include "core/trace_ops.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/illuminant.hpp"

namespace lumice::raypath {
namespace {

Error PhysicalSpectralRow(float wl, double source_weight, double mass, SpectrumOrigin origin,
                          const std::string& provenance, SpectralRow* out) {
  if (!std::isfinite(wl) || wl < 350.f || wl > 900.f) {
    return { ErrorCode::kWavelengthOutOfRange, "wavelength outside ice refractive-index table" };
  }
  if (!std::isfinite(source_weight) || source_weight < 0 || !std::isfinite(mass) || mass < 0) {
    return { ErrorCode::kInvalidArgument, "spectral weights must be finite and nonnegative" };
  }
  SpectralRow row;
  row.wavelength_nm = wl;
  row.source_weight = source_weight;
  row.measure_mass = mass;
  row.refractive_index = IceRefractiveIndex::Get(wl);
  row.origin = origin;
  row.provenance = provenance;
  const float unit = 1.f;
  SpectrumToXyz(wl, &unit, nullptr, row.xyz_basis.data());
  for (int j = 0; j < 3; ++j)
    row.coefficient[j] = mass * source_weight * row.xyz_basis[j];
  *out = std::move(row);
  return {};
}

bool InContinuousBand(float wl) {
  return std::isfinite(wl) && wl >= 380.f && wl < 780.f;
}
bool UnitDraw(float u) {
  return std::isfinite(u) && u >= 0.f && u <= 1.f;
}

}  // namespace

Error AssembleDiscreteSpectrum(const LightSourceConfig& light, AssembledSpectrum* out) {
  *out = {};
  const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_);
  if (!discrete || discrete->empty())
    return { ErrorCode::kInvalidArgument, "a nonempty discrete spectrum is required" };
  AssembledSpectrum result;
  for (size_t i = 0; i < discrete->size(); ++i) {
    SpectralRow row;
    const auto& wl = (*discrete)[i];
    const auto error = PhysicalSpectralRow(wl.wl_, wl.weight_, 1, SpectrumOrigin::kDiscreteSum,
                                           "discrete slot " + std::to_string(i), &row);
    if (!error.Ok())
      return error;
    result.rows.push_back(std::move(row));
  }
  *out = std::move(result);
  return {};
}

Error AssembleSampledSpectrum(const LightSourceConfig& light, const ProductWavelengthSample& sample,
                              AssembledSpectrum* out) {
  *out = {};
  double weight = 0;
  if (sample.provenance.empty())
    return { ErrorCode::kInvalidArgument, "sample provenance is required" };
  if (const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_)) {
    if (!sample.discrete_slot || *sample.discrete_slot >= discrete->size() ||
        (*discrete)[*sample.discrete_slot].wl_ != sample.wavelength_nm) {
      return { ErrorCode::kInvalidArgument, "sample must identify its discrete source slot" };
    }
    weight = (*discrete)[*sample.discrete_slot].weight_;
  } else {
    // The production float expression 380 + 400*u can round to 780 even
    // when the stratifier returned the largest float strictly below one.
    if (sample.discrete_slot || !std::isfinite(sample.wavelength_nm) || sample.wavelength_nm < 380.f ||
        sample.wavelength_nm > 780.f) {
      return { ErrorCode::kInvalidArgument, "continuous product sample must lie in the float-rounded [380,780] band" };
    }
    weight = GetIlluminantSpd(std::get<IlluminantType>(light.spectrum_), sample.wavelength_nm);
  }
  SpectralRow row;
  const auto error =
      PhysicalSpectralRow(sample.wavelength_nm, weight, 1, SpectrumOrigin::kProductSample, sample.provenance, &row);
  if (!error.Ok())
    return error;
  out->rows.push_back(std::move(row));
  return {};
}

Error AssembleSpectrumQuadrature(const LightSourceConfig& light, const SpectrumQuadrature& quadrature,
                                 AssembledSpectrum* out) {
  *out = {};
  const auto* type = std::get_if<IlluminantType>(&light.spectrum_);
  if (!type || quadrature.nodes.empty() || quadrature.rule.empty() ||
      quadrature.evaluation_budget < quadrature.nodes.size() ||
      (quadrature.estimated_error &&
       (!std::isfinite(*quadrature.estimated_error) || *quadrature.estimated_error < 0))) {
    return { ErrorCode::kInvalidArgument, "continuous quadrature requires explicit nodes, rule and budget" };
  }
  AssembledSpectrum result;
  double total_mass = 0;
  double compensation = 0;
  for (const auto& node : quadrature.nodes) {
    if (!InContinuousBand(node.wavelength_nm))
      return { ErrorCode::kInvalidArgument, "quadrature node outside [380,780)" };
    SpectralRow row;
    const auto error = PhysicalSpectralRow(node.wavelength_nm, GetIlluminantSpd(*type, node.wavelength_nm),
                                           node.probability_mass, SpectrumOrigin::kQuadrature, quadrature.rule, &row);
    if (!error.Ok())
      return error;
    const double adjusted = node.probability_mass - compensation;
    const double next_mass = total_mass + adjusted;
    compensation = (next_mass - total_mass) - adjusted;
    total_mass = next_mass;
    result.rows.push_back(std::move(row));
  }
  // This entry point integrates the complete probability measure, not a
  // partial band. Allow double rounding, never silently renormalize weights.
  constexpr double kMassTolerance = 64 * std::numeric_limits<double>::epsilon();
  if (!std::isfinite(total_mass) || std::abs(total_mass - 1) > kMassTolerance)
    return { ErrorCode::kInvalidArgument, "full-band quadrature probability masses must sum to one" };
  result.quadrature = quadrature;
  *out = std::move(result);
  return {};
}

Error AssembleSource(const SunParam& sun, const ProductSourceSample& sample, AssembledSource* out) {
  *out = {};
  if (!std::isfinite(sun.altitude_) || !std::isfinite(sun.azimuth_) || !std::isfinite(sun.diameter_) ||
      sun.diameter_ < 0 || sun.diameter_ > 360 || !UnitDraw(sample.draw.radial_uniform) ||
      !UnitDraw(sample.draw.azimuth_uniform) || sample.provenance.empty()) {
    return { ErrorCode::kInvalidArgument, "invalid sun domain or cap draw" };
  }
  AssembledSource result;
  result.domain = sun;
  result.sample = sample;
  SunIncidentDirection(sun, result.center_direction.data());
  const auto cap =
      MakeSphericalCapTransform((sun.azimuth_ + 180.f) * math::kDegreeToRad, -sun.altitude_ * math::kDegreeToRad,
                                (sun.diameter_ / 2.f) * math::kDegreeToRad);
  result.product_direction = TransformSphericalCap(cap, sample.draw);
  double norm = 0;
  for (int i = 0; i < 3; ++i) {
    result.incident_direction[i] = result.product_direction[i];
    norm += result.incident_direction[i] * result.incident_direction[i];
  }
  for (auto& v : result.incident_direction)
    v /= std::sqrt(norm);
  *out = std::move(result);
  return {};
}

Error CaptureProductInput(const SceneConfig& scene, const std::string& scene_identity,
                          const std::vector<ProductLayerSelection>& selection, ProductInputSnapshot* out) {
  *out = {};
  if (scene_identity.empty() || selection.empty())
    return { ErrorCode::kInvalidArgument, "empty scene identity or chain" };
  ProductInputSnapshot snapshot;
  snapshot.scene_identity = scene_identity;
  snapshot.light = scene.light_source_;
  for (size_t i = 0; i < selection.size(); ++i) {
    const auto& layer = selection[i];
    if (layer.layer_index != i || i >= scene.ms_.size()) {
      return { ErrorCode::kInvalidArgument, "a chain must start at layer zero and use consecutive layers" };
    }
    const CrystalConfig* found = nullptr;
    for (const auto& setting : scene.ms_[i].setting_) {
      if (setting.crystal_.id_ != layer.crystal_id)
        continue;
      if (found)
        return { ErrorCode::kInvalidArgument, "ambiguous crystal key within a layer" };
      found = &setting.crystal_;
    }
    if (!found)
      return { ErrorCode::kUnknownCrystalId, "crystal key absent from selected layer" };
    snapshot.layers.push_back(
        { scene_identity, i, *found, layer.representative, layer.symmetry_bits, SymmetrySemantics::kPhysical });
  }
  *out = std::move(snapshot);
  return {};
}

namespace {

bool ValidLatent(const Distribution& dist, DistributionLatentDraw draw) {
  if (!std::isfinite(draw.value))
    return false;
  return dist.type == DistributionType::kNoRandom || dist.type == DistributionType::kGaussian ||
         dist.type == DistributionType::kGaussianLegacy || UnitDraw(draw.value);
}

Error RealizeAxis(const AxisDistribution& axis, const ProductAxisDraw& draw, AssembledProductLayer* out) {
  if (axis.IsFullSphereUniform()) {
    const auto* full = std::get_if<FullSphereAxisDraw>(&draw);
    if (!full || !UnitDraw(full->latitude_uniform) || !UnitDraw(full->longitude_uniform) ||
        !ValidLatent(axis.roll_dist, full->roll)) {
      return { ErrorCode::kInvalidArgument, "full-sphere axis requires latitude/longitude uniforms and roll draw" };
    }
    const auto point = TransformFullSpherePoint(full->latitude_uniform, full->longitude_uniform);
    out->angles = { point[0], point[1], TransformDistribution(axis.roll_dist, full->roll) * math::kDegreeToRad };
  } else {
    const auto* general = std::get_if<DistributedAxisDraw>(&draw);
    if (!general || !ValidLatent(axis.azimuth_dist, general->azimuth) || !ValidLatent(axis.roll_dist, general->roll)) {
      return { ErrorCode::kInvalidArgument, "distributed axis requires explicit branch draws" };
    }
    LatitudeSample latitude;
    const auto path = lat_path::SelectLatPath(axis);
    if (path.kind == lat_path::LatPathKind::kLutInverseCdf) {
      const auto* raw = std::get_if<LatitudeLutDraw>(&general->latitude);
      if (!raw || !UnitDraw(raw->quantile) || !UnitDraw(raw->flip_uniform)) {
        return { ErrorCode::kInvalidArgument, "latitude LUT requires CDF and flip uniforms" };
      }
      latitude = TransformLatitudeLut(*GetSharedLatLut(axis.latitude_dist), *raw);
    } else {
      const auto* raw = std::get_if<DistributionLatentDraw>(&general->latitude);
      if (!raw || !ValidLatent(axis.latitude_dist, *raw))
        return { ErrorCode::kInvalidArgument, "invalid latitude draw" };
      const float value = TransformDistribution(axis.latitude_dist, *raw);
      latitude = axis.latitude_dist.type == DistributionType::kGaussianLegacy ?
                     TransformLegacyLatitude(value) :
                     LatitudeSample{ value * math::kDegreeToRad, false };
    }
    out->angles = ComposeAxisAngles(latitude, TransformDistribution(axis.azimuth_dist, general->azimuth),
                                    TransformDistribution(axis.roll_dist, general->roll));
  }
  for (float v : out->angles) {
    if (!std::isfinite(v))
      return { ErrorCode::kInvalidArgument, "non-finite realized axis" };
  }
  const auto& a = out->angles;
  const auto rotation = BuildCrystalRotation(a[0], a[1], a[2]);
  std::copy_n(rotation.GetMat(), 9, out->product_pose.begin());
  // Same sampled angles and Euler convention as BuildCrystalRotation. Evaluate
  // the elementary rotations in double rather than promoting a non-orthogonal
  // float matrix into an API with a 1e-10 rotation tolerance.
  const auto factors = ProductRotationAngles(a[0], a[1], a[2]);
  const double inner[3]{ 0, 0, factors[0] };
  const double middle[3]{ 0, factors[1], 0 };
  const double outer[3]{ 0, 0, factors[2] };
  double ri[9], rm[9], ro[9], temp[9];
  analytic::so3::Exp(inner, ri);
  analytic::so3::Exp(middle, rm);
  analytic::so3::Exp(outer, ro);
  analytic::so3::MatMul(rm, ri, temp);
  analytic::so3::MatMul(ro, temp, out->analytic_pose.data());
  return {};
}

Error RealizeLayer(const PhysicalMemberRequest& request, const ProductLayerSample& sample, AssembledProductLayer* out) {
  if (sample.provenance.empty())
    return { ErrorCode::kInvalidArgument, "layer sample provenance is required" };
  auto error = ResolvePhysicalMemberScope(request, &out->scope);
  if (!error.Ok())
    return error;
  out->sample = sample;
  const auto plan = std::visit([](const auto& p) { return BuildShapeDrawPlan(p); }, request.crystal.param_);
  const auto status = RealizeShape(plan, sample.shape, &out->shape_sample);
  if (status != ShapeSampleStatus::kOk) {
    return { ErrorCode::kInvalidArgument, "invalid shape leader record: " + std::to_string(static_cast<int>(status)) };
  }
  error = RealizeAxis(request.crystal.axis_, sample.axis, out);
  if (!error.Ok())
    return error;
  const auto& v = out->shape_sample.consumed;
  float distances[6];
  std::copy_n(v.data() + kShapeScalarFace0, 6, distances);
  for (int i = 0; i < 6; ++i)
    out->shape.face_distance[i] = distances[i];
  const Crystal actual = std::visit(
      [&](const auto& p) {
        const auto ensemble = DeriveGeometricSymmetry(p);
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, PrismCrystalParam>) {
          out->shape.kind = analytic::CrystalShapeKind::kPrism;
          out->shape.height = v[kShapeScalarHeight];
          return Crystal::CreatePrism(v[kShapeScalarHeight], distances, ensemble);
        } else {
          out->shape.kind = analytic::CrystalShapeKind::kPyramid;
          out->shape.height = v[kShapeScalarPrismH];
          out->shape.upper_h = v[kShapeScalarUpperH];
          out->shape.lower_h = v[kShapeScalarLowerH];
          out->shape.upper_wedge_deg = p.wedge_angle_u_;
          out->shape.lower_wedge_deg = p.wedge_angle_l_;
          return Crystal::CreatePyramid(p.wedge_angle_u_, p.wedge_angle_l_, v[kShapeScalarUpperH],
                                        v[kShapeScalarPrismH], v[kShapeScalarLowerH], distances, ensemble);
        }
      },
      request.crystal.param_);
  const auto& geometry = actual.CfGeom();
  std::vector<detail::EntrySubTri> triangles(detail::CountEntrySubTris(geometry));
  detail::BuildEntrySubTris(geometry, triangles.data());
  for (const auto& triangle : triangles)
    out->surface_area += triangle.area;
  out->geometry_status = analytic::BuildFaceNormals(out->shape, &out->normals, &out->polygons);
  return {};
}
}  // namespace

Error AssembleProductInput(const ProductInputSnapshot& snapshot, const std::vector<ProductLayerSample>& samples,
                           const ProductSourceSample& source, const ProductSpectrumRequest& spectrum,
                           ProductInput* out) {
  *out = {};
  if (snapshot.scene_identity.empty() || samples.empty() || samples.size() != snapshot.layers.size()) {
    return { ErrorCode::kInvalidArgument, "one explicit sample is required for each snapshot layer" };
  }
  ProductInput result;
  result.scene_identity = snapshot.scene_identity;
  auto error = AssembleSource(snapshot.light.param_, source, &result.source);
  if (!error.Ok())
    return error;
  error = std::visit(
      [&](const auto& request) {
        using T = std::decay_t<decltype(request)>;
        if constexpr (std::is_same_v<T, DiscreteSpectrumSum>)
          return AssembleDiscreteSpectrum(snapshot.light, &result.spectrum);
        else if constexpr (std::is_same_v<T, ProductWavelengthSample>)
          return AssembleSampledSpectrum(snapshot.light, request, &result.spectrum);
        else
          return AssembleSpectrumQuadrature(snapshot.light, request, &result.spectrum);
      },
      spectrum);
  if (!error.Ok())
    return error;
  for (size_t i = 0; i < samples.size(); ++i) {
    if (snapshot.layers[i].scene_identity != snapshot.scene_identity || snapshot.layers[i].layer_index != i) {
      return { ErrorCode::kInvalidArgument, "layer does not belong to this scene/chain snapshot" };
    }
    const auto& identity = samples[i].identity;
    if (identity.scene_identity != snapshot.scene_identity || identity.layer_index != snapshot.layers[i].layer_index ||
        identity.crystal_id != snapshot.layers[i].crystal.id_) {
      return { ErrorCode::kInvalidArgument, "sample does not belong to this scene/layer/crystal snapshot" };
    }
    AssembledProductLayer layer;
    error = RealizeLayer(snapshot.layers[i], samples[i], &layer);
    if (!error.Ok())
      return error;
    result.layers.push_back(std::move(layer));
  }
  *out = std::move(result);
  return {};
}

std::optional<std::array<double, 3>> SingleCrystalIncidentOrbit(const ProductInput& input) {
  if (input.layers.size() != 1 || !input.layers[0].scope.snapshot.crystal.axis_.IsFullSphereUniform()) {
    return std::nullopt;
  }
  return input.source.incident_direction;
}

Error EvaluateProductChain(const ProductInput& input, const std::vector<size_t>& members, size_t spectral_row,
                           ProductChainEvaluation* out) {
  *out = {};
  if (members.empty() || members.size() != input.layers.size() || spectral_row >= input.spectrum.rows.size()) {
    return { ErrorCode::kInvalidArgument, "invalid concrete member/spectrum selection" };
  }
  for (size_t i = 0; i < members.size(); ++i) {
    if (members[i] >= input.layers[i].scope.members.size())
      return { ErrorCode::kInvalidArgument, "member index out of range" };
  }
  const auto& spectrum = input.spectrum.rows[spectral_row];
  auto direction = input.source.incident_direction;
  double product = 1;
  ProductChainEvaluation result;
  for (size_t i = 0; i < members.size(); ++i) {
    const auto& layer = input.layers[i];
    const auto& faces = layer.scope.members[members[i]];
    ProductLayerEvaluation evaluation;
    evaluation.incident = direction;
    if (layer.geometry_status != analytic::Status::kOk) {
      result.layers.push_back(std::move(evaluation));
      *out = std::move(result);
      return {};
    }
    std::vector<int> slots(faces.size());
    if (analytic::ResolveFaceSequence(layer.normals, faces.data(), static_cast<int>(faces.size()), slots.data()) !=
        analytic::Status::kOk) {
      evaluation.status = ProductContributionStatus::kMissingFace;
      result.layers.push_back(std::move(evaluation));
      *out = std::move(result);
      return {};
    }
    std::vector<double> segments(3 * (faces.size() + 1));
    evaluation.interface_transmittances.resize(faces.size());
    analytic::PathOutputs optics{ {}, 0, segments.data(), evaluation.interface_transmittances.data() };
    if (!analytic::EvaluatePath(layer.normals, slots.data(), static_cast<int>(slots.size()), spectrum.refractive_index,
                                direction.data(), layer.analytic_pose.data(), &optics)) {
      evaluation.status = ProductContributionStatus::kInvalidOptics;
      result.layers.push_back(std::move(evaluation));
      *out = std::move(result);
      return {};
    }
    std::copy_n(optics.outgoing_direction, 3, evaluation.outgoing.begin());
    evaluation.interface_product = optics.fresnel_transmission;
    analytic::Corridor corridor(layer.normals, layer.polygons, slots.data(), static_cast<int>(slots.size()));
    evaluation.entry_area = corridor.Evaluate(segments.data(), spectrum.refractive_index).value;
    evaluation.entry_weight = lm_pcg::entry_weight(static_cast<float>(evaluation.entry_area), layer.surface_area);
    evaluation.status =
        evaluation.entry_weight > 0 ? ProductContributionStatus::kPositive : ProductContributionStatus::kZeroSupport;
    product *= evaluation.entry_weight * evaluation.interface_product;
    direction = evaluation.outgoing;
    result.layers.push_back(std::move(evaluation));
    if (product == 0)
      break;
  }
  result.optical_weight = product;
  for (int j = 0; j < 3; ++j)
    result.xyz[j] = product * spectrum.coefficient[j];
  *out = std::move(result);
  return {};
}

bool NextProductMemberChain(const ProductInput& input, std::vector<size_t>* members) {
  if (members->empty() || members->size() != input.layers.size())
    return false;
  for (size_t i = 0; i < members->size(); ++i) {
    if ((*members)[i] >= input.layers[i].scope.members.size())
      return false;
  }
  for (size_t i = members->size(); i-- > 0;) {
    if (++(*members)[i] < input.layers[i].scope.members.size())
      return true;
    (*members)[i] = 0;
  }
  return false;
}

}  // namespace lumice::raypath
