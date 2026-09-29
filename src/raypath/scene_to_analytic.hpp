#ifndef LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_
#define LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_

// Scene config -> the analytic kernel's inputs: the conversions single_path_analysis.hpp records in
// its metadata, each one a pure function so it can be tested on its own.

#include <optional>
#include <vector>

#include "analytic/path_evaluation.hpp"
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

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SCENE_TO_ANALYTIC_HPP_
