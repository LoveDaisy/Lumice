# `liblumice_analytic`: the published analytic interface

> Status: **design, not yet built** (2026-09-28). Nothing described here exists in `src/` or
> `CMakeLists.txt` yet. This document is the input to the work that builds the target and its
> export list, hands logging to the host, packages and versions the library, and adds an
> external-consumer smoke test — and to the first real module (single-path inversion + fiber
> walk), which lands with the Analyze workspace's first phase (`doc/raypath-analysis.md` §5.1.8).
>
> Every decision below is marked either **(owner)** — ruled by the owner on 2026-09-28, not open
> for re-derivation — or **(design)** — this document's own judgement, open to the owner's review.
> The header in §4.5 is a draft for review; it is not a file in the tree.

Related: `doc/api-layering-and-product-lines.md` (why a new narrow interface and not
`lumice.h`; §8 is the 2026-09-28 update), `doc/raypath-analysis.md` §5.1.6 (repository roles and
the sharing criterion), `doc/raypath-symmetry.md` §1.1 (the two meanings of P/B/D),
`doc/coordinate-convention.md` (frames, face numbers, pose chain).

---

## 0. What this library is, in one paragraph

Lumice is the product; Lumice Integral (LI) is research and reference. The sharing criterion is
**"stable, and not each other's cross-check object"** (`doc/raypath-analysis.md` §5.1.6):
the primitive and convention layer (crystal geometry, face numbering, symmetry reduction,
Snell/Fresnel) is kept as two deliberately independent copies; an algorithmic module, once it has
matured, converges to one Lumice C++ implementation that LI consumes through a shared library.
`liblumice_analytic` is that library. It is the **first** Lumice library published to an external
consumer, and it is deliberately **not** `lumice.h`: of the 23 incompatible changes classified in
`doc/api-layering-and-product-lines.md` §8, the L1 rendering/annotation surface moves an order of
magnitude faster than anything an external consumer should be pinned to. **(owner)**

