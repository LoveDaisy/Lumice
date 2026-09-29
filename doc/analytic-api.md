# `liblumice_analytic`: the published analytic interface

> Status: **partly built** (2026-09-29). As built: the target and its per-library export list
> (§2.5), logging handed to the host (§6), and packaging with a `find_package` config plus the
> version policy (§8), and an external-consumer smoke test that builds a C program and loads the
> library from Python using the install tree alone (§8.7); and the whole of the first module's v0
> (§4.3): `LUMICE_ANALYTIC_EvaluatePath` (header version 2), with its precision (§5.4) and
> thread-safety (§5.3) decisions, fiber continuation `LUMICE_ANALYTIC_TraceFiber[Batch]` (version
> 3) and component discovery `LUMICE_ANALYTIC_DiscoverComponents` (version 4). The module serves the
> Analyze workspace's first phase (`doc/raypath-analysis.md` §5.1.8). The library is not in any
> download package yet: that is §8.8's checklist, not done.
>
> Every decision below is marked either **(owner)** — ruled by the owner on 2026-09-28, not open
> for re-derivation — or **(design)** — this document's own judgement, open to the owner's review.
> The header in §4.5 is a draft for review; it is not a file in the tree. Its `EvaluatePath` part
> is now in `src/include/lumice_analytic.h` (§4.5 lists what the built header adds).

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

### 2.5 Produced where, exported how (as built)

What the artifact list is: one shared library per platform (`liblumice_analytic.so`,
`liblumice_analytic.dylib`, `lumice_analytic.dll` + import library) plus `lumice_analytic.h`,
from a Release, non-CUDA, baseline-ISA configure. The mechanisms, as built with the target:

- **Target**: `lumice_analytic` in the root `CMakeLists.txt`, gated on
  `BUILD_SHARED_LIBS AND NOT LUMICE_CUDA_ENABLED`, linking `lumice_obj` PRIVATE, with the stripping
  flag of §2.2 in Release and MinSizeRel (`-dead_strip` / `--gc-sections` / `/OPT:REF`). Its own
  code lives in `src/analytic/`, outside `lumice_obj`, so the other two libraries never carry it.
  Installed with a CMake package config (§8.4); the install layout is a contract (§8.7).
- **Header**: `src/include/lumice_analytic.h`, next to `lumice.h`. Today it holds only
  `LUMICE_ANALYTIC_API_VERSION` and `LUMICE_ANALYTIC_GetApiVersion`; the §4 module arrives with
  its implementation.
- **Export list, every platform**: one list per shared library, generated from that library's
  headers by `scripts/gen_export_list.py` and passed as a GNU version script, an ld64
  `-exported_symbols_list`, or a Windows `.def` (`lumice_apply_export_list`). This replaced both
  `WINDOWS_EXPORT_ALL_SYMBOLS` (which exported every engine symbol from the DLLs) and
  `lumice.h`'s `#pragma GCC visibility`, for all three libraries.
- **Header macros**: `LUMICE_API` / `LUMICE_TEST_API` / `LUMICE_ANALYTIC_API` on each declaration.
  They do not export: on GCC/Clang they give default visibility (the objects are compiled with
  `-fvisibility=hidden`, and a hidden symbol cannot be listed), on Windows they are the
  consumer-side `dllimport`, switched on by an INTERFACE definition of the DLL's target so that a
  target linking the objects directly sees an empty macro. The generator refuses a declaration
  without its marker.
- **Prefix gate**: `check_policies.py` rule `analytic-symbol-scope` — `LUMICE_ANALYTIC_` appears
  under `src/` only in the header and `src/analytic/`, and the header names no other `LUMICE_`
  identifier (§7).
- **Test**: `test/e2e-correctness/test_export_symbol_scope.py` compares each built library's
  export table (`nm -D` / `nm -gU` / `dumpbin /exports`) with its headers and loads it; it runs in
  CI on Linux, macOS, and Windows under both cl.exe and clang-cl.

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

`EvaluatePath` is built (§4.3–§4.5, `src/analytic/`); the fiber functions are signatures and data
conventions only. Sources: LI
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
  Known limitation: on deliberately constructed degenerate inputs the closed-form pyramid can still
  yield an open surface and pass `IsValidClosedFormPyramid`. This is a backlog item the owner
  decided not to pick (2026-09-04); no user-configurable shape has been seen to hit it. No extra
  gate is added.
- **As built: which faces exist is the engine's float decision; normals are double.** The
  evaluator builds the crystal with the simulator's own factory (`Crystal::CreatePrism` /
  `CreatePyramid`, float arguments), so face presence and the validity gate are exactly those of a
  simulated crystal — including the gate's warning, which reaches the log callback (§6). Face
  normals are then taken in double from the same per-slot plane formula (§5.4). A face number the
  crystal does not have (unknown, or absent from this shape — `13`–`18` without an upper cone) is
  `ERR_INVALID_VALUE` when named in a path, as LI's `normalize_faces` raises for it. Heights fold to
  their absolute value, as the simulator folds them.
- **Degradation as data (§9 item 9): not in v0.** A dropped face is visible only as that face
  number being rejected; an apex collapse is invisible in the result (it changes no face normal,
  only which faces exist and where). Both are still logged. A result field for it belongs with the
  wave-2 diagnostics extension (§4.3, §10), where `TraceFiber`'s result gains its `struct_size`
  fields; `PathEvaluation` can take the same field then.

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

**Scope ruling (author, 2026-09-28): v0 includes seed search.** "Single-path inversion" in v0 is
seed search (discovery) **plus** continuation from a seed. LI's contract lists `seed` as a required
`FiberProblem` field (§9.1) and its architecture lists seed search and predictor–corrector
continuation as separate duties of the fiber solver (LI `docs/overview.md` §4 item 4); both are in.
The normative source for discovery is the discovery section of LI's
`docs/phase1-math-contract.md` (§9.5 "Discovery interface semantics", §9.5.1–§9.5.10; the v0 output
subset is §9.5.8), written to the same standard as §5–§10 (LI task `discovery-contract`). This
document cites it by path and does not restate it. The library does **not** require the Monte Carlo
side to record ray poses: the Analyze all-sky map is low resolution, so seed density can start low
and be refined progressively, which is not an interaction blocker (§9 item 1). Refinement is not
monotone by itself (LI §9.5.7), so it passes the sparser components forward as warm seeds; §4.3
("`DiscoverComponents` as built") has the calling convention.

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
- **`LUMICE_ANALYTIC_DiscoverComponents`** — the seeds, for one path and one target: every
  component the sample reaches, each traced once with `TraceFiber`'s solver and classified closed
  or arc, plus the candidates that could not be classified. Analyze's core action, "click a sky
  point, get its fibers", is this call; without it the product and LI would each write their own
  way of finding a seed.

**`EvaluatePath` as built** (`src/analytic/path_evaluation.{hpp,cpp}`; the C wrapper in `analytic_api.cpp`). The
kernel has two stages, split by cost and by the kind of failure each can report:
`BuildFaceNormals` once per crystal (validation, the engine's factory, a fixed-size table of
double normals by slot — the only stage that can say `ERR_INVALID_CONFIG`), and `EvaluatePath` per
pose (no allocation, no crystal construction, returns only `valid`). Continuation and seed search
call the kernel, not the C function, so a fiber pays for the crystal once and never allocates per
point. `valid` gates exactly LI's `validity_margin_names` with LI's margin expressions (entry
incidence cosine and Snell discriminant, each internal face's incidence cosine, exit incidence
cosine and Snell discriminant, all `> 0`); an internal TIR discriminant gates nothing. Validity is
direction-level: whether the ray at that pose meets the faces' finite polygons is not checked —
LI's evaluator does not check it either, and the fiber problem is a direction-level one. When
`valid` is 0 the call succeeds with directions, transmittances and `segment_count` zero and both
pointers NULL. Input checks use LI's reference tolerances (`unit_tolerance` = `rotation_tolerance`
= `1e-10`, LI `docs/phase1-math-contract.md`): the incident direction's length within `1e-10` of 1,
`RᵀR` within `1e-10` of `I` entrywise and `det R > 0`.

