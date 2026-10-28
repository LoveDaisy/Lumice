#!/usr/bin/env python3
"""Face derivation + checker driving behind `test.sh inner` (the iteration-time fast path).

Three subcommands, one per consumer:

  faces     git diff -> files -> layers -> faces -> test layers. The human-readable
            derivation chain goes to stderr; stdout carries a single JSON plan object
            (the machine-readable contract scripts/test.sh consumes — the two streams
            are separated so the chain's wording can evolve without breaking parsing).
  checkers  runs the four diff-scoped policy gates through each module's
            diff_verdict() seam — the same functions their own main() calls — and
            reports one aggregate verdict. Untracked files are counted and their
            invisibility to these gates stated (same scope as the pre-commit hook).
  gtest     runs one selected gtest target through the shared native-sharding
            entry, behind per-suite reconciliation against --gtest_list_tests
            and a nonempty passed-count guard (gtest exits 0 on an empty filter).

inner is the iteration-time fast path only; quick remains the closing gate. The
faces are deliberately coarse — a face can miss, and whatever it misses is caught
by quick before anything merges. Nothing here writes repository state.

Diff scope, everywhere: merge-base(HEAD, main)..worktree (or --base, or --files).
The worktree — not the index — is the content source, because an iteration change
is usually unstaged and the gates must see what would actually be committed.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_policies import strip_comments  # noqa: E402

REPO = Path(__file__).resolve().parent.parent

BUILD_TREE = "build/cmake_build/static"
LAYER_MAP_RELPATH = "lumice_layer_map.tsv"

# Faces whose test layers need compiled binaries; faces outside this set (docs,
# scripts) skip the build-tree preconditions entirely.
COMPILED_LAYERS = {
    "ctest",
    "ctest-unit-correctness",
    "unit-gtest",
    "composition-gtest",
    "parity",
    "composition-gui",
    "golden-analytic",
    "gui-correctness",
    "fast-e2e",
}

# path rule -> test layers, evaluated after the src/ and doc/ rules. First match
# wins; the details are as coarse as v1 faces get, and whatever a rule misses is
# quick's job to catch.
TEST_ROOT_RULES: list[tuple[str, str, list[str]]] = [
    # (rule label, path prefix, layers).gui_unit_test and composition_correctness_test
    # draw their TUs from here too — these sources live in several binaries, so the
    # label selection (which covers all of them) is the only rule that picks no
    # wrong target.
    ("test/gui/**", "test/gui/", ["ctest", "gui-correctness"]),
    ("test/unit-correctness/gui/**", "test/unit-correctness/gui/", ["ctest"]),
    ("test/unit-correctness/scripts/**", "test/unit-correctness/scripts/", ["script-unit-pytest"]),
    ("test/composition-correctness/gui/**", "test/composition-correctness/gui/", ["composition-gui"]),
    (
        "test/composition-correctness/raypath/**",
        "test/composition-correctness/raypath/",
        ["composition-gtest"],
    ),
    ("test/parity-cross-backend/**", "test/parity-cross-backend/", ["parity"]),
    ("test/golden-analytic/**", "test/golden-analytic/", ["golden-analytic"]),
    ("test/e2e-correctness/**", "test/e2e-correctness/", ["fast-e2e"]),
    ("test/regression-sentinel/**", "test/regression-sentinel/", ["fast-e2e"]),
    ("test/e2e/**", "test/e2e/", ["fast-e2e"]),
    ("test/fixtures/**", "test/fixtures/", ["ctest", "fast-e2e"]),
    ("test/support/**", "test/support/", ["ctest", "fast-e2e"]),
    ("test/performance/**", "test/performance/", []),
]

FACE_LAYERS: dict[str, list[str]] = {
    "gui": ["ctest", "gui-correctness"],
    "raypath": ["unit-gtest", "composition-gtest"],
    "core": ["ctest"],
    "shell": ["fast-e2e", "ctest-unit-correctness"],
    "build-system": ["ctest"],
    "scripts": [],
    "docs": [],
    "test-unit-tu": ["unit-gtest"],
    "test-misc": ["ctest", "fast-e2e"],
}

# Everything under test/unit-correctness/ that compiles into unit_correctness_test
# and is not matched by TEST_ROOT_RULES above lands here, and its changed TUs'
# suites form the direct gtest filter.
UNIT_TU_PREFIX = "test/unit-correctness/"

KERNEL_SUFFIXES = {".cu", ".metal"}
KERNEL_PREFIXES = ("src/core/shared/",)

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".hh", ".hxx", ".h"}


class DerivationError(Exception):
    """A diff that cannot be classified — inner refuses to guess, exits 2."""


def git_out(args: list[str]) -> str:
    proc = subprocess.run(
        ["git", "-c", "core.quotePath=false", *args],
        cwd=REPO,
        capture_output=True,
        check=True,
    )
    return proc.stdout.decode("utf-8", errors="replace")


def default_base() -> str:
    return git_out(["merge-base", "HEAD", "main"]).strip()


# --- gtest suite parsing ----------------------------------------------------------


TEST_P_RE = re.compile(r"\bTEST_P\s*\(\s*([A-Za-z_]\w*)\s*,")
PLAIN_TEST_RE = re.compile(r"\b(?:TEST|TEST_F)\s*\(\s*([A-Za-z_]\w*)\s*,")
INSTANTIATE_RE = re.compile(
    r"\bINSTANTIATE_TEST_SUITE_P\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*,"
)


def parse_suites(tu_text: str) -> tuple[set[str], dict[str, set[str]]]:
    """(plain suites, parameterized suite -> instantiation prefixes in the same file).

    A TEST_P suite's registered names are `Prefix/Suite.test/N` — a bare `Suite.*`
    arm selects nothing, which is why every parameterized suite also emits
    `*/Suite.*`, and why the prefixes are collected at all: the reconciliation
    step matches selected suite headers in the `Prefix/Suite.` form.
    """
    code = strip_comments(tu_text)
    plain = {m.group(1) for m in PLAIN_TEST_RE.finditer(code)}
    param: dict[str, set[str]] = {}
    for m in TEST_P_RE.finditer(code):
        param.setdefault(m.group(1), set())
    for m in INSTANTIATE_RE.finditer(code):
        suite, prefixes = m.group(2), param.get(m.group(2))
        if prefixes is not None:
            prefixes.add(m.group(1))
    return plain, param


def filter_arms(plain: set[str], param: dict[str, set[str]]) -> list[str]:
    arms: list[str] = []
    for suite in sorted(plain):
        arms.append(f"{suite}.*")
    for suite in sorted(param):
        arms.append(f"{suite}.*")
        arms.append(f"*/{suite}.*")
    seen: set[str] = set()
    return [a for a in arms if not (a in seen or seen.add(a))]


def suites_of(
    tu_relpaths: list[str],
) -> tuple[list[str], list[str], set[str], dict[str, set[str]]]:
    """(filter arms, TUs that yielded no suite, plain suites, parameterized suites).

    The parsed suites ride along because the gtest subcommand's reconciliation
    needs them, and it must not re-derive them — one derivation, in faces.
    """
    plain: set[str] = set()
    param: dict[str, set[str]] = {}
    empty: list[str] = []
    for rel in tu_relpaths:
        path = REPO / rel
        try:
            text = path.read_bytes().decode("utf-8", errors="replace")
        except OSError:
            empty.append(rel)
            continue
        tu_plain, tu_param = parse_suites(text)
        if not tu_plain and not tu_param:
            empty.append(rel)
        plain |= tu_plain
        for suite, prefixes in tu_param.items():
            param.setdefault(suite, set()).update(prefixes)
    return filter_arms(plain, param), empty, plain, param


def selected_suite_headers(binary: Path, filter_str: str) -> set[str]:
    proc = subprocess.run(
        [str(binary), "--gtest_list_tests", f"--gtest_filter={filter_str}"],
        cwd=REPO / BUILD_TREE / "test",
        capture_output=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"--gtest_list_tests failed (exit {proc.returncode}): "
            f"{proc.stderr.decode('utf-8', errors='replace').strip()}"
        )
    headers = set()
    for line in proc.stdout.decode("utf-8", errors="replace").splitlines():
        if line and not line[:1].isspace() and line.rstrip().endswith("."):
            headers.add(line.strip())
    return headers


def suite_selected(headers: set[str], suite: str) -> bool:
    tail = f"{suite}."
    typed = re.compile(re.escape(suite) + r"/\d+\.$")
    return any(h == tail or h.endswith("/" + tail) or typed.fullmatch(h) for h in headers)


def reconcile(
    binary: Path,
    filter_str: str,
    plain: set[str],
    param: dict[str, set[str]],
) -> list[str]:
    """Suites from the parse that contributed zero selected cases — a partial
    miss the N=0 guard is structurally blind to."""
    headers = selected_suite_headers(binary, filter_str)
    missing = []
    for suite in sorted(plain):
        if not suite_selected(headers, suite):
            missing.append(suite)
    for suite in sorted(param):
        if not suite_selected(headers, suite):
            missing.append(f"{suite} (parameterized)")
    return missing


# --- classification ----------------------------------------------------------------


def load_layer_map() -> dict[str, str]:
    path = REPO / BUILD_TREE / LAYER_MAP_RELPATH
    if not path.exists():
        raise DerivationError(
            f"{path} not found — the layer map is a configure-time artifact; run "
            f"./scripts/build.sh -gtj release first, then inner"
        )
    mapping: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        rel, sep, layer = line.partition("\t")
        if not sep or not layer:
            raise DerivationError(f"malformed row in {path}: {line!r}")
        mapping[rel] = layer
    return mapping


class Faces:
    def __init__(self) -> None:
        self.map: dict[str, str] | None = None

    def layer_map(self) -> dict[str, str]:
        if self.map is None:
            self.map = load_layer_map()
        return self.map

    def classify(self, rel: str) -> tuple[str, str, list[str]]:
        """(face, rule label, layers). Raises DerivationError on an unclassifiable path."""
        p = PurePosixPath(rel)
        if p.suffix == ".md" or rel.startswith("doc/"):
            return "docs", "docs rule (.md or doc/**)", []
        if rel.startswith("src/gui/"):
            return "gui", "src/gui/**", FACE_LAYERS["gui"]
        if rel == "src/main.cpp" or rel.startswith("src/launcher/"):
            return "shell", "src/main.cpp, src/launcher/**", FACE_LAYERS["shell"]
        if rel.startswith("src/"):
            layer = self.layer_map().get(rel)
            if layer is None:
                raise DerivationError(
                    f"{rel} is not in the layer map — if it is a new source file, add it "
                    f"to cmake/lumice_layers.cmake and run ./scripts/build.sh -gtj release "
                    f"first, then inner"
                )
            if layer in ("raypath", "analytic"):
                return "raypath", f"layer:{layer}", FACE_LAYERS["raypath"]
            return "core", f"layer:{layer}", FACE_LAYERS["core"]
        for label, prefix, face_layers in TEST_ROOT_RULES:
            if rel.startswith(prefix):
                return f"test:{label}", label, face_layers
        if rel.startswith(UNIT_TU_PREFIX):
            if p.suffix in CXX_SUFFIXES:
                return "test-unit-tu", "test/unit-correctness/** (per-TU suites)", FACE_LAYERS["test-unit-tu"]
            return "test-misc", "test misc", FACE_LAYERS["test-misc"]
        if rel.startswith("test/"):
            return "test-misc", "test misc", FACE_LAYERS["test-misc"]
        if rel == "CMakeLists.txt" or rel.startswith("cmake/"):
            return "build-system", "root CMakeLists.txt, cmake/**", FACE_LAYERS["build-system"]
        if rel.startswith("scripts/"):
            return "scripts", "scripts/**", FACE_LAYERS["scripts"]
        raise DerivationError(
            f"{rel} maps to no face — the minimal runnable set is `./scripts/test.sh quick` "
            f"(run it, or extend the face table in scripts/test_inner.py if this path "
            f"deserves a rule)"
        )


def is_kernel(rel: str) -> bool:
    return PurePosixPath(rel).suffix in KERNEL_SUFFIXES or rel.startswith(KERNEL_PREFIXES)


def derive(files: list[str], base: str, from_diff: bool) -> tuple[dict, list[tuple[str, str, str]]]:
    """The derivation chain. Returns (plan, per-file rows); raises DerivationError on refusal."""
    faces = Faces()
    per_file: list[tuple[str, str, str]] = []
    layers: list[str] = []
    kernel = False
    untracked_n = 0
    deleted_n = 0
    map_stale_hint = False

    def add_layers(new: list[str]) -> None:
        for layer in new:
            if layer not in layers:
                layers.append(layer)

    for rel in files:
        face, rule, face_layers = faces.classify(rel)
        per_file.append((rel, rule, face))
        add_layers(face_layers)
        if is_kernel(rel):
            kernel = True
        if rel.startswith("cmake/"):
            map_stale_hint = True

    if from_diff:
        untracked = [u for u in git_out(["ls-files", "--others", "--exclude-standard"]).splitlines() if u]
        untracked_n = len(untracked)
        for u in untracked:
            face, rule, face_layers = faces.classify(u)
            per_file.append((u, f"{rule} (untracked)", face))
            add_layers(face_layers)
            if is_kernel(u):
                kernel = True
        deleted = [
            d for d in git_out(["diff", "-M", "--diff-filter=D", "--name-only", base]).splitlines() if d
        ]
        deleted_n = len(deleted)

    if kernel and "parity" not in layers:
        layers.append("parity")

    # The always-on layer: the four diff-scoped gates plus the format check run on
    # every inner invocation, whatever the diff is.
    add_layers(["checkers", "format"])

    requested_layers = layers.copy()
    covered: dict[str, str] = {}
    if "ctest" in layers:
        for child in ("ctest-unit-correctness", "composition-gtest", "parity", "composition-gui", "golden-analytic"):
            if child in layers:
                covered[child] = "ctest (same registered test, selection and environment)"
    if "ctest" in layers or "ctest-unit-correctness" in layers:
        # The direct unit command supplies its own filter. A CTest invocation
        # inheriting GTEST_FILTER could therefore select a different set.
        if "unit-gtest" in layers and not any(
            key.startswith("GTEST_") and key != "GTEST_COLOR" for key in os.environ
        ):
            covered["unit-gtest"] = (
                "ctest" if "ctest" in layers else "ctest-unit-correctness"
            ) + " (whole unit binary includes the selected suites)"
    layers = [layer for layer in layers if layer not in covered]
    build_needed = any(layer in COMPILED_LAYERS for layer in layers)
    filter_str = None
    suites_payload = None
    if "unit-gtest" in requested_layers:
        raypath_face = any(face == "raypath" for _, _, face in per_file)
        tu_face = any(face == "test-unit-tu" for _, _, face in per_file)
        tus: list[str] = []
        if raypath_face:
            for sub in ("raypath", "analytic"):
                tus += sorted(
                    str(p.relative_to(REPO))
                    for p in (REPO / UNIT_TU_PREFIX / sub).glob("*.cpp")
                )
        if tu_face:
            tus += sorted(
                f for f in files
                if f.startswith(UNIT_TU_PREFIX) and PurePosixPath(f).suffix in CXX_SUFFIXES
            )
        arms, empty, plain, param = suites_of(tus)
        for rel in empty:
            print(f"note: {rel} declares no TEST/TEST_F/TEST_P suite", file=sys.stderr)
        # A parse that comes up empty falls back to the whole binary — running more,
        # never less; a filter is only emitted when it names at least one suite.
        if arms:
            filter_str = ":".join(arms)
            suites_payload = {
                "plain": sorted(plain),
                "param": {s: sorted(p) for s, p in sorted(param.items())},
            }

    plan = {
        "base": base,
        "layers": layers,
        "requested_layers": requested_layers,
        "covered": covered,
        "gtest_filter": filter_str,
        "gtest_suites": suites_payload,
        "build_needed": build_needed,
        "kernel": kernel,
        "untracked": untracked_n,
        "deleted_unmapped": deleted_n,
        "map_stale_hint": map_stale_hint,
    }
    return plan, per_file


def print_chain(per_file: list[tuple[str, str, str]], plan: dict, from_diff: bool) -> None:
    for rel, rule, face in per_file:
        print(f"  {rel}  ->  {rule}  ->  face:{face}", file=sys.stderr)
    if from_diff:
        if plan["untracked"]:
            print(
                f"  note: {plan['untracked']} untracked file(s) joined the derivation but stay "
                f"invisible to the four diff-scoped checkers until `git add` (the same scope "
                f"the pre-commit hook sees); the commit gate covers them",
                file=sys.stderr,
            )
        if plan["deleted_unmapped"]:
            print(
                f"  note: {plan['deleted_unmapped']} deleted file(s) map to no face (deletions "
                f"select nothing); the always-on checkers still run",
                file=sys.stderr,
            )
    if plan["kernel"]:
        print("  note: kernel-mode file hit -> parity layer added", file=sys.stderr)
    if plan["map_stale_hint"]:
        print(
            "  note: cmake/** changed — the layer map may lag this diff (this derivation used "
            "the last configure's map; the next build reconfigures and self-heals)",
            file=sys.stderr,
        )
    print(f"  faces -> requested layers: {' '.join(plan['requested_layers'])}", file=sys.stderr)
    for child, parent in plan["covered"].items():
        print(f"  covered: {child} -> {parent}", file=sys.stderr)
    print(f"  executed layers: {' '.join(plan['layers'])}", file=sys.stderr)


def normalize_rel(raw: str) -> str:
    path = Path(raw)
    if path.is_absolute():
        try:
            return path.resolve().relative_to(REPO).as_posix()
        except ValueError:
            raise DerivationError(f"{raw} is outside the repository") from None
    return PurePosixPath(raw).as_posix()


def validate_coverage(plan: dict) -> None:
    """Check the current CTest registration, after the build reconfigured it."""
    covered = plan["covered"]
    if not covered:
        return

    def metadata(*selection: str) -> dict[str, dict]:
        proc = subprocess.run(
            ["ctest", "--test-dir", BUILD_TREE, "--show-only=json-v1", *selection],
            cwd=REPO, capture_output=True, text=True,
        )
        if proc.returncode:
            raise RuntimeError(f"CTest metadata failed: {proc.stderr}")
        return {test["name"]: test for test in json.loads(proc.stdout)["tests"]}

    full = metadata("-L", "unit-correctness|composition-correctness|parity|golden-analytic")
    subsets = {
        "ctest-unit-correctness": metadata("-L", "unit-correctness"),
        "unit-gtest": metadata("-R", "^LumiceUnitCorrectnessTest$"),
        "composition-gtest": metadata("-R", "^LumiceRaypathCompositionTest$"),
        "parity": metadata("-R", "LumiceParityTest"),
        "composition-gui": metadata("-R", "LumiceCompositionCorrectnessTest"),
        "golden-analytic": metadata("-R", "LumiceGoldenAnalyticTest"),
    }
    for child in covered:
        parent = full if "ctest" in plan["layers"] else subsets["ctest-unit-correctness"]
        subset = subsets[child]
        if not subset or not subset.keys() <= parent.keys():
            raise RuntimeError(f"{child}: CTest registration no longer contained in its parent")
        for name, test in subset.items():
            if test["command"] != parent[name]["command"] or test["properties"] != parent[name]["properties"]:
                raise RuntimeError(f"{child}: CTest execution differs for {name}")
            if child in ("unit-gtest", "composition-gtest"):
                props = {p["name"]: p["value"] for p in test["properties"]}
                if props.keys() & {"ENVIRONMENT", "ENVIRONMENT_MODIFICATION", "WILL_FAIL", "DISABLED"}:
                    raise RuntimeError(f"{child}: CTest overrides direct gTest semantics for {name}")
                if Path(props.get("WORKING_DIRECTORY", str(REPO / BUILD_TREE / "test"))).resolve() != (
                    REPO / BUILD_TREE / "test"
                ).resolve():
                    raise RuntimeError(f"{child}: CTest working directory differs for {name}")
                argv = test["command"]
                expected = (
                    [REPO / "test/fixtures/v3_config.json", REPO / BUILD_TREE / "test", REPO]
                    if child == "unit-gtest" else []
                )
                binary_name = "unit_correctness_test" if child == "unit-gtest" else "raypath_composition_test"
                target_name = "unit" if child == "unit-gtest" else "composition"
                if (
                    len(argv) != len(expected) + 4
                    or Path(argv[1]).resolve() != (REPO / "scripts/gtest_parallel.py").resolve()
                    or argv[2] != target_name
                    or Path(argv[3]).stem != binary_name
                    or [str(Path(a).resolve()) for a in argv[4:]] != [str(p.resolve()) for p in expected]
                ):
                    raise RuntimeError(f"{child}: direct gTest argv no longer matches CTest for {name}")
    print(f"test_inner: verified CTest containment for {', '.join(covered)}")


# --- subcommands -------------------------------------------------------------------


def cmd_faces(args: argparse.Namespace) -> int:
    if args.selfcheck:
        return selfcheck()
    base = args.base or default_base()
    if args.files:
        from_diff = False
        try:
            files = [normalize_rel(f) for f in args.files]
        except DerivationError as error:
            print(f"test_inner: cannot derive a face set: {error}", file=sys.stderr)
            return 2
    else:
        files = [
            f for f in git_out(["diff", "-M", "--diff-filter=AMR", "--name-only", base]).splitlines() if f
        ]
        from_diff = True
    try:
        plan, per_file = derive(files, base, from_diff)
        print_chain(per_file, plan, from_diff)
    except DerivationError as error:
        print(f"test_inner: cannot derive a face set: {error}", file=sys.stderr)
        return 2
    json.dump(plan, sys.stdout)
    print()
    return 0


def violations_of(verdict: object) -> list:
    """The three verdict shapes the four seams return: (violations, scanned)
    tuples, bare violation lists, and check_new_test_tus's Verdict."""
    if isinstance(verdict, tuple):
        return verdict[0]
    if hasattr(verdict, "violations"):
        return verdict.violations
    return verdict  # type: ignore[return-value]  # list[Violation]


