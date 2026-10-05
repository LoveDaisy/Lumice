#ifndef RAYPATH_DETAIL_PHYSICAL_MEMBER_SCOPE_H_
#define RAYPATH_DETAIL_PHYSICAL_MEMBER_SCOPE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "config/crystal_config.hpp"
#include "core/crystal.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

// Value-owned list snapshot. Bits and semantics are explicit, not read from UI
// state or inferred from an actual shape draw. A class belongs to an ensemble.
struct PhysicalMemberRequest {
  std::string scene_identity;
  size_t layer_index;
  CrystalConfig crystal;
  std::vector<int> representative;
  uint8_t symmetry_bits;
  SymmetrySemantics semantics;
};
struct PhysicalMemberScope {
  PhysicalMemberRequest snapshot;
  SymmetryGating gating;
  std::vector<std::vector<int>> members;
};
Error ResolvePhysicalMemberScope(const PhysicalMemberRequest& request, PhysicalMemberScope* out);

}  // namespace lumice::raypath
#endif  // RAYPATH_DETAIL_PHYSICAL_MEMBER_SCOPE_H_