Checked by: `test/unit-correctness/analytic/test_path_evaluation.cpp` (physics restated
independently — normals from the wedge geometry, Snell as a sine ratio, Fresnel in `r_s`/`r_p`
form, reversed-path reciprocity, both TIR cases, each validity gate, concurrency — plus agreement
with the simulator's float `HitSurface` chain and four pins from LI's float64 evaluator);
`test/e2e-correctness/test_analytic_evaluate_path.py` (the C ABI: codes, zero-fill, storage,
the warning path); the external-consumer smoke test (one call from the install tree, C and
ctypes). One-time comparison with LI's `parity_export.evaluate_path` while writing it: 24000
random poses over six (crystal, path) pairs — prisms `3-5`, `3-5-6-7`, a pyramid `13-15-26-28`,
an irregular pyramid (unequal cones, unequal `face_distance`) `13-25`, `3-1-26`, `13-6-24` — no
validity mismatch, largest deviation 0.054 of LI's per-pose `kinematic_atol` (itself `1e-12`,
widened near a Snell boundary). The standing check is the parity fixtures LI exports (§10).

**`TraceFiber` as built** (API version 3). The core is `src/analytic/fiber_continuation.{hpp,cpp}`,
a port of LI's reference solver (`src/lumice_integral/continuation.py` at LI `bfbd042`) function for
function — each C++ function names its LI counterpart — against LI `docs/phase1-math-contract.md`
§5–§10 as of that revision, including §6.4's step-aware closure trigger (the seed distance of a
crossing edge is measured at the section zero bisected on that edge, 52 halvings, and the closure
corrector starts there). The core does not know optics: it is a template over a map with a
`Domain` (validity, event margins, typed event) and a `Direction`, as LI's `FiberProblem` takes a
`domain_and_event_evaluator` and a `direction_evaluator`. The ice-crystal path is one such map,
`path_fiber.{hpp,cpp}` (LI `optics.path_problem`: the event margins are the validity margins, and a
Snell discriminant at or below `1e-8` is already `tir_boundary`); the unit tests drive the same core
with LI's analytic maps. Both `EvaluatePath` and the fiber map run one ray chain,
`path_chain.hpp`'s `TracePathChain`, so there is still one evaluator; moving `EvaluatePath` onto it
left its output identical bit for bit (a hash over 24000 poses on six paths).

- **Derivatives.** Forward-mode dual numbers (`jet.hpp`, `Jet<3>`; no AD framework,
  `doc/raypath-analysis.md` §5.1.6). The direction map is evaluated with the pose
  `R exp([delta]_x)` built from a `Jet<3>` delta, through the same Rodrigues `exp` as LI (Taylor
  branch below `theta^2 = 1e-8`), both for the residual Jacobian `A` (at `delta = 0`) and for the
  bordered Newton system (at the current iterate's `delta`, as LI's `jax.jacfwd`); the closure
  corrector's border, the section coordinate, is differentiated the same way. No derivative is
  written by hand. The small linear algebra is in `so3.hpp`: `A`'s singular values from Lagrange's
  identity (`sigma1 sigma2 = |r0 x r1|`, so a `sigma2` at the `1e-8` rank gate keeps its relative
  precision), the bordered system's 2-norm condition number from a one-sided Jacobi SVD, and a
  pivoted solve.
- **Options** (C field → LI field → reference default of LI §10.1, whose text and the tests it
  names are the convergence evidence; zero means the default):
  `seed_residual_tolerance` → `residual_tolerance` `1e-11` (seed gate, corrector root and
  acceptance alike); `step_initial` / `step_min` / `step_max` → `initial_step` / `minimum_step` /
  `maximum_step` `0.04` / `1e-5` / `0.12`; `max_accepted_steps` → `maximum_accepted_steps` `4000`;
  `closure_min_steps` → `closure_minimum_steps` `3`; `closure_pose_tolerance` → `closure_distance`
  `0.08`. Every other LI option is fixed at its §10.1 value (`ContinuationParams`). No default
  deviates from LI.
- **Seed orientation.** `initial_tangent_sign = +1` is `A`'s row 0 × row 1 times the handedness of
  the target basis, `(b0 x b1) . d`: a change of basis by any `Q` in O(2) scales both factors by
  `det Q`, so the traversal order does not depend on the chart (LI §11 C03). It is not LI's `+1`,
  which is its LAPACK build's SVD sign; the parity recipe is orientation-free for that reason.
- **Errors and results.** Anything that belongs to one problem of a batch — a bad face number or
  count, a non-finite or non-unit direction, a seed that is not a rotation, a bad index or sign — is
  that element's `NUMERICAL_FAILURE` / `INVALID_NUMERICAL_INPUT` with `pose_count = 0`; the call's
  return code is for the call (NULL arguments, `count < 0`, the shared options block and crystal, a
  `faces == NULL` with `face_count > 0`, the §8.2 stride). `EvaluatePath` reports the same inputs as
  call errors because it evaluates one pose; both headers say so. A closed result's last sample is
  the closure-corrected pose (it replaces the last step's end, as in LI), N = 0 when the seed is
  rejected, N = 1 when the first trial ends the trace, and every zero-length array is NULL. The
  result block is `17 N - 1` doubles. Face sequences are bounded at 64, the simulator's `kMaxHits`,
  for `EvaluatePath` too.

Checked by: `test/unit-correctness/analytic/test_fiber_continuation.cpp` (LI's conformance matrix,
§11, restated on analytic maps: C01–C04, C06's first-traversal cases, C07, C08, C10–C12, the step
controller), `test_path_fiber.cpp` (the map against `EvaluatePath` and against a central difference
that does not use `Jet`; LI's optical C03 / C05 / C06 / C08 cases, including the ch06 strip loops,
whose lengths match LI's recorded `1.645239` / `2.375620` / `3.111244` to `1e-6`; two parity-fixture
pins; concurrency), `test_jet.cpp`, `test_so3.cpp`, and `test/e2e-correctness/test_analytic_trace_fiber.py`
(the C ABI). One-time comparison with LI's own `trace_fiber` fixtures at `bfbd042` (all eight
exported cells, compared by LI `docs/analytic-parity-fixtures.md` §4): every one passes, and with
the orientations matched the accepted pose sequences are the same sequences, to at most `1.8e-13`
rad per pose, on all eight — one open arc (`3-5-6-7__near_boundary`) ends one sample earlier at
its TIR end. Largest residual on an accepted pose over those traces: `4.6e-12`, against the
`1e-11` gate. The standing check is the parity fixtures, once this repository replays them (§10).

**`DiscoverComponents` as built** (API version 4). LI `docs/phase1-math-contract.md` §9.5
(`reference-discovery-v1`, LI `bfbd042`) makes every step order and gate normative, so that two
backends given the same sample return the same components and counters; this is a port of LI's
`discovery.discover_components` and of the pieces of `s2_store` it reads, step for step
(`src/analytic/discovery.{hpp,cpp}`):

1. **Sample** (§9.5.2). The antipodal Fibonacci lattice of `sample_count` points
   `u = R^-1 s_hat`, generated inside the call in LI's evaluation order. The fields depend on the
   pose only through `u`, so each event is one `TracePathChain` at the identity pose with incident
   direction `-u`: validity, the body-frame outgoing direction `phi`, the Fresnel factor `T`, and
   `D = angle(phi, -u)`. No persisted sample format exists or is needed.
2. **Band** (§9.5.3). The events with `w = A T > 0` and `|D - delta| <= b`, inclusive at both ends,
   in increasing `(D, index)`. The entry measure `A` is evaluated only for events already in the
   band: `A` does not change `D`, so the band of the kept events is the kept events of the band, in
   the same order, at `|band|` entry measures instead of `N`. One streaming pass; only band events
   are stored.
3. **Pool, clusters, representative** (§9.5.3–§9.5.4). Extra seeds first in the caller's order,
   then the band; poses `R_i = W F_i^T`. Greedy geodesic clustering: the lowest unassigned index is
   the centre, members strictly closer than `r_c` to it (not transitive). An extra seed represents
   its cluster; otherwise the smallest offset `|D_i - delta|`, ties to the lowest index.