The first consumer is LI, through a Python binding (ctypes or pybind, LI's choice). The interface
is therefore a **pure C ABI**: no C++ types, no callbacks into the host other than logging, plain
POD structs and fixed-width arrays that a ctypes `Structure` can declare line for line.

---

## 1. Naming **(owner)** and the spelling rules under it **(design)**

| Item | Name |
|---|---|
| Library | `liblumice_analytic` (`.so` / `.dylib`; `lumice_analytic.dll` on Windows) |
| Header | `lumice_analytic.h` |
| Symbol prefix | `LUMICE_ANALYTIC_` |

Why these names and which were rejected (`kernel`, `inverse`, `math`, `raypath`, `analysis`) is
recorded with the owner's ruling; the short form is that "analytic" is the product's existing word
for deterministic evaluation as opposed to MC sampling, and every rejected name either collides
with an existing file/API name or reads as exposing the primitive layer.

Spelling rules, chosen so that **one regular expression, `\bLUMICE_ANALYTIC_`, matches every
identifier this header introduces and nothing else in the tree** — the property that lets
`scripts/check_policies.py` guard the surface the same way `no-test-symbol-in-src` guards
`LUMICE_TEST_`:

| Kind | Form | Example |
|---|---|---|
| Function | `LUMICE_ANALYTIC_<CamelCase>` | `LUMICE_ANALYTIC_TraceFiber` |
| Struct / enum type | `LUMICE_ANALYTIC_<CamelCase>`, tag `..._` | `LUMICE_ANALYTIC_FiberResult` |
| Enumerator / macro constant | `LUMICE_ANALYTIC_<UPPER_SNAKE>` | `LUMICE_ANALYTIC_FIBER_CLOSED` |
| Error code | `LUMICE_ANALYTIC_OK`, `LUMICE_ANALYTIC_ERR_<UPPER_SNAKE>` | `LUMICE_ANALYTIC_ERR_NULL_ARG` |
| Version macro | `LUMICE_ANALYTIC_API_VERSION` | — |
| Include guard | `LUMICE_ANALYTIC_H_` | — |

`LUMICE_ANALYTIC_` and `LUMICE_` are distinguishable mechanically only in one direction: every
`LUMICE_ANALYTIC_*` name also matches `\bLUMICE_`. A gate that means "product C API only" must
therefore exclude the longer prefix explicitly. Recorded here so the gate added with the target
does not rediscover it.

Where the header lives, and what the gate checks, is decided with the target (§2.5); this
document's constraint on it is only that `lumice.h` must not include `lumice_analytic.h` and
vice versa (§7).

---

## 2. Link boundary

### 2.1 The rule **(owner)**

The library links **all of `lumice_obj`**, the same object library `liblumice` and
`liblumice_testapi` are made of (`CMakeLists.txt:819`), rather than carving objects out of it.
This is the `liblumice_testapi` shape (`CMakeLists.txt:1001-1019`): same objects + its own header
+ its own prefix + its own shared library. Three consequences follow, each checked against the
build as it stands.

### 2.2 Dead-code stripping must be switched on for the new target

`-dead_strip` (Apple/Clang) and `--gc-sections` (GNU) are applied today only under
`$<CONFIG:MinSizeRel>`, and only to the `lumice` target (`CMakeLists.txt:947-951`). In Release,
every `.o` of the object library goes into the shared library whole; `-fvisibility=hidden`
(`CMakeLists.txt:924-927`) decides what is *exported*, not what is *kept*. The objects are already
compiled with `-ffunction-sections -fdata-sections` in Release (same lines), so the link flag is
the only missing half.

Requirement: the new target sets the stripping link option itself, for the configuration it
ships in. Stripping keeps static constructors and everything they reference (a spdlog registry,
the shared sink, font tables), so it affects size only, never behaviour — a stripped and an
unstripped build must behave identically, which is also how to test it.

### 2.3 Only from a build without CUDA

With `LUMICE_CUDA_ENABLED`, `lumice_obj` links `CUDA::cudart` **PUBLIC** and defines
`LUMICE_CUDA_ENABLED=1` (`CMakeLists.txt:842-844`). A shared library linked from it carries a
load-time dependency on the CUDA runtime, which stripping does not remove, and an LI Python
process would then fail to load it on any machine without CUDA — for a computation that never
touches a GPU. The published artifact is therefore produced only from a configure with
`LUMICE_CUDA_ENABLED=OFF`. Metal on macOS (`CMakeLists.txt:832-833`) is a system framework and
imposes nothing on the consumer.

### 2.4 ISA: the baseline tier

`lumice_apply_isa_march` (`CMakeLists.txt:886-909`) compiles **`lumice_obj` itself** for the tier
in `LUMICE_ISA_LEVEL`. The new target links those objects; it has no compile of its own to apply
a different tier to. So the ISA is a property of the **configure** the artifact is produced from,
not of the target, and the rule is:

- the published `liblumice_analytic` is produced from a configure with
  `-DLUMICE_ISA_LEVEL=baseline` (what every `ci.yml` job already passes);
- never from the local default `native` (`CMakeLists.txt:55`), which builds for the machine it
  runs on and is exactly the binary that crashes on a consumer's older CPU;
- no v3/v4 twin. The release ships two engine builds per platform because a Lumice-owned shell
  (`src/launcher/win_engine_loader.c` on Windows; glibc hwcaps on Linux) chooses between them at
  start-up. A third-party process loading this library has no such shell, and the analytic
  module's cost is per-call arithmetic, not the MC hot loop the v3/v4 builds were measured on.
  If a measured need for a faster tier ever appears, it is a packaging change (two files + a
  consumer-side choice), not an interface change.

On arm64 `baseline` applies no `-march` flag at all, so the rule is uniform across platforms.

### 2.5 Produced where, exported how — left to the target work

What the artifact list is: one shared library per platform (`liblumice_analytic.so`,
`liblumice_analytic.dylib`, `lumice_analytic.dll` + import library) plus `lumice_analytic.h`,
from a Release, non-CUDA, baseline-ISA configure. The mechanisms — the per-library export list
(§7), the Windows export path (today both existing shared targets rely on
`WINDOWS_EXPORT_ALL_SYMBOLS`, which would export every engine symbol), where the header lives,
and the prefix gate — are decided and built with the target.

### 2.6 Consumer notice: one engine per process

`liblumice`, `liblumice_testapi` and `liblumice_analytic` each contain their own complete copy of
the engine, statics included (the shared log sink, the global logger, every function-local
static). **A process loads exactly one of the three.** Loading two gives two independent sets of
statics — two log sinks, two callback registrations — with nothing to report the split. This goes
into the consumer-facing notes shipped with the library, verbatim.

---

## 3. Symmetry semantics **(owner)**

This section exists because the owner requires that the two repositories never compute with two
different meanings of "symmetry" without saying so. Twice already a comparison between them went
wrong on exactly this point.

### 3.1 The two meanings

`doc/raypath-symmetry.md` §1.1 is the authority; in one line each:

- **L1 — label equivalence.** The filter's `symmetry: "PBD"`: an unconditional rewrite of face
  *labels* (`P`: any 60° relabelling of the prism faces; `B`: swap face 1↔2 and the upper/lower
  cone bands; `D` by the roll mean). It does not look at the crystal's shape or the pose
  ensemble. It is shorthand for writing a face-sequence family at once.
- **L2 — physical equivalence.** The raypath-analysis panel's folded rows: two face sequences
  are one class only when both the crystal's shape (§2a) **and** the pose ensemble (§2b) realise
  the symmetry element that maps one onto the other.

On a regular hexagonal prism under a fully symmetric pose ensemble the two coincide. Below `D6h`
they do not.

### 3.2 Where this interface sits: under both

`liblumice_analytic` accepts **only a concrete face sequence** (`const int* faces`) and computes
the physics of exactly that sequence. It takes **no symmetry parameter** and offers **no
reduction or expansion function**. It never merges two sequences, and it never splits one.

Reason: symmetry reduction belongs to the primitive and convention layer, which the two
repositories keep as two independent copies precisely so that each checks the other. If this
library offered a reduction, LI would sensibly use it, and the two sides would stop
cross-checking at the one place they have already diverged twice. LI's own overview records the
same boundary from its side (LI `docs/overview.md` §5.1: "Symmetry reduction is not part of either
side's shared boundary"). The reductions stay where they are:

| Who | Meaning | Where |
|---|---|---|
| Lumice filter language | L1 | the filter's own `symmetry` field |
| Lumice analysis panel | L2 | `LUMICE_ExpandRaypathClass` (`lumice.h`, v4.49) |
| LI | L1 | `symmetry.reflection_group.pbd_orbit` |
| LI | L2 (shape half) | `symmetry.crystal_group.true_symmetry_group` (`G_true`); the pose half is the caller's pose density |

### 3.3 Rules for callers and for parity fixtures

1. A caller that reduces or expands face sequences **outside** this interface — to decide which
   sequences to trace, or to fold results into classes — must state which meaning it used, L1 or
   L2. "The symmetry reduction" without that qualifier is not an acceptable description in code,
   documentation or a result file.
2. Every LI ↔ Lumice parity fixture that involves more than one face sequence per compared
   quantity declares its semantics in a machine-readable field, `symmetry_semantics: "L1"` or
   `"L2"` (or `"none"` when every compared quantity is a single concrete sequence). A fixture
   without the field is incomplete. The external-consumer smoke test and the first module's
   parity fixture both follow this.
3. A fixture whose semantics is `L1` is only meaningful where L1 and L2 agree, or where the thing
   being tested *is* the label rewrite. LI already guards this on its side: its Lumice-family
   comparison refuses a Lumice `PBD` filter when `|G_true| < 24`.

### 3.4 The typical divergence

A **trigonal (three-fold) prism** — `face_distance` alternating, e.g. `[1, d, 1, d, 1, d]` with
`d ≠ 1`, the geometry behind Liljequist-parhelion-type calculations — has shape symmetry `D3h`,
not `D6h`. L1's `P` still merges face sequences under all six 60° relabellings, so it puts a path
entering a near face and a path entering the corresponding far face into one class; L2 merges only
under the three rotations the crystal actually has, and keeps them apart. The two give different
class lists and different per-class energies for the same simulation. Measured: LI found L1
grouping over-merges on `D3h` class `[3,5]` by **1.41×** (LI `docs/overview.md` §4.1). Any number
that crosses the repository boundary for such a crystal is wrong unless it says which meaning it
was reduced under — and it is never wrong at the level of this interface, because this interface
only ever sees one concrete sequence.

---

## 4. First module: single-path inversion + fiber walk

Signatures and data conventions only; there is no implementation. Sources: LI
`docs/phase1-math-contract.md` §9 (the backend-independent `FiberProblem` / `ContinuationOptions`
/ `FiberResult` semantics and the §9.4 status/reason table), LI `docs/conventions.md` (rows 1–8,
16–21), and the fiber computation of `doc/prototypes/analyze-workspace.html` (the panel's
"sun direction in crystal — level set = fiber" view).

### 4.1 Crystal: deterministic closed-form scalars, not `LUMICE_CrystalParam` **(design)**

`LUMICE_CrystalParam` (`lumice.h`) is built for MC sampling: `height`, `face_distance[6]`,
`zenith`, `azimuth`, `roll` are all `LUMICE_Distribution`, and `sync_group[]` carries the RNG
bookkeeping. An inversion needs one fixed crystal and an explicit pose; every distribution wrapper
would have to be `NO_RANDOM` and the pose fields ignored. Reusing it would also pull `lumice.h`
into the header, which §7 forbids.

The new `LUMICE_ANALYTIC_Crystal` carries the closed-form scalars one to one with LI's
`HexPrism.from_lumice(height, face_distance, a)` and `Pyramid.from_lumice(...)` — both of which
exist precisely to turn one *sampled Lumice instance* into scalars, so both sides already agree
that the boundary between "distribution" and "one crystal" is this one. Field semantics are those
of `doc/configuration.md` §prism / §pyramid (including negative `face_distance` and the pyramid
legality rules); the header does not restate them.

- Wedge angles are the **final angle in degrees**, as `LUMICE_CrystalParam.upper_wedge_angle`
  already is. A caller holding Miller indices converts first (`doc/configuration.md`; LI does
  this in `Pyramid.from_lumice`). There is no public Miller converter in `lumice.h` today, and
  adding one here would put a primitive-layer rule into the shared surface.
- No absolute size. Directions and Fresnel factors are scale-free; an entry cross-section, if it is
  ever added (§9 item 8), is reported in units of the hexagon edge `a = 1`, LI's convention.
- One flat struct with a `kind` discriminator rather than a tagged union: a ctypes `Structure`
  maps a flat struct field for field, where a union needs a nested declaration per member on every
  binding. Fields not used by `kind` must be zero; a non-zero unused field is
  `LUMICE_ANALYTIC_ERR_INVALID_VALUE`, so a caller who fills the wrong fields hears about it.
- A crystal that fails the closed-form validity gate (the engine would build an empty crystal and
  warn) is `LUMICE_ANALYTIC_ERR_INVALID_CONFIG`, never a silently empty result — see §6 for why.

### 4.2 Path, directions, pose **(design)**

- **Path**: `const int* faces, int face_count` — a concrete face sequence in Lumice face numbers
  (`doc/coordinate-convention.md` §1; prism faces 1–8, pyramid faces per the same document). No
  symmetry parameter (§3).
- **Refractive index**: `double refractive_index`, given by the caller. The module does not choose
  a wavelength model; Lumice's Sellmeier fit (`IceRefractiveIndex::Get`) and LI's copy of it are
  primitive-layer twins that should stay separately checkable.
- **Directions** follow LI's contract names and Lumice's conventions: `incident_direction` is the
  world-space **propagation** direction of sunlight, sun → crystal (LI's contract `s`, equal to
  `−ŝ`); `target_direction` is the world-space propagation direction of the outgoing light,
  crystal → observer (LI's `d`); the sky point the observer sees is `−d`. Both unit vectors.
- **Pose**: `double[9]`, row-major 3×3 rotation, **body → world** (`v_W = R v_B`), the same active
  chain as `doc/coordinate-convention.md` §6. Matrix, not quaternion: no sign ambiguity, and a
  fixed nine-double array is the simplest thing a C ABI can pass (LI stores poses as matrices too,
  `docs/conventions.md` row 16).
- **Crystal-frame sun direction** (the prototype's fiber coordinate, LI's `u`):
  `u = Rᵀ · (−incident_direction)`, toward the sun, in body coordinates.

**Scope ruling: "single-path inversion" in v0 is continuation from a caller-supplied seed; seed
search is not in v0.** LI's contract lists `seed` as a required `FiberProblem` field (§9.1) and its
architecture lists seed search and predictor–corrector continuation as separate duties of the
fiber solver (LI `docs/overview.md` §4 item 4). Continuation has a precise numerical contract today
(§5–§10); discovery does not. v0 builds the half that has a contract. If the owner considers a
root finder part of "inversion" from day one, LI first writes discovery's contract to the same
standard and this section is revised (§9 item 1).

### 4.3 Two function families **(design)**

- **`LUMICE_ANALYTIC_EvaluatePath`** — one crystal, one path, one pose: the outgoing direction,
  each segment's body-frame propagation direction, each interface's transmittance, the total
  Fresnel factor `T_entry · Π R_k · T_exit` (unpolarised s/p average per interface, `R_k = 1`
  under TIR — the rule in LI `docs/conventions.md` row 18 and `optics.cpp::HitSurface`), and a
  validity flag. This is the per-point detail the panel shows ("each segment's direction and
  Fresnel energy") **and** the evaluator continuation calls internally — one implementation, so
  the fiber and the panel can never disagree about what a point on the fiber is.
- **`LUMICE_ANALYTIC_TraceFiber` / `TraceFiberBatch`** — continuation along the fiber from a seed:
  accepted poses, crystal-frame sun directions, arclength increments, residual norms, tangents,
  and a `status`/`reason` pair.

**Batch shape.** LI's heavy use is one path swept over many targets (pixels or sample points)
with the crystal, the path and the sun fixed. The batch call takes one crystal and one options
block and an array of problems. The reason for a batch at all is call overhead: a ctypes call
costs on the order of microseconds, comparable to one short continuation, so per-pixel calls from
Python would be dominated by the binding. Whether the library also parallelises *inside* a batch
is left open (§9 item 10): v0 starts no threads (§5.3), and a ctypes caller can already run
batches from several threads, since ctypes releases the GIL for the duration of a foreign call.

**Result scope — a deliberate narrowing that needs the owner's ruling.** LI's `FiberResult`
(§9.3) also requires `jacobian_diagnostics`, `step_diagnostics`, `branch_diagnostics`,
`closure_diagnostics`, `terminal_payload`, `conventions`, `weight_observables` and
`component_scope`: the evidence LI's conformance matrix (§11) uses to decide whether a backend
satisfies §5–§10. The v0 draft carries only the kinematic fields the product needs. That is enough
for "LI reads the point list"; it is **not** enough for "LI uses this library as its continuation
backend and certifies it against §11". Which of the two is the goal decides whether the
diagnostics must be designed before the implementation is scheduled (§9 item 8). The draft does
not pre-empt that decision: it has no diagnostics switch.

### 4.4 Memory and error conventions **(design)**

- **Inputs** are caller-owned fixed-size POD (`Crystal`, `FiberProblem`, `ContinuationOptions`)
  plus the caller's `faces` array. The library does not keep any pointer past the call.
- **Outputs with a size unknown in advance** (segments; accepted poses) are **library-allocated
  and released by an explicit function**, the convention this tree already uses for
  `LUMICE_Scene`, `LUMICE_ResultFrame` and the test surface's render-domain mask (a result struct
  with typed pointers into an opaque `storage` block). No new ownership model.
- On any error return the output struct is zero-filled, so the matching `Release*` call is always
  safe; `Release*` on a zeroed struct or a NULL pointer is a no-op.
- **Batch**: the caller allocates an array of `count` result structs; each element is filled and
  released independently. A problem-level failure (bad seed, TIR, budget) is that element's
  `status`/`reason`, not the call's return code; the call's return code is only for errors in the
  call itself (NULL arguments, `count < 0`, an invalid crystal or options block shared by all
  elements).
- **Status is closed, reason is open.** `status` is exactly one of four values, matching LI §9.4,
  and is part of the stable ABI. `reason` codes are grouped by status (0, 100+, 200+, 300+), and
  the set is **not exhaustive across versions**: a caller must handle an unknown reason inside a
  known status, and adding a reason value is not an incompatible change. This is the trade-off
  taken against LI's contract still evolving (its §12 lists open items): the four statuses are
  the part stable enough to freeze; the fine taxonomy is mirrored for usefulness but promised only
  as "known values today". `LUMICE_ANALYTIC_REASON_UNKNOWN` covers an extension reason the C enum
  cannot name. Precedence follows LI §9.4 (a known domain event outranks the numerical symptom it
  caused: a non-finite value after a negative Snell discriminant is `TIR_BOUNDARY`, not
  `NON_FINITE`); the implementation must apply it, not report whichever check fired first.

### 4.5 Header draft

```c
/* lumice_analytic.h -- DRAFT for review (doc/analytic-api.md section 4.5). Not a file in src/.
 *
 * The published analytic interface of Lumice. Independent of lumice.h: it includes nothing from
 * it and uses none of its types (no Scene/Server handles, no Distribution, no RNG).
 * 0.x is experimental: layouts may change between versions (doc/analytic-api.md section 8).
 *
 * Conventions: frames, face numbers and the pose chain are those of doc/coordinate-convention.md;
 * this header passes them, it does not define them. Symmetry: every function takes one concrete
 * face sequence and performs no symmetry reduction (doc/analytic-api.md section 3).
 */
#ifndef LUMICE_ANALYTIC_H_
#define LUMICE_ANALYTIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LUMICE_ANALYTIC_API_VERSION 1

typedef enum LUMICE_ANALYTIC_ErrorCode_ {
  LUMICE_ANALYTIC_OK = 0,
  LUMICE_ANALYTIC_ERR_NULL_ARG,
  LUMICE_ANALYTIC_ERR_INVALID_VALUE,   /* non-finite / non-unit / out-of-range argument */
  LUMICE_ANALYTIC_ERR_INVALID_CONFIG,  /* crystal fails the closed-form validity gate */
  LUMICE_ANALYTIC_ERR_UNKNOWN,
} LUMICE_ANALYTIC_ErrorCode;

/* Library version at run time; compare with LUMICE_ANALYTIC_API_VERSION to detect a header/library
 * mismatch. Also the minimal function the build/packaging chain can be proven with before any
 * module exists. */
int LUMICE_ANALYTIC_GetApiVersion(void);

/* ---------------------------------------------------------------------------------------------
 * Crystal: one deterministic closed-form crystal. Field semantics: doc/configuration.md
 * (prism / pyramid). Fields unused by `kind` must be zero (else ERR_INVALID_VALUE). Scale-free.
 * ------------------------------------------------------------------------------------------- */
typedef enum LUMICE_ANALYTIC_CrystalKind_ {
  LUMICE_ANALYTIC_CRYSTAL_PRISM = 0,
  LUMICE_ANALYTIC_CRYSTAL_PYRAMID = 1,
} LUMICE_ANALYTIC_CrystalKind;

typedef struct LUMICE_ANALYTIC_Crystal_ {
  int kind;                 /* LUMICE_ANALYTIC_CrystalKind */
  double height;            /* prism: height; pyramid: prism-band height (prism_h) */
  double face_distance[6];  /* ratio of the regular apothem; may be negative */
  double upper_h;           /* pyramid only */
  double lower_h;           /* pyramid only */
  double upper_wedge_deg;   /* pyramid only; final angle, convert Miller indices first */
  double lower_wedge_deg;   /* pyramid only */
} LUMICE_ANALYTIC_Crystal;

/* ---------------------------------------------------------------------------------------------
 * Single-pose path evaluation.
 * ------------------------------------------------------------------------------------------- */
typedef struct LUMICE_ANALYTIC_PathEvaluation_ {
  int valid;                              /* 1 iff the path is realisable at this pose */
  double outgoing_direction[3];           /* world, propagation crystal -> observer */
  double fresnel_transmission;            /* T_entry * prod(R_k) * T_exit, R_k = 1 under TIR */
  int segment_count;                      /* face_count + 1 (incident ... outgoing) */
  const double* segment_directions;       /* segment_count * 3, body-frame propagation */
  const double* interface_transmittances; /* face_count: T at entry/exit, R at internal hits */
  void* storage;                          /* opaque; LUMICE_ANALYTIC_ReleasePathEvaluation */
} LUMICE_ANALYTIC_PathEvaluation;

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluatePath(
    const LUMICE_ANALYTIC_Crystal* crystal,
    const int* faces, int face_count,       /* concrete Lumice face numbers */
    double refractive_index,
    const double incident_direction[3],     /* world, propagation sun -> crystal */
    const double pose[9],                   /* row-major rotation, body -> world */
    LUMICE_ANALYTIC_PathEvaluation* out);   /* zero-filled on error */

void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval); /* NULL-safe */

/* ---------------------------------------------------------------------------------------------
 * Fiber continuation from a caller-supplied seed. Seed search is not in v0
 * (doc/analytic-api.md section 4.2).
 * ------------------------------------------------------------------------------------------- */
typedef struct LUMICE_ANALYTIC_FiberProblem_ {
  const int* faces;
  int face_count;
  double refractive_index;
  double incident_direction[3];  /* world unit vector, propagation sun -> crystal (LI's s) */
  double target_direction[3];    /* world unit vector, propagation crystal -> observer (LI's d) */
  double seed_pose[9];           /* row-major rotation, body -> world; required */
} LUMICE_ANALYTIC_FiberProblem;

/* NULL => reference defaults. Defaults are documented with their convergence evidence when the
 * module is implemented (doc/analytic-api.md section 9). A zero field also means "default". */
typedef struct LUMICE_ANALYTIC_ContinuationOptions_ {
  double seed_residual_tolerance;
  double step_initial;
  double step_min;
  double step_max;
  int max_accepted_steps;
  int closure_min_steps;
  double closure_pose_tolerance;
} LUMICE_ANALYTIC_ContinuationOptions;

/* Closed set: exactly one of these, as in LI's phase1-math-contract.md section 9.4. */
typedef enum LUMICE_ANALYTIC_FiberStatus_ {
  LUMICE_ANALYTIC_FIBER_CLOSED = 0,
  LUMICE_ANALYTIC_FIBER_EVENT_TERMINATED = 1,
  LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE = 2,
  LUMICE_ANALYTIC_FIBER_BUDGET_EXHAUSTED = 3,
} LUMICE_ANALYTIC_FiberStatus;

/* OPEN set: values are grouped by status (0 / 100+ / 200+ / 300+); later versions may add values.
 * Callers must handle an unknown value inside a known status. Precedence: a known domain event
 * outranks the numerical symptom it caused (LI section 9.4). */
typedef enum LUMICE_ANALYTIC_Reason_ {
  LUMICE_ANALYTIC_REASON_UNKNOWN = -1,
  LUMICE_ANALYTIC_REASON_CLOSED_LOOP = 0,
  LUMICE_ANALYTIC_REASON_TIR_BOUNDARY = 100,
  LUMICE_ANALYTIC_REASON_BRANCH_BOUNDARY = 101,
  LUMICE_ANALYTIC_REASON_PATH_INFEASIBLE = 102,
  LUMICE_ANALYTIC_REASON_VISIBILITY_BOUNDARY = 103,
  LUMICE_ANALYTIC_REASON_CHART_BOUNDARY = 104,
  LUMICE_ANALYTIC_REASON_RANK_LOSS = 105,
  LUMICE_ANALYTIC_REASON_TOPOLOGY_AMBIGUITY = 106,
  LUMICE_ANALYTIC_REASON_CORRECTOR_FAILURE = 200,
  LUMICE_ANALYTIC_REASON_LINEAR_SOLVE_FAILURE = 201,
  LUMICE_ANALYTIC_REASON_NON_FINITE = 202,
  LUMICE_ANALYTIC_REASON_STEP_UNDERFLOW = 203,
  LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT = 204,
  LUMICE_ANALYTIC_REASON_STEP_BUDGET = 300,
  LUMICE_ANALYTIC_REASON_ARCLENGTH_BUDGET = 301,
  LUMICE_ANALYTIC_REASON_EVALUATION_BUDGET = 302,
} LUMICE_ANALYTIC_Reason;

typedef struct LUMICE_ANALYTIC_FiberResult_ {
  int status;                                 /* LUMICE_ANALYTIC_FiberStatus */
  int reason;                                 /* LUMICE_ANALYTIC_Reason (open set) */
  int pose_count;                             /* N accepted samples */
  const double* poses;                        /* N * 9, row-major, body -> world */
  const double* crystal_frame_sun_directions; /* N * 3, u = R^T (-incident_direction) */
  const double* arclength_increments;         /* N - 1 */
  const double* residual_norms;               /* N */
  const double* tangents;                     /* N * 3 */
  void* storage;                              /* opaque; LUMICE_ANALYTIC_ReleaseFiberResult */
} LUMICE_ANALYTIC_FiberResult;

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiber(
    const LUMICE_ANALYTIC_Crystal* crystal,
    const LUMICE_ANALYTIC_FiberProblem* problem,
    const LUMICE_ANALYTIC_ContinuationOptions* options, /* NULL => reference defaults */
    LUMICE_ANALYTIC_FiberResult* out_result);           /* zero-filled on call error */

/* One crystal, one options block, `count` independent problems (typically one path swept over
 * many target directions). out_results: caller-allocated array of `count`; each element is filled
 * and released independently. A per-problem failure is that element's status/reason; the return
 * code reports only call-level errors. */
LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiberBatch(
    const LUMICE_ANALYTIC_Crystal* crystal,
    const LUMICE_ANALYTIC_FiberProblem* problems, int count,
    const LUMICE_ANALYTIC_ContinuationOptions* options,
    LUMICE_ANALYTIC_FiberResult* out_results);

void LUMICE_ANALYTIC_ReleaseFiberResult(LUMICE_ANALYTIC_FiberResult* result); /* NULL-safe */

/* ---------------------------------------------------------------------------------------------
 * Logging: the library writes nothing by default. The host installs a callback to receive the
 * engine's diagnostics, including crystal-construction warnings (doc/analytic-api.md section 6).
 * Call before any computation, from one thread; not safe concurrently with computation.
 * The callback itself may be invoked from any thread that calls into this library.
 * ------------------------------------------------------------------------------------------- */
typedef enum LUMICE_ANALYTIC_LogLevel_ {
  LUMICE_ANALYTIC_LOG_TRACE = 0,
  LUMICE_ANALYTIC_LOG_DEBUG,
  LUMICE_ANALYTIC_LOG_VERBOSE,
  LUMICE_ANALYTIC_LOG_INFO,
  LUMICE_ANALYTIC_LOG_WARNING,
  LUMICE_ANALYTIC_LOG_ERROR,
} LUMICE_ANALYTIC_LogLevel;

typedef void (*LUMICE_ANALYTIC_LogCallback)(LUMICE_ANALYTIC_LogLevel level,
                                            const char* logger_name,
                                            const char* message);

void LUMICE_ANALYTIC_SetLogCallback(LUMICE_ANALYTIC_LogCallback callback); /* NULL => silent */

#ifdef __cplusplus
}
#endif

#endif /* LUMICE_ANALYTIC_H_ */
```

Changes from a literal transcription of LI §9, each deliberate: no diagnostics switch in the
options (§4.3); reasons numbered explicitly so appending one never renumbers another (§4.4);
`EvaluatePath` takes `incident_direction` because the outgoing direction depends on it;
`GetApiVersion` exists so the build, export and packaging chain can be proven end to end before
the first real module lands.

---

## 5. Conventions, errors, threading **(design)**

### 5.1 Conventions are cited, not defined

Coordinate frames, face numbering and the pose chain are the primitive layer's conventions —
`doc/coordinate-convention.md` §1 (crystal frame, face numbers), §2 (world frame), §6 (rotation
chain) — and are among the objects the two repositories cross-check. LI maps onto them in its
`docs/conventions.md` without restating them. This interface does the same: it passes values in
those conventions and never redefines them. A convention change in that document is an
incompatible change of this interface even if no signature moves, and bumps
`LUMICE_ANALYTIC_API_VERSION`.

### 5.2 Errors

`LUMICE_ANALYTIC_ErrorCode` is this header's own enum, not `LUMICE_ErrorCode`: the names that
exist in both (`OK`, `ERR_NULL_ARG`, `ERR_INVALID_VALUE`, `ERR_INVALID_CONFIG`, `ERR_UNKNOWN`)
mean the same thing, but the JSON-, file- and server-related codes of `lumice.h` have no meaning
here, and sharing the type would mean including `lumice.h`. Numerical outcomes of a computation
are never error codes; they are `status`/`reason` (§4.4).

### 5.3 Thread safety and re-entrancy

- The computation functions (`EvaluatePath`, `TraceFiber`, `TraceFiberBatch`, the `Release*`
  functions) are **re-entrant and safe to call concurrently** on distinct outputs. They hold no
  mutable state between calls, and the library **starts no threads** of its own.
- `SetLogCallback` writes process-wide state (the sink and the callback pointer). It is an
  initialisation call: made once, before computation, from one thread. The same holds for
  `LUMICE_SetLogCallback` in `lumice.h` today, whose first-call registration is an unsynchronised
  static flag (`src/server/c_api.cpp:127-139`).
- **To verify during implementation, not verified here**: that the closed-form geometry and optics
  code the module will call (`geo3d_closedform.cpp`, `crystal.cpp`, `optics.cpp`) holds no
  thread-unsafe function-local cache. The re-entrancy promise is conditional on that check.
- "Links all of `lumice_obj`" does not mean "goes through the simulator". The module calls the
  closed-form geometry and optics directly; it must not route through `Simulator`, the worker
  pool or the RNG infrastructure just because they are linked in. Those carry threads and state
  this interface promises not to have.

### 5.4 Precision

The ABI is `double` throughout, because continuation's residual tolerances are set in double. The
engine's existing closed-form geometry takes `float` (`ComputeClosedFormPrism(float h, const float
dist[6])`, `geo3d_closedform.hpp:195`), and its optics runs in `float` for the MC hot loop. Whether
the module reuses those kernels, promotes them, or evaluates its own path in double is the
implementation's decision, bound by one criterion: the reference tolerances in the options must be
achievable, demonstrated against LI's float64 evaluator. `doc/numerical-robustness.md` already
asks for double precision in geometry generation.

---

## 6. Logging **(owner direction, design detail)**

The library writes nothing by default; the host receives diagnostics through
`LUMICE_ANALYTIC_SetLogCallback`. The mechanism is built with the log-sink work (the other
binaries' behaviour does not change).

**Does the first module have warning paths a silent default would swallow? Yes.** Checked in the
code the module will call, all through the `LOG_WARNING` macro family (`src/util/logger.hpp:122`,
routed to the global logger, which forwards to the process-wide shared sink):

| Where | When |
|---|---|
| `src/core/crystal.cpp:361`, `:412` | a prism or pyramid fails the closed-form validity gate and is treated as degenerate (empty crystal) |
| `src/core/geo3d_closedform.cpp:1003` | a pyramid cone has no cross-section at the requested inset; it is degraded to its apex point |
| `src/core/geo3d_closedform.cpp:1226` | pyramid face slots keep fewer than 3 vertices and are dropped, leaving the surface open |
| `src/core/geo3d.cpp:417`, `:510` | the legacy coefficient path finds a zero-volume crystal or an empty pyramid region |

Today these reach the default `stderr` console sink that every engine binary inherits
(`GetSharedSink`, `src/util/logger.hpp:42-51`), at the global logger's default level, which lets
warnings through. A library that removes that sink and has no callback installed would turn all
of them into silence. Two consequences for the design:

1. **The callback must carry these warnings**, at `LUMICE_ANALYTIC_LOG_WARNING`, with the
   message text unchanged — they name the input and the remedy, which is what a researcher
   debugging a crystal needs.
2. **A silent default must not hide an unusable result.** The degenerate-crystal case is
   therefore also an error code (`LUMICE_ANALYTIC_ERR_INVALID_CONFIG`, §4.1): with or without a
   callback, the caller cannot mistake an empty crystal for a valid one. The two *degrading*
   cases (apex collapse, dropped face) still produce a crystal, and in v0 they are reported only
   through the log. Whether they also become data on the result is left to the implementation
   (§9 item 9) — they are rare, and a cone collapsing onto its apex is a legitimate shape, not an
   error.

If the module is later extended to call code beyond the closed-form geometry and optics (for
example a higher-level crystal factory), this table must be re-checked; it describes the call
graph planned today.

---

## 7. Relation to `lumice.h`

- `lumice_analytic.h` does **not** include `lumice.h`, and `lumice.h` does not include it. The two
  headers share no type. A consumer of one never compiles against the other.
- `lumice.h` is not published by this work. It moves to explicit exports too **(owner)**, but
  stays an internal interface of Lumice's own binaries.
- **The export set of `liblumice_analytic` is exactly the `LUMICE_ANALYTIC_*` functions.** This
  does not happen by itself: `lumice.h` marks its declarations `#pragma GCC visibility
  push(default)` (`lumice.h:15`), and `c_api.cpp` — part of `lumice_obj` — includes it, so every
  `LUMICE_*` function is compiled with default visibility into the objects. A shared library linked
  from those objects with `-fvisibility=hidden` alone would export the whole `lumice.h` surface
  next to the analytic one, and on Windows `WINDOWS_EXPORT_ALL_SYMBOLS` would export everything.
  The guarantee comes from the **per-library export list** built with the target (a symbol list /
  version script / `.def` naming only `LUMICE_ANALYTIC_*`), and its test is mechanical: list the
  dynamic symbol table of the built library and assert that every exported name matches
  `\bLUMICE_ANALYTIC_` and that at least one does.
- Both libraries run the same objects. A behaviour change in shared engine code reaches both; that
  is intended (one implementation per semantics), and it is why §5.1 treats a convention change as
  an interface change.

---

## 8. Version **(design; policy text left to the packaging work)**

- **An independent counter**, `LUMICE_ANALYTIC_API_VERSION`, not tied to `LUMICE_API_VERSION`.
  The two surfaces change at rates an order of magnitude apart (`lumice.h` is at 449 after the
  classified churn in `doc/api-layering-and-product-lines.md` §8), and a consumer of one must not
  see the other's churn as a version bump.
- Same style as `LUMICE_API_VERSION`: a single integer, bumped on every incompatible change, with a
  one-line note per bump at the top of the header. Adding a function, an enum value to an open set
  (§4.4) or a field at the *end* of a result struct that the library allocates is compatible;
  anything else — including a convention change (§5.1) — is not. The runtime
  `LUMICE_ANALYTIC_GetApiVersion()` lets a binding detect a header/library mismatch.
- **0.x is experimental** until the first real module is in LI's use. The wording of the
  compatibility promise, the semver mapping of the integer, ABI and deprecation policy, and what
  1.0 commits to are written with the packaging work.

---

## 9. Open items, each with its owner

| # | Item | Decided by / when |
|---|---|---|
| 1 | Seed search (discovery) in scope for v0? v0 says no (§4.2). If yes, LI first writes discovery's contract to §5–§10 standard. | Owner, reviewing this document |
| 2 | Reference defaults of `ContinuationOptions`, each linked to convergence evidence (LI §10.1). | The first-module implementation |
| 3 | A refractive-index convenience function (Sellmeier). Default: not exposed (§4.2). | The first-module implementation, on LI's actual need |
| 4 | Semver, ABI and deprecation policy text; what 1.0 commits to. | The packaging and version-policy work |
| 5 | Export-list mechanism on each platform, Windows export path, header location, prefix gate in `check_policies.py`, the stripping flag. | The target and export-list work |
| 6 | Callback forwarding implementation; removing the console sink only in this library. | The log-sink work |
| 7 | External consumer smoke test (C + Python ctypes, install tree only), with `symmetry_semantics` in any fixture. | The external-consumer smoke test |
| 8 | **Does `FiberResult` need LI §9.3's diagnostics** (`jacobian_`/`step_`/`branch_`/`closure_diagnostics`, `terminal_payload`, `conventions`, `weight_observables`, `component_scope`) and the entry cross-section `A_P`? "LI reads point lists" → no, extend on demand; "LI certifies this library as its continuation backend against its §11" → yes, designed before the implementation is scheduled. | Owner, reviewing this document |
| 9 | Surface crystal *degradation* (apex collapse, dropped face) as result data, not only as a log line (§6). | The first-module implementation |
| 10 | Parallelism inside `TraceFiberBatch` (v0: none; caller parallelises). Revisit only with a measured batch where binding-side threading is the bottleneck. | The first-module implementation |
| 11 | Re-read LI `docs/phase1-math-contract.md` §9 before implementing: this draft mirrors it as of 2026-09-28, and LI's §12 lists open items that may move it. LI's §9.1 also says problem construction "MUST not import or invoke Lumice" — a rule LI revises on its side when it adopts this library. | The first-module implementation (and LI, on adoption) |
| 12 | Verify no thread-unsafe static cache in the called geometry/optics code (§5.3). | The first-module implementation |
