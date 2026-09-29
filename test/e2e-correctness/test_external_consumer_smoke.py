"""liblumice_analytic is usable from its install tree alone, by a C program and by Python ctypes.

The other two tests of this library (test_export_symbol_scope.py, test_analytic_log_sink.py) read
the build tree. This one reads what a consumer gets: `cmake --install --component analytic`
produces a prefix holding only the header, the library and the package config, and two consumers
under external_consumer_smoke/ run against that prefix and nothing else —

- a standalone CMake project that does find_package(LumiceAnalytic), links
  Lumice::lumice_analytic and compares LUMICE_ANALYTIC_GetApiVersion() with the header's
  LUMICE_ANALYTIC_API_VERSION (the run-time check of doc/analytic-api.md section 8.1);
- smoke.py, which finds the library by the install-tree layout of doc/analytic-api.md section 8.7
  through LUMICE_ANALYTIC_INSTALL_DIR, and doubles as the sample for a Python binding.

"Install tree alone" is asserted, not assumed: every include directory in the C project's
compile_commands.json must resolve outside this repository, and the installed include directory
must be among them. The source file itself lives in the repository, so only include flags are
checked, never the whole command line.

The prefix is LUMICE_ANALYTIC_INSTALL_DIR when the caller sets it (CI points it at the prefix its
packaging step installed), and otherwise a fresh component install of the local shared build into
a temporary directory. Needs a shared build (`./scripts/build.sh -sj release`) for the latter;
skipped when there is none, or in a CUDA configure, which does not produce the library. Free of
test/e2e's helpers and of numpy/Pillow, like its two siblings, so the Windows shared-export CI legs
can run it too.

symmetry_semantics: none — neither consumer compares a face sequence across a symmetry
(doc/analytic-api.md section 3); besides the version query each evaluates one concrete path (3-5
through a regular prism) and checks only that it comes back valid.
"""
from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SMOKE_DIR = Path(__file__).resolve().parent / "external_consumer_smoke"
_BUILD_CANDIDATES = (ROOT / "build", ROOT / "build" / "cmake_build" / "shared")
_ENV = "LUMICE_ANALYTIC_INSTALL_DIR"

pytestmark = pytest.mark.slow

# Every compiler flag that adds an include search directory, on gcc/clang and cl/clang-cl. An
# imported target's INTERFACE_INCLUDE_DIRECTORIES arrive as -isystem (or /external:I), not -I, so
# checking -I alone would inspect nothing.
_INCLUDE_FLAGS = ("-isystem", "-iquote", "-idirafter", "-I", "/external:I", "-external:I", "/imsvc", "-imsvc", "/I")


def _shared_build_dir() -> Path:
    for build in _BUILD_CANDIDATES:
        cache = build / "CMakeCache.txt"
        if not cache.is_file():
            continue
        text = cache.read_text(encoding="utf-8", errors="replace")
        # The type tag is BOOL or UNINITIALIZED depending on how the -D was spelled.
        if not re.search(r"^BUILD_SHARED_LIBS:[A-Z]+=ON$", text, re.MULTILINE):
            continue
        if re.search(r"^LUMICE_CUDA_ENABLED:[A-Z]+=ON$", text, re.MULTILINE):
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        return build
    pytest.skip(f"no shared build among {[str(b) for b in _BUILD_CANDIDATES]} (./scripts/build.sh -sj release)")


def _run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False, **kwargs)
    assert proc.returncode == 0, f"{cmd} failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


@pytest.fixture(scope="module")
def install_prefix(tmp_path_factory: pytest.TempPathFactory) -> Path:
    preset = os.environ.get(_ENV)
    if preset:
        prefix = Path(preset)
        print(f"{_ENV} preset: using {prefix}")
    else:
        build = _shared_build_dir()
        prefix = tmp_path_factory.mktemp("analytic") / "install"
        print(f"{_ENV} unset: installing component analytic of {build} into {prefix}")
        _run(["cmake", "--install", str(build), "--component", "analytic", "--prefix", str(prefix)])
    assert (prefix / "include" / "lumice_analytic.h").is_file(), f"no lumice_analytic.h under {prefix}"
    return prefix.resolve()


@pytest.fixture(scope="module")
def expected_version(install_prefix: Path) -> int:
    header = (install_prefix / "include" / "lumice_analytic.h").read_text(encoding="utf-8")
    m = re.search(r"^#define LUMICE_ANALYTIC_API_VERSION ([0-9]+)$", header, re.MULTILINE)
    assert m, "LUMICE_ANALYTIC_API_VERSION not found in the installed header"
    return int(m.group(1))


def _include_dirs(entry: dict) -> list[str]:
    if "arguments" in entry:
        args = entry["arguments"]
    else:
        args = shlex.split(entry["command"], posix=not sys.platform.startswith("win"))
    dirs: list[str] = []
    i = 0
    while i < len(args):
        arg = args[i]
        for flag in _INCLUDE_FLAGS:
            if arg == flag and i + 1 < len(args):
                dirs.append(args[i + 1])
                i += 1
                break
            if arg.startswith(flag) and len(arg) > len(flag):
                dirs.append(arg[len(flag) :])
                break
        i += 1
    return [d.strip('"') for d in dirs]


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def _find_executable(build: Path) -> Path:
    name = "external_consumer_smoke.exe" if sys.platform.startswith("win") else "external_consumer_smoke"
    hits = [p for p in build.rglob(name) if p.is_file()]
    assert len(hits) == 1, f"expected one {name} under {build}, found {hits}"
    return hits[0]


def test_c_consumer_builds_and_runs_against_install_tree_only(
    install_prefix: Path, expected_version: int, tmp_path: Path
) -> None:
    if shutil.which("ninja") is None:
        pytest.fail("ninja not on PATH; the consumer project is configured with -G Ninja")
    build = tmp_path / "build"
    _run(
        [
            "cmake",
            "-S",
            str(SMOKE_DIR),
            "-B",
            str(build),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_PREFIX_PATH={install_prefix}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
    )
    _run(["cmake", "--build", str(build)])

    entries = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    assert entries, "compile_commands.json is empty"
    root = ROOT.resolve()
    installed_include = (install_prefix / "include").resolve()
    for entry in entries:
        dirs = [Path(entry["directory"], d).resolve() for d in _include_dirs(entry)]
        leaks = [str(d) for d in dirs if _is_within(d, root)]
        assert not leaks, f"{entry['file']} includes from the source tree {root}: {leaks}"
        assert installed_include in dirs, f"{entry['file']} does not include {installed_include}: {dirs}"

    proc = _run([str(_find_executable(build))])
    assert f"LUMICE_ANALYTIC_GetApiVersion={expected_version}" in proc.stdout, proc.stdout
    assert "LUMICE_ANALYTIC_EvaluatePath rc=0 valid=1 segments=3" in proc.stdout, proc.stdout


def test_python_ctypes_consumer_loads_install_tree(install_prefix: Path, expected_version: int) -> None:
    env = dict(os.environ, **{_ENV: str(install_prefix)})
    proc = _run([sys.executable, str(SMOKE_DIR / "smoke.py")], env=env)
    assert f"LUMICE_ANALYTIC_GetApiVersion={expected_version}" in proc.stdout, proc.stdout
    assert "LUMICE_ANALYTIC_EvaluatePath rc=0 valid=1 segments=3" in proc.stdout, proc.stdout
