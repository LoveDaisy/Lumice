"""liblumice_analytic writes nothing of its own: loading it and calling it leaves stderr empty.

Every engine binary inherits a console sink on stderr from util/logger.hpp's GetSharedSink().
liblumice_analytic takes that sink out of its own copy while the library loads (a namespace-scope
object in src/analytic/analytic_api.cpp) and hands diagnostics to the host only through
LUMICE_ANALYTIC_SetLogCallback (doc/analytic-api.md section 6).

What this proves, and what it does not. LUMICE_ANALYTIC_SetLogCallback logs one INFO line through
the engine's global logger when a callback is installed. That line is the observable here: it
must arrive at the host's callback, at LUMICE_ANALYTIC_LOG_INFO, and must not appear on the
child's stderr — so a deleted or link-stripped silencing object goes red, and so does a callback
sink that never got attached. Loading and calling without a callback must leave stderr empty too.
It does NOT drive a real engine warning (crystal.cpp, geo3d_closedform.cpp) through the library,
because the library exposes no function that can warn yet; that end-to-end path belongs to the
first module that calls the crystal code. The warning-level half of the mechanism — LOG_WARNING
silent once the console sink is removed, delivered at LUMICE_ANALYTIC_LOG_WARNING with its text
intact — is pinned in-process by
test/unit-correctness/util/test_logger_default_console_sink_removable.cpp.

Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in a CUDA
configure, which does not produce the library. Free of test/e2e's helpers and of numpy/Pillow, like
test_export_symbol_scope.py, so the Windows shared-export CI legs can run it too.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SHARED_OUT = ROOT / "build" / "Release" / "shared"

pytestmark = pytest.mark.slow

_PATTERN = {"win32": "lumice_analytic.dll", "darwin": "liblumice_analytic.dylib", "linux": "liblumice_analytic.so"}


def _platform() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def _cuda_configured() -> bool:
    for cache in (ROOT / "build" / "CMakeCache.txt", ROOT / "build" / "cmake_build" / "shared" / "CMakeCache.txt"):
        if cache.is_file():
            text = cache.read_text(encoding="utf-8", errors="replace")
            if "BUILD_SHARED_LIBS:BOOL=ON" in text and "LUMICE_CUDA_ENABLED:BOOL=ON" in text:
                return True
    return False


def _find() -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    pattern = _PATTERN[_platform()]
    hits = sorted(p for p in SHARED_OUT.rglob(pattern) if p.is_file())
    if not hits:
        if _cuda_configured():
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        pytest.fail(f"{pattern} not found under {SHARED_OUT} although a shared build exists there")
    if len(hits) > 1:
        pytest.fail(f"more than one {pattern} under {SHARED_OUT}: {hits}")
    return hits[0]


def _run_child(body: str) -> subprocess.CompletedProcess[str]:
    code = "import ctypes, sys\n" f"lib = ctypes.CDLL({str(_find())!r})\n" f"{body}\n"
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"child failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


def test_load_and_call_without_callback_writes_nothing() -> None:
    proc = _run_child(
        "lib.LUMICE_ANALYTIC_GetApiVersion.restype = ctypes.c_int\n"
        "print(lib.LUMICE_ANALYTIC_GetApiVersion())"
    )
    assert proc.stderr == ""
    assert proc.stdout.strip().isdigit()


def test_callback_receives_the_line_stderr_does_not() -> None:
    proc = _run_child(
        "CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p)\n"
        "got = []\n"
        "cb = CB(lambda level, name, msg: got.append((level, name.decode(), msg.decode())))\n"
        "lib.LUMICE_ANALYTIC_SetLogCallback.argtypes = [CB]\n"
        "lib.LUMICE_ANALYTIC_SetLogCallback(cb)\n"
        "lib.LUMICE_ANALYTIC_GetApiVersion()\n"
        "lib.LUMICE_ANALYTIC_SetLogCallback(CB())\n"  # NULL: forwarding stops, library silent again
        "lib.LUMICE_ANALYTIC_GetApiVersion()\n"
        "print(repr(got))"
    )
    assert proc.stderr == ""
    got = eval(proc.stdout)  # noqa: S307 -- repr of a list of (int, str, str) written by our own child
    assert len(got) == 1, got
    level, name, msg = got[0]
    assert level == 3, got  # LUMICE_ANALYTIC_LOG_INFO
    assert name == "global"
    assert "log callback installed" in msg and not msg.endswith("\n"), msg
