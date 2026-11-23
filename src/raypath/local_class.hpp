#ifndef LUMICE_RAYPATH_LOCAL_CLASS_HPP_
#define LUMICE_RAYPATH_LOCAL_CLASS_HPP_

// The local-class answer's VALUES (doc/raypath/raypath-elements.md's consumers): one row's
// physical classes at one sky direction, resolved by raypath/detail/local_class.hpp's resolver
// and carried across the workspace and C faces. Pure data — a frozen answer owns its storage
// and stays readable after the row scope and the workspace are gone.

#include <cstddef>
#include <vector>

#include "core/crystal.hpp"

namespace lumice::raypath {

struct LocalClass {
  std::vector<int> representative;     // the class's lexicographic minimum
  std::vector<size_t> member_indices;  // into the row scope's member list, ascending
  // The members' own face sequences, same order as member_indices: the frozen answer is
  // self-contained (readable after the row scope and the workspace are gone).
  std::vector<std::vector<int>> member_sequences;
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

// The row + sky point a workspace resolves classes for: the analysis list row's own selection
// spelling. The scene sun's azimuth is the workspace's to read, not the caller's to restate.
struct LocalClassQuery {
  IdType crystal_id = 0;
  std::vector<int> representative;
  uint8_t symmetry_bits = 0;
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
};

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_LOCAL_CLASS_HPP_
