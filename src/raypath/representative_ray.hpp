#ifndef LUMICE_RAYPATH_REPRESENTATIVE_RAY_HPP_
#define LUMICE_RAYPATH_REPRESENTATIVE_RAY_HPP_

// The representative-ray face: ONE concrete member's (crystal + face sequence) legal corridor at
// ONE explicit pose and wavelength, recovered as a drawable path — entry point, every internal
// reflection point, exit point, and the world-frame direction of every leg. Three analytic-kernel
// authorities, no second implementation of any of them (the assembly only sequences them):
//   1. the corridor's legality gates (`Corridor::Evaluate`: the entry/exit optics and the
//      corridor area),
//   2. the chain's direction-level validity and per-interface quantities (`TracePathChain`, the
//      one implementation `EvaluatePath` wraps — called here for the failure's name too),
//   3. the corridor's recovery (`RecoverRepresentativePath`: intersection centroid, unfolded
//      line, affine fold-back, independent containment verification).
// A pose any of them refuses is a named refusal riding the result, with the measured corridor
// quantities as evidence — never an approximate trajectory. The query is synchronous,
// deterministic, allocation-light and cancellation-free: one corridor evaluation, one chain
// evaluation, O(faces x corners) of geometry, no sampling loops, so it declares no budget and
// polls no stop.
//
// Identity: the result binds to the request's member, pose and wavelength (never a class-total
// path). The pose is the caller's explicit input — a point result's selected pose; the incident
// direction is the scene sun's (`scene_to_analytic.hpp`), the wavelength follows the point/map
// resolution rule (the scene's single discrete wavelength, else 550). Frames and angles are
// doc/geometry/coordinate-convention.md's.

#include <optional>

#include "config/config_manager.hpp"
#include "core/def.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/representative_ray_result.hpp"

namespace lumice::raypath {

struct RepresentativeRayRequest : PathSelection {
  // The pose the ray is recovered at: row-major body -> world, validated as a rotation. The
  // face sequence comes from PathSelection's single layer (more than one is refused).
  double pose[9]{};
  // Unset: the scene's single discrete wavelength, else 550 (the point resolution's rule).
  std::optional<double> wavelength_nm;
};

// The whole analysis. On error `out` is left default-constructed. `geometry` (optional): a
// workspace's prepared tables for the selection's crystal — the query then builds no tables of
// its own (the workspace's reuse read).
Error AnalyzeRepresentativeRay(const ConfigManager& config, const RepresentativeRayRequest& request,
                               RepresentativeRayResult* out, const PreparedGeometry* geometry = nullptr);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_REPRESENTATIVE_RAY_HPP_
