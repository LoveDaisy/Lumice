"""The real quick orchestrator owns status aggregation and parallelism.

Use a file rendezvous rather than elapsed-time thresholds: the local ctest stub
cannot finish until the Windows child has observed it running, and vice versa.
No SSH or simulator is used by these orchestration tests.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest

SCRIPT = Path(__file__).resolve().parents[3] / "scripts/test.sh"
pytestmark = pytest.mark.skipif(shutil.which("bash") is None, reason="test.sh needs bash")


def _write(root, name, content):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


@pytest.fixture
def tree(tmp_path):
    _write(tmp_path, "scripts/test.sh", SCRIPT.read_text())
    _write(tmp_path, "build/cmake_build/static/CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=Release\n")
    _write(tmp_path, "pyproject.toml", 'addopts = ["-m", "not slow"]\n')
    _write(tmp_path, "bin/ctest", "#!/bin/sh\nexit 0\n")
    _write(tmp_path, "bin/pytest", "#!/bin/sh\nexit 0\n")
    # Keep GUI out of this contract without CI=1 (which explicitly disables win).
    env = dict(os.environ, PATH=str(tmp_path / "bin") + os.pathsep + os.environ["PATH"],
               CI="", LUMICE_SKIP_GUI_TESTS="1", LUMICE_WIN_QUICK_LEG="1")
    return tmp_path, env


def _run(tree, marker="PASS", code=0, scope="quick"):
    root, env = tree
    _write(root, "scripts/win_quick_leg.sh",
           f"#!/bin/sh\nprintf 'WIN_LEG_RESULT: %s\\n' '{marker}'\nexit {code}\n")
    result = subprocess.run(["bash", "scripts/test.sh", scope], cwd=root, env=env,
                            text=True, capture_output=True, timeout=15)
    summaries = [line for line in result.stdout.splitlines() if line.startswith("RESULT:")]
    assert len(summaries) == 1, result.stdout + result.stderr
    return result, summaries[0]


@pytest.mark.parametrize("marker,code,summary,exit_code", [
    ("PASS", 0, "PASS", 0),
    ("FAIL stage=build reason=command-failed", 1, "FAIL stage=build", 1),
    ("FAIL stage=ctest reason=no-tests", 1, "FAIL stage=ctest reason=no-tests", 1),
    ("FAIL stage=sync reason=tree-mismatch", 1, "FAIL stage=sync reason=tree-mismatch", 1),
    ("SKIPPED unreachable", 0, "SKIPPED unreachable", 0),
    ("SKIPPED busy", 0, "SKIPPED busy", 0),
    ("SKIPPED lfs-dirty resources/lumice.ico", 0, "SKIPPED lfs-dirty", 0),
    ("PASS", 7, "FAIL stage=transport reason=exit-7", 1),
    ("SKIPPED busy", 7, "FAIL stage=transport reason=exit-7", 1),
    ("garbage", 0, "FAIL stage=transport reason=invalid-result", 1),
    ("PASSgarbage", 0, "FAIL stage=transport reason=invalid-result", 1),
])
def test_status_and_exit_mapping(tree, marker, code, summary, exit_code):
    result, line = _run(tree, marker, code)
    assert result.returncode == exit_code, result.stdout + result.stderr
    assert f"(win: {summary}" in line
    assert line.startswith("RESULT: FAIL" if exit_code else "RESULT: PASS")
    assert "[PASS] fast-e2e" in result.stdout


@pytest.mark.parametrize("content", ["exit 9", "printf 'WIN_LEG_RESULT: PASS\\nWIN_LEG_RESULT: PASS\\n'"])
def test_missing_or_duplicate_marker_is_red(tree, content):
    root, env = tree
    _write(root, "scripts/win_quick_leg.sh", content + "\n")
    result = subprocess.run(["bash", "scripts/test.sh", "quick"], cwd=root, env=env,
                            text=True, capture_output=True, timeout=15)
    assert result.returncode == 1
    assert "reason=missing-or-duplicate-result" in result.stdout


@pytest.mark.parametrize("setting,ci", [("", ""), ("1", "1")])
def test_disabled_leg_keeps_result_shape(tree, setting, ci):
    root, env = tree
    env.update(LUMICE_WIN_QUICK_LEG=setting, CI=ci)
    result, line = _run(tree, "FAIL stage=build reason=must-not-run", 1)
    assert result.returncode == 0
    assert line == "RESULT: PASS"
    assert "[SKIP] win-quick-leg" in result.stdout
    assert "not-enabled" in result.stdout
    assert not (root / "build/win_quick_leg.last.log").exists()


def test_full_scope_does_not_start_windows_leg(tree):
    result, line = _run(tree, "FAIL stage=build reason=must-not-run", 1, scope="full")
    assert result.returncode == 0
    assert line == "RESULT: PASS"
    assert "win-quick-leg" not in result.stdout


def test_leg_is_parallel_and_uses_the_same_ctest_labels(tree):
    root, env = tree
    # Each child records its entry, then waits on the other. Sequential execution
    # cannot satisfy the handshake and is killed by subprocess.run's bound.
    _write(root, "bin/ctest", """#!/bin/sh
