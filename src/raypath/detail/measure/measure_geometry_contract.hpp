#ifndef LUMICE_RAYPATH_DETAIL_MEASURE_GEOMETRY_CONTRACT_HPP_
#define LUMICE_RAYPATH_DETAIL_MEASURE_GEOMETRY_CONTRACT_HPP_

// The frozen interface between schema3's geometry layer (scrum 660: the kind-1/2/3 critical-set
// producers over u-S^2, ported from LI module C) and this measure layer (declared pose densities,
// visibility certificates, fiber quadrature, weight profiles). FROZEN FIRST, before 660 lands
// (task issue: "interface first" is the whole point of running the two in parallel): 660 conforms
// to these value types; after integration the SEMANTICS may not change — a gap discovered then is
// escalated (recorded in the task's progress and the dispatch session), never patched silently by
// bending a field's meaning. Adding a new enum VALUE is not a semantic change; redefining an
// existing one is.
//
// Vocabulary authority (schema3 design, conclusions of the schema2 redesign discussion, 2026-10-07
// sections 2/5): objects live on u-S^2, where u = R^T s_hat is the body-frame sun direction of a
// pose R (the reduction theorem: for a fixed chain every path quantity is a function of u alone;
// only the pose density sees the full pose). Kind vocabulary: kind-1 = D_P critical sets (caustics,
// ring edges; per-wavelength critical-value tables), kind-2 = dU_P gate boundaries, kind-3 = weight
// kink polylines (internal TIR onsets). Event names REUSE schema1's pinned words where they apply
// (tir_boundary, path_infeasible, gated_out) — schema1 is not touched by this task, the words are
// borrowed so the two vocabularies cannot drift apart.
//
// Frame conventions shared by every type below (the analytic kernel's, which LI mirrors):
//   - u is a BODY-frame unit vector; the world sun position vector s_hat points at the sun
//     (altitude/azimuth per src/util/sky_direction.hpp; the propagation direction of incident
//     light is -s_hat).
//   - A pose is a row-major body->world rotation; at the identity pose body == world and
//     u = s_hat.
//   - Angles are radians unless a name says _deg. Densities are per-radian unless a reference
//     measure is explicitly named.
//
// Evidence-form semantics (pinned here, with the mock contract tests, so integration cannot
// "discover" it later): the certified/unlit criteria are universal statements over a continuum, and
// a finite sample stream can only support them under a declared evidence form. Every
// FiberSampleStream names one:
//   - kStructural: the producer asserts a structural fact (e.g. the corridor area A is
//     identically zero off the sampled support by construction, or the entry gate excludes the
//     whole band) — the samples are then witnesses, not the proof.
//   - kSampledExhaustive: the producer asserts the stream covers the support to its declared
//     resolution (e.g. a periodic midpoint grid over a full spin orbit at spacing d_theta; the
//     assertion is resolution-limited and says so).
//   - kSampledPartial: no coverage claim (spot samples, a walk that stopped early).
// A certificate that would need universal quantification but only sees kSampledPartial routes to
// unproven rather than guessing. lit_fraction is measure-weighted over the samples the evidence
// actually covers: numerator = sum of weights of lit in-support samples, denominator = sum of
// weights of all in-support samples; 1.0 means "every covered unit of measure is lit", not "the
// whole support is lit" — which covered means depends on the evidence form, and the certificate
// records which form was decisive.

#include <cstddef>
#include <vector>