4. **Gauss-Newton and admissibility** (§9.5.4). At most 30 minimum-norm steps
   `R <- R exp(-A^T (A A^T)^-1 r)`, stopping once `|r| <= tau / 100`, with `A` from the same
   `Jet<3>` Jacobian `TraceFiber` uses; admissible iff `|r| <= tau`
   (`tau = residual_tolerance + relative_residual_tolerance`), every validity margin `> 0`, and
   `A > eps`. An inadmissible representative is dropped; its cluster's other members are not
   tried.
5. **Dedup, trace, classify** (§9.5.4–§9.5.5). A pose strictly closer than `eta` to a stored pose
   of an accepted component (both traces of an arc) is folded without a trace. Otherwise the
   forward trace; on one of the five arc events (TIR, branch, path-infeasible, visibility, chart
   boundary) the backward trace, and LI's six-row table decides component or incomplete candidate
   and which counter moves.

- **The finite crystal** enters only through the entry measure `A_P(R)`
  (`src/analytic/entry_measure.{hpp,cpp}`): the area, perpendicular to the incident direction, of
  the entry points whose internal ray meets every later face of the path inside its polygon (LI
  §7), by unfolding the path's faces into a corridor and clipping their projections along the
  internal direction. It is a port of LI's `geometry.entry_measure` over its corridor primitives,
  with LI's status names in LI's gate order, and it is a primitive-layer twin kept on purpose
  (§3, `doc/raypath-analysis.md` §5.1.6): LI keeps its own and the two are compared, not merged.
  The corners are the engine's float closed-form face polygons promoted to double —
  `BuildFaceNormals` returns them next to the normals — and `eps = 1e-6 * (shortest edge)^2` is
  LI's. Measured once against LI on 12000 random poses over three crystals (a prism with `3-5` and
  `3-5-6-7`, the asymmetric pyramid with `13-15-26-28`): the status agreed everywhere, and the
  value equalled 0.25 times LI's (this library's crystals are half LI's size in length) to within
  `1.4e-5` relative, the float corners' spread on the pyramid's smallest corridors. The value is
  not in the v0 result; the gate uses `> eps`, and a wave-2 weight can read the same kernel.
- **Deterministic; no seed parameter.** The lattice has no random numbers, so the result is fixed
  by the inputs, which is what "reproducible" asked for. A random-number seed would be a knob no
  sampler reads (LI's i.i.d. sampler, the contract's alternative, is not built); a later random
  sampler would arrive as a new field or function, with a version bump (§8.2).
