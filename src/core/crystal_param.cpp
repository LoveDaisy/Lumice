#include "core/crystal_param.hpp"

#include <array>

#include "util/logger.hpp"

namespace lumice {

// Which Distribution each ShapeScalar slot names on this crystal type, or nullptr
// for a slot the type simply does not have. A prism has one height and six faces;
// a pyramid has three stacked heights and six faces.
//
// These two functions are the SINGLE source of "does this slot physically exist
// on this type". Both consumers derive from them and neither restates the fact:
// leader normalization needs the addresses, canonicalization needs only the
// nullptr pattern, and `IsShapeScalarApplicable` (below) exposes the same pattern
// to the C API. A previous revision kept a separate `kApplicable*` bool table for
// canonicalization; the two encodings had nothing tying them together, so adding
// a crystal type or moving a slot could silently desynchronize them.
ShapeScalarSlots GetShapeScalarSlots(PrismCrystalParam& p) {
  return {
    &p.h_, nullptr, nullptr, nullptr, &p.d_[0], &p.d_[1], &p.d_[2], &p.d_[3], &p.d_[4], &p.d_[5],
  };
}

ShapeScalarSlots GetShapeScalarSlots(PyramidCrystalParam& p) {
  return {
    nullptr, &p.h_pyr_u_, &p.h_prs_, &p.h_pyr_l_, &p.d_[0], &p.d_[1], &p.d_[2], &p.d_[3], &p.d_[4], &p.d_[5],
  };
}

namespace {

// `slots` is read for its nullptr pattern only — never dereferenced — so this pass
// works on exactly the same applicability fact NormalizeSyncGroupsImpl uses.
void CanonicalizeSyncGroupsImpl(int sync_group[kShapeScalarCount], const ShapeScalarSlots& slots) {
  // Rule 1: a group declared on a slot this crystal type does not have is not a
  // membership at all. Zeroing first is what makes rules 2 and 3 see the real
  // member set.
  for (int i = 0; i < kShapeScalarCount; i++) {
    if (slots[i] == nullptr) {
      sync_group[i] = 0;
    }
  }

  // Rule 2: a one-member group is independence spelled differently.
  for (int i = 0; i < kShapeScalarCount; i++) {
    if (sync_group[i] == 0) {
      continue;
    }
    int members = 0;
    for (int k = 0; k < kShapeScalarCount; k++) {
      if (sync_group[k] == sync_group[i]) {
        members++;
      }
    }
    if (members < 2) {
      sync_group[i] = 0;
    }
  }

  // Rule 3: renumber 1..N by first appearance in ShapeScalar order, so the same
  // partition always has the same integers. Linear scans over <= 10 slots — a map
  // would cost more than it saves.
  int old_id[kShapeScalarCount]{};
  int new_id[kShapeScalarCount]{};
  int assigned = 0;
  for (int i = 0; i < kShapeScalarCount; i++) {
    if (sync_group[i] == 0) {
      continue;
    }
    int mapped = 0;
    for (int k = 0; k < assigned; k++) {
      if (old_id[k] == sync_group[i]) {
        mapped = new_id[k];
        break;
      }
    }
    if (mapped == 0) {
      old_id[assigned] = sync_group[i];
      mapped = assigned + 1;
      new_id[assigned] = mapped;
      assigned++;
    }
    sync_group[i] = mapped;
  }
}

// Leader-normalize one param's groups. `slots` maps a ShapeScalar index to the
// Distribution living there, or nullptr for a slot this crystal type lacks.
void NormalizeSyncGroupsImpl(const int sync_group[kShapeScalarCount], const ShapeScalarSlots& slots) {
  for (int i = 0; i < kShapeScalarCount; i++) {
    if (sync_group[i] == 0 || slots[i] == nullptr) {
      continue;
    }
    // The leader is this group's first occupied slot in ShapeScalar order, which
    // — because that order is the RNG draw order — is also the member that
    // actually consumes the draw.
    int leader = -1;
    for (int k = 0; k < i; k++) {
      if (sync_group[k] == sync_group[i] && slots[k] != nullptr) {
        leader = k;
        break;
      }
    }
    if (leader < 0) {
      continue;  // i is itself the leader.
    }
    if (!DistributionValueEqual(*slots[i], *slots[leader])) {
      LOG_WARNING(
          "Crystal shape sync group {}: member at shape-scalar index {} declared a different distribution than its "
          "group leader at index {}; overriding the member to match the leader.",
          sync_group[i], i, leader);
    }
    *slots[i] = *slots[leader];
  }
}

}  // namespace


void CanonicalizeSyncGroups(PrismCrystalParam& p) {
  CanonicalizeSyncGroupsImpl(p.sync_group_, GetShapeScalarSlots(p));
}

void CanonicalizeSyncGroups(PyramidCrystalParam& p) {
  CanonicalizeSyncGroupsImpl(p.sync_group_, GetShapeScalarSlots(p));
}

void NormalizeSyncGroups(PrismCrystalParam& p) {
  NormalizeSyncGroupsImpl(p.sync_group_, GetShapeScalarSlots(p));
}

void NormalizeSyncGroups(PyramidCrystalParam& p) {
  NormalizeSyncGroupsImpl(p.sync_group_, GetShapeScalarSlots(p));
}

void PrepareSyncGroups(PrismCrystalParam& p) {
  CanonicalizeSyncGroups(p);
  NormalizeSyncGroups(p);
}

void PrepareSyncGroups(PyramidCrystalParam& p) {
  CanonicalizeSyncGroups(p);
  NormalizeSyncGroups(p);
}


bool IsShapeScalarApplicable(CrystalKind kind, int slot) {
  if (slot < 0 || slot >= kShapeScalarCount) {
    return false;
  }
  // Derived from the same slot maps the two sync-group passes scope themselves
  // by, so applicability keeps exactly one definition. The local param exists
  // only to give those maps addresses to point at — no Distribution is read, and
  // nothing outlives this call.
  if (kind == CrystalKind::kPrism) {
    PrismCrystalParam probe;
    return GetShapeScalarSlots(probe)[slot] != nullptr;
  }
  PyramidCrystalParam probe;
  return GetShapeScalarSlots(probe)[slot] != nullptr;
}

}  // namespace lumice
