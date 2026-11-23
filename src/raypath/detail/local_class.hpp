#ifndef LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_
#define LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_

#include <cstddef>
#include <vector>

#include "core/crystal.hpp"
#include "raypath/detail/physical_member_scope.hpp"

namespace lumice::raypath {

// The local-class answer at one sky direction: PURE ALGEBRA over the row's frozen member scope,
// zero sampling anywhere (the owner ruling this implements: who merges with whom, who
// representatives whom, are theorems — no Monte-Carlo or sample quantity may enter the
// decision). A class is the orbit of one row member under the elements that are retained
// (MeasureRetainsElement ∧ ShapeAdmitsElement) AND fix the target sky direction
// (ElementFixesSkyDirection); the class stays inside the row (an image leaving the row is
// dropped, so a partial-bit row keeps its own scope). The representative is the class's
// lexicographically smallest sequence; the transport facts keep, per member, one retained
// sky-fixing element carrying it to the representative — the closed-form pose map solution
// transport consumes one layer up (CompensatedPose).
//
// The classes partition the row's distinct members at this direction and are sorted by
// representative; nothing here ranks them by brightness (that is a rendering question the map /
// survey faces own) and nothing claims the searched-for light must be found — whether a class
// actually carries light at the target is a discovery fact, fused one layer up.
struct LocalClass {
  std::vector<int> representative;                // the class's lexicographic minimum
  std::vector<size_t> member_indices;             // into the row scope's member list, ascending
  std::vector<CrystalElement> to_representative;  // per member: g with g·member == representative
};

struct LocalClassRequest {
  // The sky point the classes are resolved at (the click), and the scene sun's azimuth in
  // degrees. Both spellings of a vertical plane's azimuth (φ and φ + 180) are accepted: the
  // sky-view predicate and the compensation mirror are invariant under it.
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  double sun_azimuth_deg = 0.0;
};

// Resolves the partition. Errors only on a scope that cannot be interpreted at all (a scope
// whose crystal variant carries no shape); an empty row member list resolves to an empty
// partition.
Error ResolveLocalClasses(const PhysicalMemberScope& row, const LocalClassRequest& request,
                          std::vector<LocalClass>* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_
