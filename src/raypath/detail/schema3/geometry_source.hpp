#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_GEOMETRY_SOURCE_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_GEOMETRY_SOURCE_HPP_

// schema3 report core, geometry adapter (scrum 666.1): the raypath-side consumer of the 660
// kernel (src/analytic/dp_*), producing the 661 frozen contract's value types. The kernel keeps
// the single authority for every geometry semantics; this layer is a TRANSPORT — the two
// mapping groups it owns are declared here because the docking test
// (test_contour_contract_docking.cpp) carried them only until a production consumer existed
// (dp_contour.hpp's own note names this task as the trigger), and they carry no semantics of
// their own:
//   1. the mirrored-struct -> contract conversions (ChainCurve -> WeightSingularChain,
//      RestrictedFamilyCurve -> CriticalSetCurve, OrbitFiberStream -> FiberSampleStream) —
//      field-for-field, with the mirrored routing enums translated value-for-value;
//   2. the walk-status -> existence reading, which is dp_contour's own single ruling
//      (ChainFromBoundaryPieces': kOk -> computed, every other status -> walk_truncated) — read
//      through the authority, not restated.
//
// The partition assembly (PartitionAxisOf) repeats the kernel's own call ORDER (the kind-2 walk,
// the interior set, the topology, IntervalPartition — the sequence PartitionDeviationAxisImpl
// runs); the semantics live entirely in the kernel calls. The escape regime crosses as the
// kernel's slug STRING (data, G3's interim shape): the contract's typed escape_regime field has
// no value for any regime the port names today (the contract registered "slab_crease", the port
// spells each refusal separately), so the object layer reads PartitionedAxis::regime_slug and
// the contract struct stays untouched until the owner rules (G3, escalated — not decided here).
// The certificate consumes only `coverage`, so no false data flows meanwhile.

#include <string>
#include <vector>

#include "analytic/dp_contour.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_partition.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"

namespace lumice::raypath::schema3 {

// The angular tolerance every orbit-stream certificate call runs at — a caller discipline, not
// a free knob: the measured floor is test_geometry_source.cpp's acos-at-1 finding (~1.5e-8 rad),
// so 1e-6 keeps three decades of margin. Named here (not in a consumer's anonymous namespace)
// because the next certificate consumer (666.2's corroboration wiring) reads the same ruler.
inline constexpr double kOrbitAngularTol = 1e-6;

// ---------------------------------------------------------------------------
// The mirrored-struct -> contract conversions (the production mapping).
// ---------------------------------------------------------------------------

// The total four-arm translation of the analytic mirror's existence enum (the docking test's
// carried mapping; -Wswitch keeps a mirror arm drift red here).
ExistenceState ExistenceOf(analytic::CurveExistence existence);

// kernel WalkStatus -> contract ExistenceState, through dp_contour's ruling itself (an empty
// record costs nothing): kOk is computed, every refusal is a walk truncation.
ExistenceState ExistenceOfWalkStatus(analytic::WalkStatus status);

// ChainCurve -> WeightSingularChain, field for field. The chain's D_P values (the mirror's
// passthrough `values`) and its `note` have no contract-side carrier in v1 — they stop here by
// contract (the docking test's declared vocabulary gap), not by omission.
WeightSingularChain ChainOf(const analytic::ChainCurve& chain);

// RestrictedFamilyCurve -> CriticalSetCurve, field for field. The producer-side `indices` /
// `routed_nonfinite` / `note` have no contract-side carrier in v1 (same declared gap).
CriticalSetCurve CurveOf(const analytic::RestrictedFamilyCurve& curve);

// OrbitFiberStream -> FiberSampleStream, field for field. The producer-side `grid` /
// `spin_degenerate` / `note` have no contract-side carrier in v1 (same declared gap).
FiberSampleStream StreamOf(const analytic::OrbitFiberStream& stream);

// ---------------------------------------------------------------------------
// The partition axis: the completeness certificate's data, in the contract's shape.
// ---------------------------------------------------------------------------

struct PartitionedAxis {
  PartitionContext context{};  // the contract value type; kIncomplete carries NO trustworthy
                               // regime until G3 rules (see the module docstring)
  std::string regime_slug;     // kernel EscapeRegimeName slug — the report-side datum (data)
  std::string message;         // the walk's or the escape's LI message text (stable prefixes)
  analytic::WalkStatus walk_status = analytic::WalkStatus::kOk;  // the kind-2 walk's own status
  bool walk_closed = false;                                      // the walk's closure certificate (kOk)
  std::vector<analytic::DeviationInterval> intervals;            // empty unless coverage == kComplete
};

// The ONE assembly of a field's axis (the kernel's own call order — WalkBoundary -> interior
// critical points, slab branch or lattice Newton -> DomainTopologyOf -> IntervalPartition, and
// FieldOnsets' table off the same walk/interior): every consumer (the partition reading, the
// support block's endpoint objects, the enumeration's chains) reads this, so the expensive
// pieces run once and no consumer re-derives a second assembly. `record` is filled on a closed
// walk (the kernel's own invariant: a refusal delivers no loop data).
struct AxisAssembly {
  PartitionedAxis axis;
  std::vector<analytic::CriticalOnset> onsets;  // FieldOnsets' table; empty when the walk refused
  analytic::BoundaryWalkRecord record;          // the closed walk's rich bookkeeping (chains)
};

AxisAssembly AssembleAxis(const analytic::DeviationField& field);

// The partition view of the assembly (the certificate's consumption face). A walk refusal means
// no partition was run (coverage kUnknown, intervals empty — the kernel's own fail-closed
// shape); an escape means the partition refused (coverage kIncomplete, intervals empty —
// PartitionResult's mechanical invariant); a complete partition carries its intervals.
PartitionedAxis PartitionAxisOf(const analytic::DeviationField& field);

// The kernel regime slug -> the contract's registered EscapeRegime value, by name (the
// registered table IS the mapping — a future contract value maps without touching this
// function). False when no contract value carries that slug: today that is EVERY kernel regime
// (the contract registered only "slab_crease", the port names only suffixed refusals — the G3
// evidence; the slug stays data at the object layer meanwhile).
bool ContractRegimeOfSlug(const std::string& slug, EscapeRegime* out);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_GEOMETRY_SOURCE_HPP_