printf '%s\\n' "$*" > local-entered
while [ ! -f win-entered ]; do sleep 0.05; done
""")
    _write(root, "scripts/win_quick_leg.sh", """#!/bin/sh
printf '%s\\n' "$1" > win-entered
while [ ! -f local-entered ]; do sleep 0.05; done
printf 'WIN_LEG_RESULT: PASS\\n'
""")
    result = subprocess.run(["bash", "scripts/test.sh", "quick"], cwd=root, env=env,
                            text=True, capture_output=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    labels = (root / "win-entered").read_text().strip()
    assert labels in (root / "local-entered").read_text()
    assert "(win: PASS)" in result.stdout


@pytest.mark.parametrize("mutation,expected", [
    ("untracked-asset", "FAIL stage=sync reason=transfer"),
    ("tracked-asset", "SKIPPED lfs-dirty"),
    ("symlink", "FAIL stage=sync reason=package-exception"),
    ("gitlink", "FAIL stage=sync reason=package-exception"),
])
def test_real_packager_checks_head_entries_not_untracked_assets(tmp_path, mutation, expected):
    root = SCRIPT.parents[1]
    _write(tmp_path, "scripts/win_quick_leg.sh", (root / "scripts/win_quick_leg.sh").read_text())
    for name in ("win_quick_leg_remote.cmd", "win_quick_leg_remote.ps1"):
        _write(tmp_path, "scripts/" + name, (root / "scripts" / name).read_text())
    _write(tmp_path, "resources/asset.txt", "tracked asset\n")

    def git(*args):
        return subprocess.check_output(["git", *args], cwd=tmp_path, text=True).strip()

    git("init", "-q")
    git("config", "user.email", "test@example.invalid")
    git("config", "user.name", "Packager Test")
    git("add", ".")
    git("commit", "-qm", "fixture")
    if mutation == "symlink":
        try:
            (tmp_path / "linked-asset").symlink_to("resources/asset.txt")
        except OSError:
            pytest.skip("OS does not permit symlink creation")
        git("add", "linked-asset")
        git("commit", "-qm", "symlink fixture")
    elif mutation == "gitlink":
        git("update-index", "--add", "--cacheinfo", "160000," + git("rev-parse", "HEAD") + ",submodule")
        git("commit", "-qm", "gitlink fixture")
    elif mutation == "tracked-asset":
        (tmp_path / "resources/asset.txt").write_text("dirty tracked asset\n")
    else:
        (tmp_path / "resources/untracked.txt").write_text("not part of HEAD\n")
    # Exercise the actual HEAD packager, stopping at transfer without a network.
    _write(tmp_path, "bin/ssh", "#!/bin/sh\ncase \"$*\" in *' type '*) exit 1 ;; esac\nexit 0\n")
    _write(tmp_path, "bin/scp", "#!/bin/sh\nexit 27\n")
    env = dict(os.environ, PATH=str(tmp_path / "bin") + os.pathsep + os.environ["PATH"],
               LUMICE_WIN_QUICK_HOST="fixture", LUMICE_WIN_QUICK_WS="C:/fixture/ws-winleg")
    result = subprocess.run(["bash", "scripts/win_quick_leg.sh"], cwd=tmp_path, env=env,
                            text=True, capture_output=True, timeout=30)
    assert f"WIN_LEG_RESULT: {expected}" in result.stdout, result.stdout + result.stderr
    assert result.returncode == (0 if mutation == "tracked-asset" else 1)