def cmd_checkers(args: argparse.Namespace) -> int:
    from check_loop_fatal_asserts import diff_verdict as loop_fatal_verdict
    from check_new_gui_tests import diff_verdict as gui_tests_verdict
    from check_new_refs import diff_verdict as new_refs_verdict
    from check_new_test_tus import diff_verdict as test_tus_verdict

    base = args.base or default_base()
    diff_args = [base]
    content_ref = None  # the worktree: an iteration change is usually unstaged
    base_ref = base

    checks = [
        ("new-refs", new_refs_verdict(diff_args, content_ref)),
        ("new-gui-tests", gui_tests_verdict(diff_args, content_ref, base_ref)),
        ("loop-fatal-asserts", loop_fatal_verdict(diff_args, content_ref)),
        ("new-test-tus", test_tus_verdict(diff_args, content_ref, base_ref)),
    ]

    untracked = [u for u in git_out(["ls-files", "--others", "--exclude-standard"]).splitlines() if u]
    if untracked:
        print(
            f"checkers: {len(untracked)} untracked file(s) are invisible to these gates until "
            f"`git add` (same diff scope as the pre-commit hook); the commit gate covers them"
        )

    failed = False
    for name, verdict in checks:
        violations = violations_of(verdict)
        if violations:
            failed = True
            print(f"checker {name}: FAIL ({len(violations)} violation(s))", file=sys.stderr)
            for v in sorted(violations, key=lambda x: (str(x.path), x.line)):
                print(v.render(), file=sys.stderr)
        else:
            detail = ""
            if isinstance(verdict, tuple):
                detail = f" ({verdict[1]} scanned)"
            print(f"checker {name}: PASS{detail}")

    # One uniform verdict->exit mapping for all four: violations non-empty means 1.
    # For new-test-tus the "TU half only, notices never gate" boundary is carried by
    # its diff_verdict, which is the only thing consulted here.
    return 1 if failed else 0