namespace lumice::raypath {

// ---------------------------------------------------------------------------
// Open enums with registered value tables.
//
// Each enum below is open: new values may be appended (a value a consumer does not know is data to
// report, not an error to reject). To keep "open" from degrading into an uncheckable comment, every
// enum ships its registered-value table (the accessor at the end of its group) and the contract
// test walks the table; a new value that is not added to the table leaves the table walk unchanged
// but fails the coverage test the same change must extend — the mechanism, not a promise.
// ---------------------------------------------------------------------------

// kind-1 existence states (schema3 `existence`): computed | escaped(regime) | walk_truncated |
// s4_declared. `escaped` carries a PartitionContext regime name; the parentheses are part of the
// schema3 spelling, the regime name is open (see EscapeRegime).
enum class ExistenceState { kComputed, kEscaped, kWalkTruncated, kS4Declared };
const char* ExistenceStateName(ExistenceState state);
const std::vector<ExistenceState>& RegisteredExistenceStates();

// Partition escape regimes, named by the geometry layer's partition machinery (LI module C's
// TopologyEscape). Open on purpose: the report side is fail-closed against unknown regimes, and a
// regime name is data. Registered today (LI wave-3 pull-forward 52.8, the 3-1-4-5/3-4-1-5 family):
// "slab_crease" is the one regime the port has named so far — the spelling is LI's own
// (dp_field/certificate.py `_slab_crease_gates`); the table exists so the walk test can
// hold the registry honest, not to bound the set.
//
// kUnset is a SENTINEL, not a regime (G3 owner ruling, 2026-10-09): it is the escape_regime
// fields' default and means "no regime applies / none was set". The kernel's regimes travel as
// slug STRINGS (PartitionedAxis::regime_slug, StructureObjectRecord::escape_regime_slug) and none
// of the slugs the port names has a contract value, so a producer never writes kUnset as an
// escape answer. Two consequences pinned here: (1) with kUnset in the table,
// RegisteredEscapeRegimes is the enum's complete registered string face (sentinel included), NOT
// a roster of regime names — "unset" is not a regime, and the name-lookup consumer
// (ContractRegimeOfSlug) excludes the sentinel explicitly; (2) the judging consumers read only
// coverage/existence, so the sentinel cannot enter a verdict: CertifyVisibility's signature
// structurally carries the context but its read set does not include the field, while
// DeriveBucket's signature carries only existence x visibility — the field can reach a bucket
// only through the certificate face (the invariance negative controls pin both arms, the bucket
// arm end-to-end).
enum class EscapeRegime { kSlabCrease, kUnset };
const char* EscapeRegimeName(EscapeRegime regime);
const std::vector<EscapeRegime>& RegisteredEscapeRegimes();

// schema1's pinned event words, reused verbatim for kind-2/kind-3 chain events. Registered table
// per schema1's filter vocabulary; a schema3-only event may be appended but schema1's words come
// first and are never respelled.
enum class ChainEventKind { kTirBoundary, kPathInfeasible, kGatedOut, kCorridorClosed };
const char* ChainEventKindName(ChainEventKind kind);
const std::vector<ChainEventKind>& RegisteredChainEventKinds();

// How a fiber sample stream's weights bind to the declared measure (see FiberSampleStream).
// Routing-sensitive (the quadrature switches on it), so it carries a registered table like the
// open enums above — accessors after FiberSampleStream, where the type is complete.
enum class MeasureBinding {
  // weight is a solid-angle element dOmega on u-S^2; the measure factor is the u-marginal density
  // rho_u w.r.t. dOmega (regular two-dimensional supports only).
  kSolidAngle,
  // weight is a parameter element of the source's own fiber parameterization (v1: d_theta of a
  // spin orbit — the axis family's azimuth orbit at fixed latitude and roll); the measure factor
  // is the declared density along that orbit w.r.t. the same parameter.
  kFiberParameter,
};

// ---------------------------------------------------------------------------
// SupportPiece: one piece of the declared measure's support on u-S^2 (the geometry layer hands
// the manifold for RESTRICTION — a plate family's latitude circle as the domain of restricted
// stationary points; the measure layer owns the density on it). Correction A of the schema3 design:
// the manifold belongs to the geometry layer, the density on it belongs here.
//
// v1 shapes: a spherical latitude-longitude rectangle (kArea) or an embedded polyline (kCurve,
// `closed` when the curve is a loop). The u points are first-class: a curve's preimage points are
// what a weight profile or a restricted certificate reads, not a derived quantity.
// ---------------------------------------------------------------------------
struct SupportPiece {
  enum class Shape { kArea, kCurve };
  Shape shape = Shape::kArea;
  // kArea: the open patch {lat in (lat_lo, lat_hi), lon in (lon_lo, lon_hi)}, radians.
  double lat_lo = 0.0;
  double lat_hi = 0.0;
  double lon_lo = 0.0;
  double lon_hi = 0.0;
  // kCurve: consecutive points on the unit sphere (body frame), and the parameter value of each
  // point in the source's own parameterization (`param` of point i pairs with points[i]).
  std::vector<double> u;  // 3 * point_count, packed
  std::vector<double> param;
  bool closed = false;
};

// ---------------------------------------------------------------------------
// CriticalSetCurve (kind-1): a polyline sample of one D_P critical set. Per point: u, the curve
// tangent (unit, body frame), the D_P value there, and the per-wavelength critical values at that
// point (n-continuation: structure position per wavelength; the wavelength list is the caller's —
// the contract fixes only that row k of `critical_d_p` belongs to `wavelengths_nm[k]`).
//
// existence carries the schema3 existence state; kEscaped names the regime in `escape_regime` and
// kWalkTruncated records how far the walk got (`walk_s` = arclength covered, NaN when not
// applicable). The curve may be EMPTY with existence kComputed (a computed empty set is data).
// `closed` says the polyline is a loop (the plate family's restricted latitude circle is the
// canonical closed kind-1 curve); the weight profile's quadrature wraps a closed curve.
// ---------------------------------------------------------------------------
struct CriticalSetCurve {
  ExistenceState existence = ExistenceState::kComputed;
  EscapeRegime escape_regime = EscapeRegime::kUnset;  // read only when existence == kEscaped;
                                                      // kUnset = none set (sentinel, not a regime)
  double walk_s = 0.0;                                // arclength covered before truncation
  bool closed = false;
  std::vector<double> u;        // 3 * point_count
  std::vector<double> tangent;  // 3 * point_count
  std::vector<double> d_p;      // point_count
  std::vector<double> wavelengths_nm;
  // point_count * wavelengths_nm.size(), row-major: critical D_P at point i, wavelength k.
  std::vector<double> critical_d_p;
  // Preimage annotation per point (optional, NaN when absent): the declared-measure support
  // parameter the point sits at, when the geometry layer knows it. The weight profile computes
  // rho_u from `u` directly; this field is for provenance, not for the density.
  std::vector<double> support_param;
};

// ---------------------------------------------------------------------------
// WeightSingularChain (kind-2 / kind-3): a polyline of the weight's singular support — a gate
// boundary (kind-2) or a TIR/kink onset (kind-3). A point may carry an event word; a kink flag
// marks where the chain's own rule changes (a corner of a gate boundary walk, a TIR onset).
// ---------------------------------------------------------------------------
struct WeightSingularChain {
  bool is_gate_boundary = false;  // kind-2 when true, kind-3 (kink polyline) when false
  ExistenceState existence = ExistenceState::kComputed;
  EscapeRegime escape_regime = EscapeRegime::kUnset;  // kUnset = none set (sentinel, not a regime)
  std::vector<double> u;                              // 3 * point_count
  std::vector<double> param;                          // source parameterization, pairs with `u`
  std::vector<char> kink;                             // point_count, 1 where the chain kinks
  // ChainEventKind value per point, -1 for "no event"; pairs with `u`.
  std::vector<int> event;
  bool closed = false;
};

// ---------------------------------------------------------------------------
// FiberSample: one point of the M1 geometric feedstock. `area` A is the entry-measure value at
// the pose that realizes u (absolute, in the crystal's length unit — the corridor's own unit, NOT
// rescaled to LI's hexagon-edge convention), `transmission` T the path power with every internal
// reflectance (the chromatic weighted-power kernel: entry/exit Fresnel and each internal R, 1
// under TIR). `valid` is the direction-level chain validity; `jet_degenerate` marks a point whose
// normal Jacobian the producer could not certify. `parameter` is the source's fiber parameter
// (NaN for unparameterized spot samples); `weight` is the quadrature element the stream's
// MeasureBinding names.
//
// SampleEvent (src/analytic/discovery.hpp) is an ADAPTER SOURCE for the intensity path only: it
// carries the merged product w = A*T and no A/T split, which is enough for a quadrature sum and
// NOT enough for certificate discrimination (an A = 0 corridor and a T = 0 TIR gate are different
// unlit reasons). A certificate-grade stream needs the split fields filled; "660-side sources
// should expose the A/T decomposition" is pre-registered as an expected integration-gap entry.
// A SECOND pre-registered integration gap, same shape: CERTIFICATE-GRADE STREAMS REQUIRE u
// FIDELITY — `u` must be the real body-frame sun direction, never a placeholder. A
// kFiberParameter stream is quadrature-legal with placeholder u (QuadratureIntensity reads only
// `parameter` under that binding), but CertifyVisibility reads `u` of every sample for support
// membership under EVERY binding, so the same stream fed to the certificate answers
// no_in_support_samples across the board — an integration-time symptom whose cause would be
// unreadable without this note.
// ---------------------------------------------------------------------------
struct FiberSample {
  double u[3] = { 0.0, 0.0, 0.0 };
  double area = 0.0;
  double transmission = 0.0;
  bool valid = false;
  bool jet_degenerate = false;
  double parameter = 0.0;
  double weight = 0.0;
};

struct FiberSampleStream {
  std::vector<FiberSample> samples;
  MeasureBinding binding = MeasureBinding::kSolidAngle;
  // The evidence form of the whole stream (see the header's evidence-form block). One form per
  // stream: a producer holding both structural and sampled facts sends two streams. Routing-
  // sensitive (the certificate quantifies over it), so it carries a registered table too —
  // accessors below, after the type is complete.
  enum class EvidenceForm { kStructural, kSampledExhaustive, kSampledPartial };
  EvidenceForm evidence = EvidenceForm::kSampledPartial;
  // Human-stable identifier of the producing source (for report provenance); free text.
  const char* source_name = "";
};

// The two routing-sensitive enums' registered tables (a50, same mechanism as the four open
// tables above): a NEW binding or evidence value must extend its table, its routing switch
// (QuadratureIntensity's — whose fail-visible default counts an un-routed value in
// binding_mismatch instead of swallowing the sample) and the coverage walk in the same change.
const char* MeasureBindingName(MeasureBinding binding);
const std::vector<MeasureBinding>& RegisteredMeasureBindings();
const char* EvidenceFormName(FiberSampleStream::EvidenceForm form);
const std::vector<FiberSampleStream::EvidenceForm>& RegisteredEvidenceForms();

// ---------------------------------------------------------------------------
// PartitionContext: what the geometry layer's partition says about the delta-slice this object
// lives in. coverage kComplete = the critical-value partition covers the delta axis with no
// escapes; kIncomplete names the regime that refused; kUnknown = no partition was run (v1: kind-1
// producers that do not partition yet). The certificate routes unproven on anything but
// kComplete.
// ---------------------------------------------------------------------------
struct PartitionContext {
  enum class Coverage { kComplete, kIncomplete, kUnknown };
  Coverage coverage = Coverage::kUnknown;
  EscapeRegime escape_regime = EscapeRegime::kUnset;  // read when coverage == kIncomplete;
                                                      // kUnset = none set (sentinel, not a regime)
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DETAIL_MEASURE_GEOMETRY_CONTRACT_HPP_