- **Densification is not monotone, so the calling convention is warm seeds.** A denser sample can
  lose a component a sparser one found (LI §9.5.7: greedy clustering moves its centres as events
  are added, and an inadmissible representative is not replaced; LI's counterexample is the `D3h`
  prism's `5-3` at 43.0347°). Analyze's low-then-dense pattern is therefore: pass the sparser
  call's component seeds as `extra_seeds` of the denser call. An extra seed is only the
  Gauss-Newton start of its cluster — never traced on its own, never counted toward completeness —
  so it can only help that cluster converge onto a known component. The Fibonacci lattices of two
  sizes are not nested, so this pattern, not the sample, is what carries a component forward.
  Measured on `3-5` near its boundary (LI fixture `3-5__near_boundary`'s target): `1e5` gives 4
  components, `1e6` gives 5, and the warm `1e6` call keeps all 4 sparse ones.
- **`completeness` is procedural** (LI §9.5.6): `COMPLETE` means every admissible, non-folded
  candidate closed or became an arc, never that every component was found; a target with no
  candidate is `COMPLETE` with zero components. The post-hoc check LI offers (`check_band_coverage`,
  §9.5.6a) is not in v0.
- **Options** (C field → LI → reference default of LI §9.5.9; zero means the default, negative or
  non-finite is `ERR_INVALID_VALUE`): `sample_count` → `N`, `1000000`, at most
  `LUMICE_ANALYTIC_MAX_DISCOVERY_SAMPLE_COUNT` (`1e8`: the call cannot be cancelled, and `1e8` is about
  3 s of single-threaded sampling at the measured `28 ms` per `1e6`; above it is `ERR_INVALID_VALUE`); `band_half_width` (radians;
  LI's argument is in degrees) → `0.2°`; `cluster_radius` → `0.3` rad; `distance_threshold` →
  `closure_distance` of the call's continuation, `0.08` rad by default. LI requires `eta > 0`; here
  a negative one is rejected and zero selects the default, so the threshold in effect is always
  positive. The call's `ContinuationOptions` are the one trace policy (LI: "no separate discovery
  budget"). Every trace starts in the library's `+1` orientation, `TraceFiber`'s (§4.3 above).
- **Guarded where LI is not.** A target at `0` or `pi` from the incident direction (LI §9.5.3 needs
  a component of `d` normal to `s`, and LI's reference does not guard it, its §12) is
  `ERR_INVALID_VALUE`.
- **Two differences from LI, both inert on every LI fixture.** The entry measure's exit gate uses
  the call's refractive index where LI's uses its package constant `N_ICE = 1.31` (they agree at
  1.31, the index of every fixture). And this library's direction is NaN outside the path's
  domain, where LI's JAX evaluator returns whatever the formulas give; a Gauss-Newton iterate that
  leaves the domain therefore ends inadmissible here, where LI could in principle come back. No
  fixture reaches that branch.
- **Result layout.** `DiscoveryResult` (a `struct_size` struct, §8.2) holds the counters and two
  library-allocated arrays, components and incomplete candidates. Each element points to its
  traces: `forward`, and `backward` or NULL when no backward trace was run — never an embedded
  zeroed struct, whose status 0 would read as `CLOSED`. Pointers rather than embedded
  `FiberResult`s also keep the element layout fixed when `FiberResult` grows in wave 2. The pointed-to
  `FiberResult`s are views into the discovery block (their own `storage` is NULL); only
  `ReleaseDiscoveryResult` frees them.
- **Cost.** At the default `N = 1e6` the band takes 25–28 ms single-threaded (Apple M-series,
  release) and the rest of the call under 2 ms on LI's eight fixture scenes — the sampling pass, not
  the traces, is the cost. No thread is started (§5.3).

Checked by: `test/unit-correctness/analytic/test_discovery.cpp`, which states each LI conformance
row it covers — C15 (canonical pixel at `N = 1e6`: pool 5024, 6 clusters, 5 folded, LI's counts;
rows 225/226; three caustic pixels), C16 (the two boundary-hugging rows), C17 (`1-3` at 60°: two
arcs 0.82 rad apart), C18 (each classification branch on LI's capped analytic circle, the arc ends
of `1-3`, the one-pose arc of `3-1` at 64.7434° pinned as LI's known limitation), C19 (the `D3h`
prism differing only through `A`, an unlit member with an empty pool, LI's three pyramid paths at
eight deviations), C20 (cluster centres and strictness, representative, dedup strictness, warm
seeds near and far, the funnel identities on every result), C21 (a dark target, a starving budget,
the warm-seed densification above) — plus the bands of two LI parity fixtures point by point, the
pipeline on LI's own band reproducing LI's seeds to `1e-9`, and concurrency;
`test_entry_measure.cpp` (LI pins, an analytic case, the threshold); and
`test/e2e-correctness/test_analytic_seed_search.py` (the C ABI, one call against fixture
`3-5-6-7__random`, a sparse-then-dense round trip). Each strict or inclusive comparison was broken
on purpose once to see a test fail (cluster radius, band ends, dedup threshold, the entry-measure
gate). One-time comparison with LI's eight `seed_search` fixtures at `bfbd042` (`N = 1e5`): this
library's own band equals LI's exported band in size and order (`|du| <= 1.1e-16`,
`|dD| <= 1.4e-14`), and on both bands every funnel count and counter is equal, every component's
kind matches, and every seed lies within `1.3e-14` rad of LI's curve. The standing check is the
parity fixtures, once this repository replays them (§10).

**Batch shape.** LI's heavy use is one path swept over many targets (pixels or sample points)
with the crystal, the path and the sun fixed. The batch call takes one crystal and one options
block and an array of problems. The reason for a batch at all is call overhead: a ctypes call
costs on the order of microseconds, comparable to one short continuation, so per-pixel calls from
Python would be dominated by the binding. Whether the library also parallelises *inside* a batch
is left open (§9 item 10): v0 starts no threads (§5.3), and a ctypes caller can already run
batches from several threads, since ctypes releases the GIL for the duration of a foreign call.

**Result scope (author, 2026-09-28): v0 returns the point list only.** LI's `FiberResult` (§9.3)
also requires `jacobian_diagnostics`, `step_diagnostics`, `branch_diagnostics`,
`closure_diagnostics`, `terminal_payload`, `conventions`, `weight_observables` and
`component_scope`: the evidence LI's conformance matrix (§11) uses to decide whether a backend
satisfies §5–§10. v0 carries only the kinematic fields the product needs. Diagnostics and weights
enter in wave 2 as a `struct_size`-compatible extension of the result structs (§8.2), after LI's
explore `fiber-diagnostics-contract` has converged on their contract (§9 item 8). v0 has no
diagnostics switch.

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
  uint32_t struct_size;                   /* caller sets sizeof(*out) before the call (section 8.2) */
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
 * Fiber continuation from a seed. Seed search is in v0 (doc/analytic-api.md section 4.2); its
 * function family is added with the seed-search work.
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
  uint32_t struct_size;                       /* caller sets sizeof(*out_result) (section 8.2) */
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
 * code reports only call-level errors. `count == 0` is a legal empty batch: a no-op success that
 * touches nothing. `count < 0` is a call-level error and returns immediately without touching
 * out_results at all -- its writable length is unknown, so there is nothing safe to zero-fill.
 * For every other call-level error (NULL crystal, NULL problems, an invalid options block, with
 * `count >= 0`), every element of out_results is zero-filled, so Release* is safe to call on all
 * `count` elements regardless of which error path was taken. */
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
the first real module lands. Not from LI: the leading `struct_size` of the two caller-allocated
result structs, which lets a later version append a field without a version bump (§8.2; the
draft's `#include <stdint.h>` is implied).

**What the built header changes against this draft** (`src/include/lumice_analytic.h`, version 2).
The `ErrorCode`, `CrystalKind`, `Crystal`, `PathEvaluation`, `EvaluatePath` and
`ReleasePathEvaluation` declarations carry the draft's types, fields, order and signatures
unchanged. What was added: the `LUMICE_ANALYTIC_API` marker on each function (§2.5); an explicit
`#include <stdint.h>`; the version-note block (§8.1); and the contract comments this draft left to
§4.3 — when `valid` is 0 and what the result holds then, which inputs are `ERR_INVALID_VALUE` with
their tolerances, that a face absent from the crystal's shape is one of them, and that a prism's
unused fields must be zero. The draft's `LUMICE_ANALYTIC_API_VERSION 1` above is the draft's own
number, not the built one.

Version 3 adds the fiber declarations. They carry the draft's types, fields, order and signatures
with one deliberate difference: `FiberProblem` ends with `int initial_tangent_sign` (0 or `+1`,
`-1`). The draft had no way to trace the other orientation, which an arc needs — LI traces an arc
that ends on an event at both ends from the same seed twice (LI contract §8, §9.5.5) — and the sign
belongs to the problem, not to the options, because one batch typically holds both orientations of
the same seeds. `ContinuationOptions` stays exactly the draft's seven fields, with no `struct_size`:
it is an input struct, and §8.2 prices a change to one at a version bump. Added comments: the
element-level / call-level error split and the §8.2 stride rule on `TraceFiberBatch`, the N = 0 /
N = 1 cases and the closed-loop sample convention on `FiberResult`, each option's LI field and
default. `EvaluatePath` gained the `face_count <= 64` bound and `ERR_UNKNOWN` for an allocation
failure.

Version 4 adds discovery, which the draft did not have: `DiscoveryProblem` (the fiber problem's
path, index and directions without a seed, plus the extra seeds), `DiscoveryOptions` (the sampling,
clustering and dedup parameters of LI §9.5.9 — kept apart from `ContinuationOptions`, which is the
trace policy the call shares with `TraceFiber`), the closed sets `Completeness`, `ComponentKind`
and `IncompleteCause`, the `DiscoveredComponent` / `IncompleteCandidate` elements, which point to
their traces rather than embedding them, `DiscoveryResult` with `struct_size`, and
`DiscoverComponents` / `ReleaseDiscoveryResult`. Why each shape: §4.3, "`DiscoverComponents` as
built". Nothing existing changed.

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
  functions) are **re-entrant and safe to call concurrently** on distinct outputs (third bullet:
  verified for the code `EvaluatePath` reaches; the fiber functions must re-check any new callee). They hold no mutable state between calls
  themselves, and the library **starts no threads** of its own.
- `SetLogCallback` writes process-wide state (the sink and the callback pointer). It is an
  initialisation call: made once, before computation, from one thread. The same holds for
  `LUMICE_SetLogCallback` in `lumice.h` today, whose first-call registration is an unsynchronised
  static flag (`src/server/c_api.cpp:127-139`); the analytic library's registration is a
  function-local static initialiser, so concurrent first calls cannot attach its sink twice, and
  the callback pointer is swapped under the sink's own lock.
- **Verified (as built, 2026-09-29)**: the code `EvaluatePath` reaches holds no mutable static
  state. Its call graph is `src/analytic/path_evaluation.cpp` → `Crystal::CreatePrism` /
  `CreatePyramid` (`crystal.cpp`: the closed-form factory, `DeriveGeometricSymmetry`,
  `PopulateFromCfGeom`) → `geo3d_closedform.cpp`, plus `optics_shared.h`'s header-only Fresnel and
  the logger on the warning path. None of `geo3d_closedform.cpp`, `crystal.cpp`, `optics.cpp`,
  `geo3d.cpp`, `math.cpp` has a function-local `static` variable or a `thread_local`; the logger's
  statics (`util/logger.hpp`) are magic-static singletons, whose initialisation C++11 makes
  thread-safe, and spdlog's sinks lock internally. Each call builds its own `Crystal` and table on
  the stack or heap and shares nothing. Pinned by
  `PathEvaluationConcurrency.ConcurrentCallsMatchSerialResults` (eight threads, each building the
  crystal and evaluating, bit-identical to a serial run). The fiber functions add
  `src/analytic/{fiber_continuation,path_fiber}.cpp` and the headers `path_chain.hpp`,
  `so3.hpp`, `jet.hpp`: no `static` or `thread_local` variable in any of them; each call builds its
  own table and result vectors. Pinned by
  `PathFiberConcurrency.ConcurrentTracesMatchSerialResults` (eight threads, bit-identical to serial).
- "Links all of `lumice_obj`" does not mean "goes through the simulator". The module calls the
  closed-form geometry and optics directly; it must not route through `Simulator`, the worker
  pool or the RNG infrastructure just because they are linked in. Those carry threads and state
  this interface promises not to have.

### 5.4 Precision

The ABI is `double` throughout, because continuation's residual tolerances are set in double. The
engine's existing closed-form geometry takes `float` (`ComputeClosedFormPrism(float h, const float
dist[6])`), and its optics runs in `float` for the MC hot loop. Whether the module reuses those
kernels, promotes them, or evaluates its own path in double is the implementation's decision,
bound by one criterion: the reference tolerances in the options must be achievable, demonstrated
against LI's float64 evaluator. `doc/numerical-robustness.md` already asks for double precision in
geometry generation.

**As built (2026-09-29): one source, two precisions.** Three options were on the table: (a) make
the existing closed-form geometry and optics precision-parametric, float for the simulator and
double for this library; (b) call the float code and promote its results; (c) write a separate
double path. (c) would put a second implementation of a primitive in this repository, against the
sharing criterion of `doc/raypath-analysis.md` §5.1.6, and was only admissible if (a) and (b)
were both excluded by evidence.

- **(b) is excluded by measurement.** LI's float64 evaluator with each face normal rounded to
  float32 — a lower bound on (b)'s error, since the engine's pyramid normals also carry the float
  constants of the slope — against the same evaluator with exact normals, 300 valid poses per
  path, `n = 1.31`: prism `3-5` 3.1e-7, prism `3-5-6-7` 6.2e-7, pyramid 28° `13-15-26-28` 9.7e-7
  largest outgoing-direction deviation. The requirement is LI's `kinematic_atol` = `1e-12` (and
  continuation's default residual tolerance, `1e-11`): (b) misses it by five to six orders.
- **(a) is what is built**, and in three places only, because a path evaluation needs face
  normals and a Fresnel factor and nothing else of the geometry:
  1. The cone slope `a = (√3/4) / tan(wedge)` — `ClosedFormConeSlopeFromWedgeDeg<Real>`
     (`geo3d_closedform.{hpp,cpp}`): the template parameter is the precision of the two constants;
     the float instance is the expression the simulator always evaluated.
  2. The per-slot face plane `(a, b, c, d)` — `ClosedFormHexFacePlane`, double. The simulator's
     pyramid rounds it to float and normalises its `face_normal` from the rounded values (its
     historical order, kept); this library normalises the unrounded values, for a prism's slots
     0–7 as well. (The simulator's prism takes its normals from the `kHexFace*` direction tables
     instead; the two derivations agree to within an ulp, and the unit test holds the library's
     normals to the star directions at `1e-15`.)
  3. The Fresnel reflectance — `GetReflectRatioT<T>` in `core/shared/optics_shared.h`, the
     single-source header Metal, CUDA and the host trace share. `GetReflectRatio(float, float)`
     stays the wrapper every backend calls; the double instance exists on the host only.
  The float instances are unchanged to the bit: a hash over 72056 closed-form prism and pyramid
  results (every field) and `GetReflectRatio` outputs is identical before and after, a seeded
  render of two pyramid configurations is byte-identical, and the Metal shader compiles with the
  templated header.
- **What stays outside a shared primitive.** The refraction *vector* (`rr·d + (rr·cos − √disc)·n`)
  has no single-source helper in this tree today — the host `HitSurface`, the Metal kernel and the
  CUDA kernel each inline it. The evaluator writes it in LI's `refract_smooth` form, with LI's
  margin expressions, so its validity boundaries are LI's. It is cross-checked against the
  simulator by a unit test that runs the float `HitSurface` chain on the same crystal and path
  (agreement to float precision), and against LI by the parity fixtures.
- **The continuation keeps the margin** (2026-09-29). The corrector's root gate is the
  `1e-11` residual tolerance; the evaluator's measured deviation from LI's is at most `0.054` of a
  `1e-12` kinematic tolerance, about `5e-14` — 185 times smaller. Over LI's eight `trace_fiber`
  fixtures the largest accepted residual is `4.6e-12` and no trace ended in a corrector failure, so
  nothing in the continuation asks for more precision than this section's ruling provides.
- **Face presence stays float** (§4.1): the engine's float closed form decides which faces exist,
  as for a simulated crystal. Only on a knife-edge input where a face appears or vanishes between
  float and double geometry could that differ from LI, which decides it in double.

---

## 6. Logging **(owner direction, design detail; mechanism as built)**

The library writes nothing by default; the host receives diagnostics through
`LUMICE_ANALYTIC_SetLogCallback`. The mechanism is built with the log-sink work (the other
binaries' behaviour does not change).

**Does the first module have warning paths a silent default would swallow? Yes.** Checked in the
code the module will call, all through the `LOG_WARNING` macro family (`src/util/logger.hpp:137`,
routed to the global logger, which forwards to the process-wide shared sink):

| Where | When |
|---|---|
| `src/core/crystal.cpp:361`, `:412` | a prism or pyramid fails the closed-form validity gate and is treated as degenerate (empty crystal) |
| `src/core/geo3d_closedform.cpp:1054` | a pyramid cone has no cross-section at the requested inset; it is degraded to its apex point |
| `src/core/geo3d_closedform.cpp:1277` | pyramid face slots keep fewer than 3 vertices and are dropped, leaving the surface open |
| `src/core/geo3d.cpp:417`, `:510` | the legacy coefficient path finds a zero-volume crystal or an empty pyramid region |

Today these reach the default `stderr` console sink that every engine binary inherits
(`GetDefaultConsoleSink` attached by `GetSharedSink`, `src/util/logger.hpp:40-66`), at the global logger's default level, which lets
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
graph planned today. **Re-checked for `EvaluatePath` (2026-09-29)**, which does go through the
crystal factory (`Crystal::CreatePrism` / `CreatePyramid`, §4.1): the factory's own warnings are
the two `crystal.cpp` rows above, and `DeriveGeometricSymmetry` / `PopulateFromCfGeom` log nothing;
the `geo3d.cpp` rows are the legacy path, which the factory no longer takes. Not a log line and not
silenceable: the closed-form evaluator's overflow guards (`FatalAbort` on a vertex-pool or
face-polygon overflow, `geo3d_closedform.cpp`) print to stderr and abort the process, as they do in
every engine binary. They guard bounds a 2M-sample fuzz stays well inside; reaching one is an
engine bug, not an input the library could report.

**As built.** `src/util/logger.hpp` names the console sink (`GetDefaultConsoleSink()`), which
`GetSharedSink()` still attaches by default — so `liblumice`, `liblumice_testapi`, the CLI and the
GUI are unchanged. `src/analytic/analytic_api.cpp` holds a namespace-scope object whose
constructor removes that sink from *this library's* copy of `GetSharedSink()` during the library's
dynamic initialisation, i.e. before `dlopen` / `LoadLibrary` returns and before any
`LUMICE_ANALYTIC_*` call can be made; the other libraries' copies are separate statics (§2.6) and
never see the removal. `LUMICE_ANALYTIC_SetLogCallback` attaches
`lumice::analytic::AnalyticCallbackSink` (`src/analytic/analytic_callback_sink.hpp`) on its first
call and logs one INFO line, "log callback installed", through the engine's global logger; that
line is what makes the silence testable at the library boundary — it must reach the callback and
not stderr (`test/e2e-correctness/test_analytic_log_sink.py`). The warning-level path (a
`LOG_WARNING` silent without a callback, delivered at `LUMICE_ANALYTIC_LOG_WARNING` with its text
intact) is pinned in-process by
`test/unit-correctness/util/test_logger_default_console_sink_removable.cpp`, and end to end by
`test/e2e-correctness/test_analytic_evaluate_path.py`: a crystal the closed-form gate rejects,
passed to `EvaluatePath`, returns `ERR_INVALID_CONFIG`, its gate warning arrives at the callback
at `LUMICE_ANALYTIC_LOG_WARNING`, and the child's stderr stays empty.

There is **one** receiver: a second `SetLogCallback` replaces the first, and there is no per-level
filter or fan-out. That is sufficient for a host that owns the process; several independent
listeners in one process would need a new call, not an extra parameter.

The callback sink is a second copy of `lumice::CCallbackSink` (`src/util/callback_sink.hpp`) typed
on this header's enum, for the same reason as the error codes (§5.2, §7): the two headers share no
type. That makes two small types kept once per header — error codes and the log callback. If a
third such pair appears, re-weigh a type-agnostic template shared by both over another copy.

---

## 7. Relation to `lumice.h`

- `lumice_analytic.h` does **not** include `lumice.h`, and `lumice.h` does not include it. The two
  headers share no type. A consumer of one never compiles against the other.
- `lumice.h` is not published by this work. It moves to explicit exports too **(owner)**, but
  stays an internal interface of Lumice's own binaries.
- **The export set of `liblumice_analytic` is exactly the `LUMICE_ANALYTIC_*` functions.** This
  does not happen by itself: `lumice.h` marks every declaration `LUMICE_API` (default visibility
  on GCC/Clang), and `c_api.cpp` — part of `lumice_obj` — includes it, so every `LUMICE_*`
  function is compiled with default visibility into the objects. A shared library linked from
  those objects with `-fvisibility=hidden` alone would export the whole `lumice.h` surface next to
  the analytic one. The guarantee comes from the **per-library export list** (§2.5: a version
  script / symbol list / `.def` naming only `LUMICE_ANALYTIC_*`), and its test is mechanical: list
  the dynamic symbol table of the built library and assert that every exported name matches
  `\bLUMICE_ANALYTIC_` and that at least one does (`test_export_symbol_scope.py`).
- Both libraries run the same objects. A behaviour change in shared engine code reaches both; that
  is intended (one implementation per semantics), and it is why §5.1 treats a convention change as
  an interface change.

---

## 8. Version, compatibility and packaging **(design; as built with the packaging work)**

Code comments across the tree cite "doc/analytic-api.md section 8" for all of this; the
subsections keep that number.

### 8.1 What the version is

- **An independent counter**, `LUMICE_ANALYTIC_API_VERSION`, not tied to `LUMICE_API_VERSION`.
  The two surfaces change at rates an order of magnitude apart (`lumice.h` is at 449 after the
  classified churn in `doc/api-layering-and-product-lines.md` §8), and a consumer of one must not
  see the other's churn as a version bump.
- **A single integer, and the only version.** It is bumped on every incompatible change (§8.2),
  with a one-line note at the top of the header saying what changed — the same style as the
  `BREAKING` / `ADDED` / `BEHAVIOR` notes above `LUMICE_API_VERSION` in `lumice.h`. There is no
  second, three-part semver number beside it: the CMake package version is this integer (§8.4),
  read from the header by the build rather than written a second time, so the version a build
  accepts and the version `LUMICE_ANALYTIC_GetApiVersion()` reports cannot drift apart.
- **The run-time check.** `LUMICE_ANALYTIC_GetApiVersion()` returns the library's value; a binding
  compares it with the header's macro (or, for ctypes, with the value it was written against) to
  detect a header/library mismatch that no linker catches.

### 8.2 Compatible and incompatible changes

An incompatible change bumps the integer. In 0.x a compatible one bumps it too — every addition so
far has (versions 2, 3 and 4 each added functions and nothing else incompatible), and that is the
rule: `find_package` accepts only the exact version (§8.4) and a ctypes binding pins the version it
was written against, so the integer is the only way a consumer can tell which functions a library
has. From 1.0, when compatibility is promised, a compatible change leaves the integer alone and the
table below becomes the rule.

| Change | Kind |
|---|---|
| A new function | Compatible |
| A new value in an open set (`LUMICE_ANALYTIC_Reason`, §4.4) | Compatible — callers must already handle an unknown reason |
| A field appended at the end of `PathEvaluation`, `FiberResult` or `DiscoveryResult`, under the `struct_size` rule below | Compatible |
| A field added to an element of a library-allocated array (`DiscoveredComponent`, `IncompleteCandidate`) | Incompatible — the caller indexes the array with its own `sizeof` |
| Growth of a library-allocated buffer reached through `storage` (`segment_directions`, `poses`, …) | Compatible — the caller never lays memory out for it |
| A changed signature, a removed function, a renamed or reordered field | Incompatible |
| Any field added to a caller-owned input struct (`Crystal`, `FiberProblem`, `ContinuationOptions`, `DiscoveryProblem`, `DiscoveryOptions`) | Incompatible — the library would read past what an older caller allocated |
| Any change to a closed set (`LUMICE_ANALYTIC_FiberStatus`, `LUMICE_ANALYTIC_ErrorCode`, `LUMICE_ANALYTIC_Completeness`, `LUMICE_ANALYTIC_ComponentKind`, `LUMICE_ANALYTIC_IncompleteCause`) | Incompatible |
| Any change to a convention the functions pass (frames, face numbers, pose chain — §5.1) | Incompatible, even with an unchanged signature |
| A result field added other than by the `struct_size` rule | Incompatible |

**The `struct_size` rule** (answers §9 item 12). The two result structs are caller-allocated —
passed by pointer, or as a caller-allocated array in `TraceFiberBatch` (§4.4) — so a library built
against a header with more fields would, without a guard, write past the memory an older caller
sized, and in the batch case into the next element. Each therefore begins with
`uint32_t struct_size`, the Win32 `cbSize` pattern:

- The caller zero-initialises the struct and sets `struct_size = sizeof(the struct)` as compiled
  against its header, before the call.
- The library writes only fields that lie wholly inside `struct_size` bytes; a field beyond it is
  one the caller does not know, and stays untouched. A caller newer than the library sees its
  extra fields left at zero, so every appended field must give zero the meaning "not provided".
- `struct_size` smaller than the first published layout is a call-level `ERR_INVALID_VALUE`.
- Zero-filling on error (§4.4) zeroes everything after `struct_size`, never `struct_size` itself.
- In `TraceFiberBatch` the array stride is `out_results[0].struct_size`, not the library's own
  `sizeof`; every element must carry the same value (a mismatch is a call-level error). The
  library cannot index an array of structs smaller or larger than its own any other way.
- New fields go at the end only. Input structs get no `struct_size`: they are small, fixed, and a
  change to them is rare enough that a version bump is the honest price (the table above).

### 8.3 What 0.x promises

**0.x is experimental** until the conditions in §8.6 hold. In 0.x, **no two values of
`LUMICE_ANALYTIC_API_VERSION` are promised compatible**, including across a change the table in
§8.2 calls compatible — the table is the rule the library tries to keep, not a guarantee a consumer
may build on yet. Two things stay promised even in 0.x: every incompatible change bumps the
integer, and every bump carries its one-line note saying what changed. "Nothing is promised" is
about compatibility, never about disclosure.

### 8.4 The CMake package, and how a mismatch is refused

A shared, non-CUDA configure (§2.3) installs, relative to `CMAKE_INSTALL_PREFIX`:

```
include/lumice_analytic.h                      the one header; lumice.h is never installed with it
lib/liblumice_analytic.so | .dylib             Linux / macOS
bin/lumice_analytic.dll + lib/lumice_analytic.lib   Windows: DLL + import library
lib/cmake/LumiceAnalytic/LumiceAnalyticConfig.cmake
lib/cmake/LumiceAnalytic/LumiceAnalyticConfigVersion.cmake
lib/cmake/LumiceAnalytic/LumiceAnalyticTargets*.cmake
```

All of these rules, and nothing else, are the install component `analytic`:
`cmake --install <build> --component analytic --prefix <prefix>` produces exactly this tree, even
from a build that compiled only some targets (a plain `cmake --install` installs it together with
everything else the configure installs). CI runs that command on Linux, macOS and Windows (cl.exe
and clang-cl) and checks the files are there (`e2e-slow`, `windows-shared-export`).

A consumer's CMake:

```cmake
find_package(LumiceAnalytic 1 REQUIRED)   # 1 = the LUMICE_ANALYTIC_API_VERSION written against
add_executable(app main.c)
target_link_libraries(app PRIVATE Lumice::lumice_analytic)
```

with `-DCMAKE_PREFIX_PATH=<prefix>` pointing at the install tree. The imported target carries the
include directory and, on Windows, the `LUMICE_ANALYTIC_SHARED_DEFINE` that turns the header's
macro into `dllimport` (§2.5); nothing else is exported — the engine is linked PRIVATE and does not
reach the consumer. The package version is the integer of §8.1 with **`ExactVersion`**
compatibility: any requested version other than the installed one is refused **at configure
time**, before anything is compiled (`Could not find a configuration file for package
"LumiceAnalytic" that is compatible with requested version …`). That is the mechanical form of
§8.3. Omitting the version accepts whatever is installed; a consumer who does that should check
`LUMICE_ANALYTIC_GetApiVersion()` at run time instead. The config uses `@PACKAGE_INIT@`, so an
install tree still works after being moved.

On Windows the DLL has to be findable when the consumer runs: copy it next to the executable, or
put `<prefix>/bin` on `PATH`. For example:

```cmake
add_custom_command(TARGET app POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:Lumice::lumice_analytic> $<TARGET_FILE_DIR:app>)
```

Two omissions are deliberate. **No SONAME / SOVERSION symlinks**: in 0.x a consumer builds and
installs against one version, and no machine is expected to hold two side by side for the
loader to choose between; `ExactVersion` plus the run-time check covers the mismatch case. And
**`bin/` + `lib/`, unlike `lumice`**, which puts its Windows engine DLL at the prefix root: that
layout serves the release shell loading its engine from its own directory, while this library is
consumed by other projects' builds, for which `bin/` + `lib/` is the convention.

### 8.5 Deprecation

- **In 0.x there is no deprecation stage.** A function or field to be removed is removed in the
  next bump, and the bump's note says so. Keeping a deprecated name alive for a period would be a
  compatibility promise 0.x does not make (§8.3).
- **From 1.0**: a declaration to be removed is first marked with a `LUMICE_ANALYTIC_DEPRECATED`
  macro (`__attribute__((deprecated))` / `__declspec(deprecated)`, written when first needed),
  stays for at least one minor version, and is removed in the next major one.

### 8.6 What 1.0 requires

All three, together:

1. The first real module (§4) has shipped in at least one download-package release.
2. LI has consumed it for at least one release cycle without the interface needing an incompatible
   change.
3. The owner rules the interface stable.

1.0 is where the single integer gains semver structure (major for incompatible changes, minor for
compatible additions), the package switches from `ExactVersion` to `SameMajorVersion`, and the
§8.2 table becomes a promise rather than a rule.

### 8.7 Locating the library without CMake (the install-tree contract)

A consumer that does not use CMake — LI's Python bindings through ctypes or pybind — finds the
library by path. The contract is the layout of §8.4 **relative to the install prefix**: the header
at `include/lumice_analytic.h`; the library at `lib/liblumice_analytic.so` (Linux),
`lib/liblumice_analytic.dylib` (macOS), or `bin/lumice_analytic.dll` with its import library at
`lib/lumice_analytic.lib` (Windows). The file names differ by platform; the directory each sits in,
relative to the prefix, does not, and changing it is an incompatible change in the sense of §8.2.

How a consumer learns the prefix is the consumer's decision. The suggested name, so the
external-consumer smoke test and LI's bindings agree, is an environment variable
**`LUMICE_ANALYTIC_INSTALL_DIR`** holding the prefix. It is read by the consumer's own code only:
Lumice's `src/` never reads it, it is not in `src/util/env_knobs.cpp`, and
`doc/env-var-policy.md` — which governs what Lumice itself reads — does not cover it.

Both kinds of consumer exist in this repository as a smoke test that sees only an install tree:
`test/e2e-correctness/external_consumer_smoke/` holds a standalone CMake project
(`find_package(LumiceAnalytic)`, links `Lumice::lumice_analytic`, compares
`LUMICE_ANALYTIC_GetApiVersion()` with the header's macro) and `smoke.py`, a dependency-free ctypes
loader that finds the library from `LUMICE_ANALYTIC_INSTALL_DIR` by the layout above — the minimal
sample for a Python binding. `test/e2e-correctness/test_external_consumer_smoke.py` drives both
against the prefix that variable names, or, when it is unset, against a fresh
`cmake --install --component analytic` of the local shared build. It also asserts that every
include directory of the C project resolves outside the source tree, and that the installed
`include/` is one of them, so "install tree only" is checked rather than assumed. CI runs it in
`e2e-slow` (Linux and macOS "rest" legs) and in both `windows-shared-export` legs, each time
against the prefix that job's packaging step has just installed.

### 8.8 Checklist for putting the library in a download package

Owner, 2026-09-28: the library does not enter a download package while it holds only placeholder
functions. The first real module's work opens this list. **The state described here is that of
`release.yml` on 2026-09-28; re-read it before acting.**

- `release.yml`'s top-level comment says "no library is published for anyone to link"; rewrite it.
- Which configures produce the library today: `linux-x64` is `isa_split`, and its baseline tree
  (`build-baseline`: shared, non-CUDA, `-DLUMICE_ISA_LEVEL=baseline`) already builds and installs
  it into `install-baseline/` — exactly the configure §2.4 asks for; the merge step copies only
  named files, so it does not reach the package, and shipping it there means adding those files
  (§8.4's list) to the copy and to the manifest. `linux-arm64` and `macos-arm64` build the default
  static flavour, where the target does not exist: they need an extra shared configure or a
  separate analytic-only job. `windows-x64` sets `cuda: ON` on both of its configures, so **no
  Windows configure in the release workflow is non-CUDA** — the Windows library needs a new
  non-CUDA configure (§2.3), not a reuse of the `isa_split` trees.
- Artifact shape: a separate `lumice-analytic-<version>-<platform>.{tar.gz,zip}` rather than files
  inside the CLI/GUI package is recommended — the people downloading the application do not need a
  development library, and the reverse — but it is that work's call.
- `CHANGELOG.md`'s Sourcing rule gains a fourth mechanical command:
  `git diff <prev_tag>..<tag> -- src/include/lumice_analytic.h | grep LUMICE_ANALYTIC_API_VERSION`.
- `CONTRIBUTING.md`'s list of platform packages ("The release produces platform-specific packages") gains the new artifact.
- The consumer-facing notes shipped with it include §2.6 (one engine per process) verbatim, and
  §8.4's Windows DLL note.

---

## 9. Open items, each with its owner

| # | Item | Decided by / when |
|---|---|---|
| 1 | ~~Seed search (discovery) in scope for v0?~~ **Answered** — yes, v0 includes seed search (author, 2026-09-28; §4.2). Spec: LI `docs/phase1-math-contract.md` §9.5. No MC ray-pose recording is required: Analyze's all-sky map is low resolution, seed density goes low first and is refined progressively. **As built** (API version 4, §4.3): `DiscoverComponents`, deterministic lattice sample, warm seeds as the densification convention because densification is not monotone. | Author and owner, 2026-09-28 |
| 2 | ~~Reference defaults of `ContinuationOptions`, each linked to convergence evidence (LI §10.1).~~ **Answered** — every default is LI §10.1's, none deviates; the mapping and evidence are in §4.3 (`TraceFiber` as built). | The first-module implementation (`TraceFiber`, 2026-09-29) |
| 3 | A refractive-index convenience function (Sellmeier). Default: not exposed (§4.2). | The first-module implementation, on LI's actual need |
| 4 | ~~Semver, ABI and deprecation policy text; what 1.0 commits to.~~ **Answered** — see §8 (as built): one integer is the only version, `find_package` requires it exactly (§8.4), compatible/incompatible table (§8.2), no promise in 0.x (§8.3), deprecation (§8.5), graduation conditions (§8.6). | The packaging and version-policy work (done) |
| 5 | ~~Export-list mechanism on each platform, Windows export path, header location, prefix gate in `check_policies.py`, the stripping flag.~~ **Answered** — see §2.5 (as built): `scripts/gen_export_list.py` + `lumice_apply_export_list`, `.def` on Windows, `src/include/lumice_analytic.h`, rule `analytic-symbol-scope`. | The target and export-list work (done) |
| 6 | ~~Callback forwarding implementation; removing the console sink only in this library.~~ **Answered** — see §6 (as built): `GetDefaultConsoleSink()` removed at load time in `src/analytic/analytic_api.cpp`, `AnalyticCallbackSink` attached by `LUMICE_ANALYTIC_SetLogCallback`. | The log-sink work (done) |
| 7 | ~~External consumer smoke test (C + Python ctypes, install tree only), with `symmetry_semantics` in any fixture.~~ **Answered** — see §8.7 (as built): `test/e2e-correctness/test_external_consumer_smoke.py` + `external_consumer_smoke/`, consuming the prefix named by `LUMICE_ANALYTIC_INSTALL_DIR`; it compares no face sequence and says so, `symmetry_semantics: none`, in its docstrings (§3.3 rule 2). | The external-consumer smoke test (done) |
| 8 | ~~Does `FiberResult` need LI §9.3's diagnostics and the entry cross-section `A_P`?~~ **Answered** — two steps (author, 2026-09-28): v0 returns the point list only; diagnostics + weights enter in wave 2 as a `struct_size`-compatible extension, once LI's explore `fiber-diagnostics-contract` has converged (§4.3, §10). | Author and owner, 2026-09-28 |
| 9 | ~~Surface crystal *degradation* (apex collapse, dropped face) as result data, not only as a log line (§6).~~ **Answered for v0** — not surfaced: a dropped face shows only as that face number being rejected, an apex collapse only in the log. A result field for it goes with the wave-2 diagnostics extension (§4.1). | The first-module implementation (`EvaluatePath`, 2026-09-29) |
| 10 | Parallelism inside `TraceFiberBatch` (v0: none; caller parallelises). Revisit only with a measured batch where binding-side threading is the bottleneck. **As built**: none; the batch shares one crystal build and starts no threads, and concurrent calls are safe (§5.3). No measurement has asked for more. | The first-module implementation (`TraceFiber`, 2026-09-29); reopen on a measured bottleneck |
| 11 | ~~Re-read LI `docs/phase1-math-contract.md` §9 before implementing: this draft mirrors it as of 2026-09-28, and LI's §12 lists open items that may move it.~~ **Answered** — re-read at LI `bfbd042`: §9 unchanged in shape; §6.4 / §10.1 had moved (the step-aware closure trigger), and that is what is built (§4.3). LI's §9.1 also says problem construction "MUST not import or invoke Lumice" — a rule LI revises on its side when it adopts this library. | The first-module implementation (`TraceFiber`, 2026-09-29); LI, on adoption |
| 12 | ~~Whether `PathEvaluation`/`FiberResult` should carry a `struct_size`/version field (Win32 `cbSize`, Vulkan `sType`+`pNext` are existing patterns) so a future field addition would not need an `LUMICE_ANALYTIC_API_VERSION` bump (§8).~~ **Answered** — yes: a leading `uint32_t struct_size`, the Win32 `cbSize` pattern (§4.5 draft, rules in §8.2). | The packaging and version-policy work (done) |
| 13 | ~~Verify no thread-unsafe static cache in the called geometry/optics code (§5.3).~~ **Answered** — none on `EvaluatePath`'s call graph; pinned by a concurrency test (§5.3). | The first-module implementation (`EvaluatePath`, 2026-09-29) |
| 14 | An optional batch-mode `FiberResult` variant that also returns per-point segment directions and interface transmittances (today only `EvaluatePath` returns those, §4.3), for a caller with many accepted poses who would otherwise pay one ctypes call per point to get them — in tension with §4.3's own binding-overhead concern. **Not built in v0**: `FiberResult` returns the point list only (§4.3); a caller that needs per-point segments calls `EvaluatePath` per pose. Still open. | Wave 2, with the diagnostics extension |

---

## 10. Rollout in waves **(author and owner, 2026-09-28)**

The library is filled one module per wave. Lumice implements a module first; LI switches its
dependency one wave later.

| Wave | Shared library (this repo) | Analyze function it serves (`doc/raypath-analysis.md` §5.1) | LI side |
|---|---|---|---|
| **1** | Module A v0: `EvaluatePath` + **seed search** + `TraceFiber[Batch]`, point list only | Function 1: fiber detail | Writes the discovery contract, exports parity fixtures, researches the diagnostics/weights contract; does not switch |
| 2 | Module A v1 (diagnostics + weights, `struct_size`-compatible extension); module B (single-path S² binning + banded sum) | Function 2: single-path all-sky map | Certifies A v1, then switches fiber and retires the JAX continuation |
| 3 | Module C (`dp_field` / `contour` / `focusing`, `Jet2` forward hyper-dual) | Function 3: preset points and mechanism labels | After ch12/12.1 are done with it: switch B, then C |

**Parity fixtures flow LI → Lumice.** LI exports them at a pinned revision; this repo copies them in
and runs them in CI. A change goes one way: LI changes first → re-export → this repo's parity goes
red → fix the C++.

### 10.1 Parity with LI (as built)

**Where it lives.** `test/fixtures/li-parity/` holds LI's export verbatim: one JSON per fixture, LI's
`manifest.json`, and a `SOURCE` file naming the full LI commit, the export command, the date, the
`--verify` result and whether a second export was byte-identical. The format, fields, recipes and
tolerance basis are LI `docs/analytic-parity-fixtures.md`; this section does not restate them.
`test/unit-correctness/analytic/test_li_parity.cpp` replays every fixture, one gtest case each
(`LiParityEvaluatePath` / `LiParityTraceFiber` / `LiParitySeedSearch`), inside
`unit_correctness_test`, so it runs wherever that target runs — including every leg of CI's `build`
job, whose `ctest -L` selector includes `unit-correctness`. Placement follows
`doc/testing-architecture.md` §3: the oracle is another implementation but not the legacy CPU
backend, and it is not a closed form, so it is `unit-correctness`, not `parity-cross-backend` or
`golden-analytic`.

**What it proves and what it does not.** The cases call the kernels (`EvaluatePath`, `TraceFiber`,
`IceDiscovery::DiscoverOnBand`), not the C ABI, because a `seed_search` fixture is a replay on LI's
exported band and only the kernel accepts a band. So parity covers the kernels' semantics; the
ABI's translation of `LUMICE_ANALYTIC_Crystal` and the v0 options block into them stays the
subject of the ctypes tests (`test/e2e-correctness/test_analytic_*.py`). LI's continuation options
have four fields the kernel has no knob for (`rotation_tolerance`, `dtype`, `sample_retention`,
`diagnostic_level`); the reader asserts each holds the one value the kernel implements and fails on
any field it does not know, so an option LI adds cannot be dropped silently. The comparison geometry
(rotation distance, SO(3) log/exp, geodesic densification, Hausdorff) is written out in the test
rather than taken from `src/analytic/so3.hpp`, so a defect there cannot also blind the ruler.

**Tolerances are the fixture's.** Every bound is read from the fixture's `tolerance.<quantity>.value`.
The file contains two recipe constants and no tolerance: the 1e-3 rad densification spacing (part
of LI's curve-distance definition) and the 1e-12 absolute term LI's own verifier adds to a relative
arclength comparison so a zero-length reference is comparable. Each case prints the measured error
next to its bound, red or green, which is the running record of how much room the bounds leave.
First run (LI `bfbd042`): all 35 fixtures green with no change to the C++; the largest margins used
were 2.3e-3 of 0.012 rad (curve distance, near a critical point), 4.9e-4 of 2e-3 (arclength) and
4.6e-12 of 1e-11 (residual); everything else sat at rounding level.

**Two fixtures that look like gaps and are not.** `3-5-6-7__critical` has no files: the manifest
records it as skipped with a reason (that path's `D_P` has no interior extremum on this crystal),
and the manifest check accepts exactly that — a reason, and no listed file. The
`13-15-26-28__near_boundary__seed_search` fixture expects a complete search with an empty band and no
component: it is a negative case, and a search that invents a component there is a failure.

**Updating.**

1. In LI, on a clean checkout of the rev to adopt (a `git worktree` of `origin/main` keeps the
   working tree out of it; `li_tracked_tree_clean` must be `true`):
   `uv run python scripts/export_analytic_parity.py --output-dir <dir> --verify`. `--verify` must
   pass. Export twice and compare with `diff -r`; the export is deterministic, so a difference is
   itself something to report to LI.
2. Replace the whole of `test/fixtures/li-parity/` with the output and rewrite `SOURCE` for the new
   rev. Unchanged fixtures are byte-identical (sorted keys, shortest round-trip floats), so the diff
   shows only what LI changed.
3. Run the cases. A red is then read with three questions:
   - Did LI change? Compare the red fixture with its previous version in git. A changed `expected`
     or `tolerance` at a new `li_rev` means LI moved, and the C++ follows.
   - Did this repo change? If the fixture is unchanged since the last green run, a red is a
     regression here.
   - Does LI still verify it? If `--verify` passes at that rev and the C++ disagrees, the C++ is
     wrong. If LI's own read-back fails, the problem is LI's; report it there.
4. Who changes first: LI. The C++ is fixed until green. Neither a fixture nor a bound is edited in
   this repo, and no case is disabled to get green; a disagreement about a tolerance or a semantic
   goes back to LI, is fixed there with its evidence, and comes back as a re-export.

A `schema_version` other than 1 fails the provenance and manifest cases: read LI's updated page,
change the reader, then re-export. A new cell needs no change here — cases are generated from
`manifest.json`, and the manifest check fails if a file is present but not listed or listed but not
present. A new fixture *kind* needs a reader first: the manifest check goes red on a listed file that
no replay suite covers, so it cannot pass silently. The reader's SO(3) helpers are a deliberate second
implementation, independent of `so3.hpp` so the ruler does not share the code under test; nothing
checks the two against each other automatically, and that fork is accepted.