def build_type() -> str:
    cache = REPO / BUILD_TREE / "CMakeCache.txt"
    if not cache.exists():
        raise RuntimeError(
            f"{cache} not found — configure the static test tree first: ./scripts/build.sh -gtj release"
        )
    for line in cache.read_text(encoding="utf-8").splitlines():
        if line.startswith("CMAKE_BUILD_TYPE:"):
            value = line.partition("=")[2]
            if value:
                return value
    raise RuntimeError(f"CMAKE_BUILD_TYPE missing from {cache}")


PASSED_RE = re.compile(r"\[\s*PASSED\s*\]\s*(\d+) tests?")


def cmd_gtest(args: argparse.Namespace) -> int:
    plan = json.loads(Path(args.plan_file).read_text(encoding="utf-8"))
    build_type_value = build_type()
    bin_dir = REPO / "build" / build_type_value / "static" / "bin"

    if args.target == "unit":
        binary = bin_dir / "unit_correctness_test"
        filter_str = plan.get("gtest_filter")
        # The ctest registration passes three positional argv (fixtures, test build
        # dir, repo root); the direct call replicates them, or unrelated suites fail
        # on missing paths and read as real failures.
        argv = [str(binary)]
        if filter_str:
            argv.append(f"--gtest_filter={filter_str}")
        argv += [
            str(REPO / "test" / "fixtures" / "v3_config.json"),
            str(REPO / BUILD_TREE / "test"),
            str(REPO),
        ]
        suites = plan.get("gtest_suites") or {}
        reconcile_sets: tuple[set[str], dict[str, set[str]]] = (
            set(suites.get("plain", [])),
            {s: set(p) for s, p in (suites.get("param") or {}).items()},
        )
    else:
        binary = bin_dir / "raypath_composition_test"
        filter_str = None
        argv = [str(binary)]
        reconcile_sets = (set(), {})

    if not binary.exists():
        print(
            f"test_inner: {binary} not found — build the static test tree first: "
            f"./scripts/build.sh -gtj release",
            file=sys.stderr,
        )
        return 1

    if filter_str:
        plain, param = reconcile_sets
        missing = reconcile(binary, filter_str, plain, param)
        if missing:
            print(
                "test_inner: suite reconciliation FAILED — the filter selected none of: "
                + ", ".join(missing)
                + " (a parsed suite that selects nothing is a silent miss; fix the filter, "
                "do not widen it blindly)",
                file=sys.stderr,
            )
            return 1
        print(f"test_inner: reconciliation ok ({len(plain) + len(param)} parsed suites all selected)")

    # One run, streamed live and parsed as it goes: gtest exits 0 when a filter
    # matches nothing, so the PASSED count is the only thing that separates a real
    # pass from a silent empty selection.
    proc = subprocess.Popen(
        [sys.executable, str(REPO / "scripts/gtest_parallel.py"), args.target, *argv],
        cwd=REPO / BUILD_TREE / "test",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )
    passed_n: int | None = None
    assert proc.stdout is not None
    for line in proc.stdout:
        sys.stdout.write(line)
        m = PASSED_RE.search(line)
        if m:
            passed_n = int(m.group(1))
    proc.wait()
    if proc.returncode != 0:
        return proc.returncode
    if passed_n is None or passed_n == 0:
        print(
            "test_inner: the run reported 0 passed tests — treating as FAIL (an empty "
            "selection must never read as green)",
            file=sys.stderr,
        )
        return 1
    return 0


