#ifndef CORE_CRYSTAL_PARAM_H_
#define CORE_CRYSTAL_PARAM_H_

#include <cstdint>

#include "core/crystal_kind.hpp"
#include "core/math.hpp"

// The crystal shape parameters and the rules on them that need no JSON: the shape-scalar index
// space, the two param structs, the sync-group passes, and the P/B/D symmetry bits. They live in
// core rather than config because core/crystal.cpp consumes all of them, and core must not
// include config (cmake/lumice_layers.cmake: foundation sits below scene). Their JSON form —
// to_json/from_json and the key names — stays in config/crystal_config.{hpp,cpp}, which includes
// this header.

namespace lumice {

// Raypath symmetry bits (P = prism-face rotation, B = basal mirror, D = direction mirror), as a
// filter's `symmetry_` and every reduction over it spell them. FilterConfig::kSymNone/kSymP/kSymB/
// kSymD are aliases of these kept for compatibility with the config-layer spelling; there is one
// definition, here.
namespace sym {
constexpr uint8_t kSymNone = 0;
constexpr uint8_t kSymP = 1;
constexpr uint8_t kSymB = 2;
constexpr uint8_t kSymD = 4;
}  // namespace sym

//! @brief Index space for every randomizable crystal shape scalar.
//!
//! @details Slots are shared by both crystal types: a prism only owns
//!   kShapeScalarHeight + the six faces, a pyramid only owns the three
//!   pyramidal/prism heights + the six faces. Slots that do not apply to a type
//!   are simply never read for that type (CanonicalizeSyncGroups zeroes their
//!   sync group so an inapplicable declaration cannot leak into equality).
//!
//!   ⚠️ The order is deliberately chosen to be *verbatim the RNG draw order* in
//!   simulator.cpp's CrystalMaker: prism draws h_ then d_[0..5]; pyramid draws
//!   h_pyr_u_ -> h_prs_ -> h_pyr_l_ then d_[0..5]. That makes "a group's leader
//!   = its lowest-index member applicable to the crystal type" identical to "the
//!   member drawn first", so no second ordering definition is needed anywhere.
//!
//!   Note this is NOT the field declaration order of PyramidCrystalParam, and
//!   NOT the field order of the C API's LUMICE_CrystalParam
//!   (height/prism_h/upper_h/lower_h). The divergence is intentional; the C API
//!   mirror carries the same warning.
enum ShapeScalar : int {
  kShapeScalarHeight = 0,  // PrismCrystalParam::h_ — prism only
  kShapeScalarUpperH = 1,  // PyramidCrystalParam::h_pyr_u_ — pyramid only
  kShapeScalarPrismH = 2,  // PyramidCrystalParam::h_prs_ — pyramid only
  kShapeScalarLowerH = 3,  // PyramidCrystalParam::h_pyr_l_ — pyramid only
  kShapeScalarFace0 = 4,   // d_[0] — both types
  kShapeScalarFace1 = 5,
  kShapeScalarFace2 = 6,
  kShapeScalarFace3 = 7,
  kShapeScalarFace4 = 8,
  kShapeScalarFace5 = 9,
  kShapeScalarCount = 10,
};

//! @brief Shape-scalar sync groups: 0 = independent, 1..N = group id.
//!
//! @details Members of one group share a SINGLE random draw (the group's first
//!   applicable member consumes the RNG; the rest reuse its value without
//!   consuming anything). Zero-initialized = every scalar independent = the
//!   behavior before sync groups existed.
//!
//!   Deliberately a parallel array on the param structs rather than a field on
//!   Distribution: that type is shared with the orientation distributions and
//!   crosses the GPU device wire (pcg_shared.h), which is the wrong scope for a
//!   host-only shape-sampling concept.
//!
//!   Heights fold with std::abs while face distances stay signed (see
//!   CrystalMaker), so a group that mixes a height with a face distance shares
//!   the same *raw* draw but the height member consumes |v|. The mechanism does
//!   not forbid such a group; the asymmetry is documented, not validated away.

struct PrismCrystalParam {
  Distribution h_{ DistributionType::kNoRandom, 1.0f, 0.0f };  // Height, equal to c/a in HP2.0
  Distribution d_[6]{};                                        // Distance to center for prism faces
  int sync_group_[kShapeScalarCount]{};                        // Shape-scalar sync groups, 0 = independent
};

struct PyramidCrystalParam {
  Distribution h_prs_{};                                             // Prism height
  Distribution h_pyr_u_{ DistributionType::kNoRandom, 0.0f, 0.0f };  // Upper pyramidal relative height, from 0.0 to 1.0
  Distribution h_pyr_l_{ DistributionType::kNoRandom, 0.0f, 0.0f };  // Lower pyramidal relative height, from 0.0 to 1.0
  Distribution d_[6]{};                                              // Distance to center for prism faces
  int sync_group_[kShapeScalarCount]{};                              // Shape-scalar sync groups, 0 = independent
  // Upper wedge angle (degrees). The default is Miller {1,0,-1,1}, i.e.
  // MillerIndexToWedgeAngleDeg(1, 1) rounded -- see core/miller_wedge.hpp for the conversion.
  float wedge_angle_u_ = 28.0f;
  float wedge_angle_l_ = 28.0f;  // Lower wedge angle (degrees)
};

//! @brief Rewrite sync_group_ into its canonical form. Three rules, all required:
//!   1. slots not applicable to this crystal type are zeroed;
//!   2. single-member groups are zeroed (a singleton group IS independence);
//!   3. surviving groups are renumbered 1..N by first appearance in ShapeScalar order.
//!
//! @details Canonical form is not cosmetic: config_compare.hpp's operator== is
//!   the re-simulation trigger predicate, so `[2,1,2,1,2,1]` and `[1,2,1,2,1,2]`
//!   — the same partition — must compare equal or a semantic no-op would kick
//!   off a full re-run. Applied on parse, on serialization, and inside
//!   operator== (on local copies).
void CanonicalizeSyncGroups(PrismCrystalParam& p);
void CanonicalizeSyncGroups(PyramidCrystalParam& p);

//! @brief Leader-normalize the distributions inside each sync group.
//!
//! @details The group's leader (lowest applicable ShapeScalar index, i.e. the
//!   member drawn first) owns the distribution; every other member's
//!   Distribution is overwritten with the leader's. A member that differed is
//!   overwritten anyway, but only after a LOG_WARNING — neither silent nor
//!   rejected.
//!
//!   No ordering precondition: this pass does NOT need CanonicalizeSyncGroups to
//!   have run first. Both the members it rewrites and the leaders it elects are
//!   scoped to slots this crystal type actually has, and that scoping is
//!   structural — an inapplicable slot names no field to read or donate, so it
//!   can be neither leader nor member no matter what its group number says. The
//!   other two canonical-form rules cannot move the outcome either: collapsing a
//!   singleton group turns one no-op (a lone member finds no earlier peer) into
//!   another (group 0 is skipped), and renumbering is a bijection on non-zero ids
//!   while this pass only ever compares ids for equality. Pinned by
//!   NormalizeAloneExcludesInapplicableSlotWithoutCanonicalizeFirst, which runs
//!   this pass on its own.
//!
//!   (An earlier revision of this comment claimed the opposite — that a slot
//!   rule 1 is about to zero could be elected leader and donate its distribution.
//!   It cannot: such a slot has no distribution to donate. Prefer
//!   PrepareSyncGroups anyway, because both passes are needed, not because one
//!   protects the other.)
void NormalizeSyncGroups(PrismCrystalParam& p);
void NormalizeSyncGroups(PyramidCrystalParam& p);

//! @brief Run both sync-group passes: canonicalize, then leader-normalize.
//!
//! @details The composed entry point every parse path should call, so that
//!   neither pass can be forgotten. Each answers a different question and both
//!   are required: canonical form is what makes operator== (the re-simulation
//!   trigger) see equal partitions as equal, leader normalization is what makes
//!   the group's members carry one distribution. Their relative order does not
//!   change the result (see NormalizeSyncGroups above); it is fixed here only so
//!   there is one entry point rather than a choice at every call site.
void PrepareSyncGroups(PrismCrystalParam& p);
void PrepareSyncGroups(PyramidCrystalParam& p);

//! @brief Does shape-scalar `slot` physically exist on this crystal type?
//!
//! @details The runtime query onto the ONE table that answers this — the same
//!   slot map CanonicalizeSyncGroups and NormalizeSyncGroups scope themselves
//!   by. It exists so consumers outside this TU (the C API, and through it the
//!   GUI, which may not include this header) can ask core rather than keep a
//!   private truth table that has to be edited in lockstep. Out-of-range slots
//!   answer false rather than trapping, so a caller iterating 0..N over a
//!   mismatched slot count degrades to "not applicable" instead of reading
//!   garbage.
//!
//!   O(1), no allocation, no side effects — safe to call per row per frame.
bool IsShapeScalarApplicable(CrystalKind kind, int slot);

}  // namespace lumice

#endif  // CORE_CRYSTAL_PARAM_H_
