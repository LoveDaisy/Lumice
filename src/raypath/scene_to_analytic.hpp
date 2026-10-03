#ifndef LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_
#define LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_

// Scene config -> the analytic kernel's inputs: the conversions single_path_analysis.hpp records in
// its metadata, each one a pure function so it can be tested on its own.

#include <optional>
#include <vector>

#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"
#include "config/crystal_config.hpp"
#include "config/light_config.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

// The nominal crystal of a crystal entry: every shape scalar at its distribution's centre slot
// (the fixed value, the uniform midpoint, the gauss mean, the laplacian location). Heights are
// passed as the config holds them; the kernel folds their sign as the simulator does.
struct CrystalConversion {
  analytic::CrystalShape shape;
  std::string kind;  // "prism" or "pyramid"
  std::vector<NominalShapeScalar> scalars;
  double upper_wedge_deg = 0.0;
  double lower_wedge_deg = 0.0;
  bool shape_is_nominal = false;
};

CrystalConversion ConvertCrystal(const CrystalParam& param);

// The wavelength: the user's if given, else the config's when its spectrum is exactly one discrete
// wavelength, else kDefaultWavelengthNm; then n = IceRefractiveIndex::Get. Outside the table's
// [350, 900] nm (where Get would silently answer 1.0) it is kWavelengthOutOfRange.
struct WavelengthChoice {
  double wavelength_nm = 0.0;
  WavelengthSource source = WavelengthSource::kDefault;
  double refractive_index = 0.0;
};

Error ResolveWavelength(const LightSourceConfig& light, std::optional<double> user_nm, WavelengthChoice* out);

// The world propagation direction of the sun's rays (sun -> crystal): the centre of the cap the
// simulator samples (simulator.cpp SampleRayDir), in double.
void SunIncidentDirection(const SunParam& sun, double out[3]);

// One single-layer face sequence, resolved against the crystal's present faces. kMultiLayerUnsupported
// for more than one layer, kInvalidPath for none or a bad length, kFaceNotInCrystal naming the first
// face the crystal lacks.
Error ResolveSingleLayerPath(const std::vector<std::vector<int>>& layers, const analytic::FaceNormalTable& table,
                             std::vector<int>* slots);

// One layer of the modern scene-measure/diagnostic-field route. Unlike the older single-path
// analysis surface above, this admits a one-face external reflection as well as 2..64-face
// transmitted chains.
Error ResolveDiagnosticLayerPath(const std::vector<int>& faces, const analytic::FaceNormalTable& table,
                                 std::vector<int>* slots);

// A crystal's axis distribution as the band sum's pose density (LI docs/band-sum-contract.md section
// 2.2; the one-to-one table is LI ch11-pose-density-families.md section 1). A full-sphere uniform
// axis is `random`; otherwise the azimuth must be uniform over 360 degrees, the zenith a Gaussian
// (the sphere density the engine samples with its sin(theta) Jacobian), and the roll either uniform
// over 360 degrees (a zenith-only family) or a Gaussian (a zenith x roll family). Anything else is
// not expressible in v1 and is reported, with the distribution named, in `unsupported`; there is no
// approximate fallback. The family is a label only — the zenith mean is carried explicitly, so
// column and plate (parry and lowitz) are the same density for the same numbers; it is named after
// the nearer of the two presets' means (90 / 0 degrees).
struct PoseDensityConversion {
  analytic::PoseDensitySpec spec;
  std::string unsupported;  // empty when `spec` is valid
  bool Ok() const { return unsupported.empty(); }
};

PoseDensityConversion ConvertAxisToPoseDensity(const AxisDistribution& axis);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_