def selfcheck() -> int:
    """Detection-power checks for the suite parser and the reconciliation guard."""
    tus: list[str] = []
    for sub in ("raypath", "analytic"):
        tus += sorted(str(p.relative_to(REPO)) for p in (REPO / UNIT_TU_PREFIX / sub).glob("*.cpp"))
    ok = True
    plain_all: set[str] = set()
    param_all: dict[str, set[str]] = {}
    for rel in tus:
        plain, param = parse_suites((REPO / rel).read_bytes().decode("utf-8", errors="replace"))
        if not plain and not param:
            print(f"[FAIL] {rel}: parsed zero suites", file=sys.stderr)
            ok = False
        plain_all |= plain
        for suite, prefixes in param.items():
            param_all.setdefault(suite, set()).update(prefixes)
    if ok:
        print(
            f"[ok] all {len(tus)} unit-correctness/raypath+analytic TUs parse non-empty "
            f"({len(plain_all)} plain + {len(param_all)} parameterized suites)"
        )

    build_type_value = build_type()
    binary = REPO / "build" / build_type_value / "static" / "bin" / "unit_correctness_test"
    if not binary.exists():
        print(
            f"[FAIL] {binary} not found — build the static test tree first "
            f"(./scripts/build.sh -gtj release); the parameterized-selection assertion "
            f"needs the real binary",
            file=sys.stderr,
        )
        return 2

    li = REPO / UNIT_TU_PREFIX / "analytic" / "test_li_parity.cpp"
    li_plain, li_param = parse_suites(li.read_bytes().decode("utf-8", errors="replace"))
    arms = filter_arms(li_plain, li_param)
    if not li_param:
        print("[FAIL] test_li_parity.cpp: expected TEST_P suites, parsed none", file=sys.stderr)
        return 1
    headers = selected_suite_headers(binary, ":".join(arms))
    missing = reconcile(binary, ":".join(arms), li_plain, li_param)
    instanced = [s for s in li_param if any(h.endswith("/" + s + ".") for h in headers)]
    if missing:
        print(f"[FAIL] test_li_parity.cpp suites not selected: {missing}", file=sys.stderr)
        ok = False
    elif not instanced:
        print(
            "[FAIL] test_li_parity.cpp: parameterized suites selected only in bare form — "
            "the Prefix/Suite. arms did not reach the instances",
            file=sys.stderr,
        )
        ok = False
    else:
        print(
            f"[ok] test_li_parity.cpp: {len(li_param)} parameterized suite(s) selected in "
            f"Prefix/Suite. form (instantiation prefixes: "
            f"{sorted({p for ps in li_param.values() for p in ps})})"
        )
    return 0 if ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    faces = sub.add_parser("faces", help="diff -> faces -> layer plan (JSON on stdout)")
    faces.add_argument("--base", default=None, help="diff base (default: merge-base HEAD main)")
    faces.add_argument("--files", nargs="+", default=None, help="explicit file list override")
    faces.add_argument(
        "--selfcheck",
        action="store_true",
        help="assert the suite parser's detection power instead of deriving",
    )
    faces.set_defaults(func=cmd_faces)

    coverage = sub.add_parser("coverage", help="validate CTest containment after the build")
    coverage.add_argument("--plan-file", required=True)
    coverage.set_defaults(func=lambda args: validate_coverage(
        json.loads(Path(args.plan_file).read_text(encoding="utf-8"))
    ) or 0)

    checkers = sub.add_parser("checkers", help="run the four diff-scoped gates")
    checkers.add_argument("--base", default=None, help="diff base (default: merge-base HEAD main)")
    checkers.set_defaults(func=cmd_checkers)

    gtest = sub.add_parser("gtest", help="run one direct gtest binary behind the guards")
    gtest.add_argument("--plan-file", required=True)
    gtest.add_argument("--target", choices=["unit", "composition"], required=True)
    gtest.set_defaults(func=cmd_gtest)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

