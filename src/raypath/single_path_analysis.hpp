#ifndef LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
#define LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_

// Single-path analysis: everything the Analyze workspace computes once ONE single-layer raypath of
// ONE crystal entry has been chosen (doc/raypath/raypath-analysis.md section 5.1.8, phase one) — the
// components of the fiber over a sky point, each component's poses with per-pose detail, and the
// deviation over the whole sun-direction sphere. It strings liblumice_analytic's kernel
// (src/analytic/: EvaluatePath, the fiber continuation, component discovery) to a scene config,
// and it knows nothing of a user interface. PointResult is the immutable completed value;
// SinglePathResult is the temporary internal envelope that retains deferred context inputs.
//
// Every conversion from the scene to the kernel's inputs is decided here once and recorded in the
// result (SinglePathMetadata): the crystal's shape (a distribution in the config, a nominal value
// here), the sun direction, the wavelength and refractive index. A request this module cannot
// answer truthfully — a multi-layer chain, a face the crystal lacks, a wavelength outside the
// refractive-index table — is an error with a code and a message, never a result.
//
// Frames and angles are doc/geometry/coordinate-convention.md's: a pose is the row-major body -> world
// rotation R = Rz(azimuth - 180) Ry(-zenith) Rz(roll); world directions are propagation directions
// (sky_direction.hpp: the sky point a direction comes from sits at altitude asin(-z)).

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "analytic/stop.hpp"
#include "config/config_manager.hpp"
#include "core/def.hpp"
#include "core/math.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/point_result.hpp"
#include "raypath/survey_reuse.hpp"

namespace lumice::raypath {

// The budget's default and bound: a 0 budget_ms request resolves to the default, an out-of-range
// one is refused. Enforcement is boundary-checked (loop boundaries only), and a stopped run
// serializes its partial document with meta.interrupted — an honest early exit, not a failure.
constexpr int kDefaultPointBudgetMs = 15000;
constexpr int kMaxPointBudgetMs = 120000;

// The kernel's reference default (LI section 9.5.9) and its hard upper bound; the call cannot be
// cancelled, so a larger request is refused.
constexpr int kDefaultSampleCount = 1000000;
constexpr int kMaxSampleCount = 100000000;
constexpr int kDefaultSunGridLatCount = 90;
constexpr int kMaxSunGridLatCount = 1800;

struct SinglePathRequest : PathSelection {
  // One face sequence per scattering layer, in Lumice face numbers (entry, internal reflections,
  // exit). v1 takes exactly one layer; the list shape exists so a shell can hand over whatever the
  // user wrote and this module alone decides what is refused.
  // The sky point, azimuth measured as the sun's (`analyze --center` convention).
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  // Unset: the config's wavelength if it names exactly one, else kDefaultWavelengthNm.
  std::optional<double> wavelength_nm;
  int sample_count = kDefaultSampleCount;
  // Latitude rows of the sun-direction grid; 0 skips the grid.
  int sun_grid_lat_count = kDefaultSunGridLatCount;
  // Warm starts, typically the component seeds of an earlier call (row-major body -> world, 9 per
  // pose). Densifying is not monotone (LI section 9.5.7); feeding back seeds is the only way to keep
  // what a sparser call found.
  std::vector<double> warm_seeds;
  // Untrusted warm candidate. The query admits it through survey_reuse before
  // consuming seeds or facts; absent means inline context.
  std::optional<WarmCandidate> warm_candidate;
  // The budget and the caller's one-way stop flag (see lumice_raypath.h): the deadline resolves
  // once at entry (0 = none for an internal caller; the bridge resolves the documented default),
  // the flag is polled at loop boundaries only. A stopped run reports meta.interrupted.
  int budget_ms = 0;
  const volatile int* interrupted = nullptr;
  // The query's one-way cancel token (the new contract's cancel core): folded into the
  // discovery legs' single stop and into the context's cancel-only stop.
  const analytic::StopToken* cancel = nullptr;
};

// The whole analysis, one typed entry: the execution runs the discovery legs and — when
// include_context is declared — produces the survey context IN THE EXECUTION (the deferred
// envelope that once retained the inputs for a later materialization is gone: a result is a
// frozen value, and nothing a reader does — getters, serialization, repeated reads — can start
// physical computation). A caller that declares no context pays nothing for it.
Error AnalyzePathPointValue(const ConfigManager& config, const SinglePathRequest& request, bool include_context,
                            PointResult* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
