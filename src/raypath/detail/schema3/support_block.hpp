#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_SUPPORT_BLOCK_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_SUPPORT_BLOCK_HPP_

// schema3 report core, support block (scrum 666.1): the delta-axis facts of one layer's members,
// the schema1 `reach` in its target-free form (the owner's schema2-redesign conclusions section
// 5: "per member x family delta intervals + endpoint-closed objects + partition state; the
// machine expression of the C05/C06 120-degree complement"). One row per fixed face sequence;
// the family aggregate groups the rows of one reflection-group orbit.
//
// A row carries, all measured at ONE assembly of the field (geometry_source.hpp's AssembleAxis —
// the partition, the onsets and the walk record come from that one pass):
//   - the member (the face sequence, schema1's literal spelling) and the partition axis
//     (intervals + the contract's coverage state + the escape slug as data, G3 interim);
//   - the ENDPOINT objects: kernel onsets (FieldOnsets' own table) whose value sits at an
//     interval endpoint within the partition's own merge constant kExtremumAtol — the profile
//     vocabulary (finite_jump / log_divergence / boundary_onset / degenerate / ...) is the
//     kernel's, passed through;
//   - the CONSTANT closed curves: closed-form weight kinks (KinkCoverage::kClosedFormAuthority)
//     whose D_P is constant along the circle within kExtremumAtol — the C06 hole edge, the basal
//     TIR kink's constant-value circle (measured 149.2468 deg at 550 nm on the beta crystal).
//     Each carries the n-continuation leg: the same circle re-read at the caller's wavelength
//     table (measured +1.8506 deg across 400-700 nm, the corpus's +1.851).
//
// Fail-closed forms: a walk-refused or escaped member has NO intervals, NO endpoint objects and
// NO constant curves — the refusal (partition state + message + slug) is the row's content.
// A per-wavelength read that cannot find the circle records NaN, never a plausible value.

#include <string>
#include <vector>

#include "analytic/dp_field.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_partition.hpp"
#include "raypath/detail/schema3/geometry_source.hpp"

namespace lumice::raypath::schema3 {

// A constant-D_P closed curve on the member's delta axis.
struct ConstantDeltaCurve {
  double d_p = 0.0;                    // radians, at the base index
  int weight_step = 0;                 // the TIR onset's internal step k
  std::vector<double> wavelengths_nm;  // the caller's table (parallel to critical_d_p)
  std::vector<double> critical_d_p;    // the constant value per wavelength (NaN = not found there)
};

// One fixed face sequence's delta-axis facts.
struct MemberSupport {
  std::vector<int> member;
  PartitionedAxis axis;                                  // intervals + partition state + slug (data)
  std::vector<analytic::CriticalOnset> endpoint_onsets;  // onsets AT an interval endpoint
  std::vector<ConstantDeltaCurve> constant_curves;       // closed constant-D_P circles inside
};

// The Phi-class aggregate: whether one reflection-group orbit's rows share the support — every
// row's partition completed and the interval endpoints agree within kExtremumAtol. `intervals`
// is the shared list when `shared`, empty otherwise (an agreeing family reports its intervals
// once; a disagreeing one reports nothing rather than a fabricated union). `members` carries the
// clustered rows' face sequences and `phi_class_note` their shared orbit annotation (both
// filled by the enumeration's clustering, which owns the identity side; empty note = the orbit
// was never asserted).
struct FamilySupport {
  bool shared = false;
  std::vector<analytic::DeviationInterval> intervals;
  std::vector<std::vector<int>> members;
  std::string phi_class_note;
};
FamilySupport AggregateFamily(const std::vector<MemberSupport>& rows);

// Whether two rows sit on the SAME delta-axis support: equal-length interval lists whose
// endpoints agree within the partition's own merge constant (kExtremumAtol). The one ruler for
// family aggregation — AggregateFamily and the enumeration's clustering both read it, so the
// "same support" comparison has a single implementation.
bool SameSupport(const std::vector<analytic::DeviationInterval>& a, const std::vector<analytic::DeviationInterval>& b);

// The layer's support block (the schema3 top-level `support` field's content).
struct SupportBlock {
  std::vector<MemberSupport> members;
  std::vector<FamilySupport> families;
};

// Builds one member's support row. `wavelengths_nm` / `indices` are parallel arrays (the
// caller's n-continuation table; both empty = no per-wavelength leg); the base index drives the
// partition and the onset table.
MemberSupport MemberSupportOf(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                              const int* slots, int slot_count, double base_index, const std::vector<int>& member,
                              const std::vector<double>& wavelengths_nm, const std::vector<double>& indices);

// The row from an assembly the caller already ran (the enumeration's path — one axis assembly
// per member feeds the objects AND this row).
MemberSupport SupportRowOf(const AxisAssembly& assembly, const analytic::FaceNormalTable& normals,
                           const analytic::FacePolygonTable& polygons, const int* slots, int slot_count,
                           double base_index, const std::vector<int>& member, const std::vector<double>& wavelengths_nm,
                           const std::vector<double>& indices);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_SUPPORT_BLOCK_HPP_
