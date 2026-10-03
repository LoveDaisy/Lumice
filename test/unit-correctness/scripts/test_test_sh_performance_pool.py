"""Exercise test.sh's slow pools with real pytest collection, not a copied splitter.

Only ctest and shared-library availability are stubbed. The pytest shim records
argv before removing xdist (not installed in the policy job) and collecting tiny
real cases instead of running simulations. Configured roots bypass --ignore;
the positive control pins that mechanism alongside the fixed entrypoint.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import pytest

SCRIPT = Path(__file__).resolve().parents[3] / "scripts" / "test.sh"
ROOTS = ["test/e2e-correctness", "test/performance", "test/extra correctness"]
pytestmark = pytest.mark.skipif(shutil.which("bash") is None, reason="test.sh needs bash")


def _write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _config(root: Path, roots: list[str]) -> None:
    _write(root / "pyproject.toml", "[tool.pytest.ini_options]\ntestpaths = [\n"
           + "".join(f"    {json.dumps(path)},\n" for path in roots)
           + ']\naddopts = ["-m", "not slow"]\nmarkers = ["slow: slow pool"]\n')


@pytest.fixture
def tree(tmp_path: Path):
    _write(tmp_path / "scripts/test.sh", SCRIPT.read_text(encoding="utf-8"))
    _config(tmp_path, ROOTS)
    for i, root in enumerate(ROOTS):
        _write(tmp_path / root / f"test_case_{i}.py", "import pytest\n"
               "@pytest.mark.slow\ndef test_slow(): pass\n"
               "def test_fast(): pass\n")
    _write(tmp_path / "build/cmake_build/static/CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=Release\n")
    _write(tmp_path / "test/e2e/capi_runner.py",
           'def lib_candidates(root, build_type): return [root / "liblumice_testapi.so"]\n')
    _write(tmp_path / "test/__init__.py", "")
    _write(tmp_path / "test/e2e/__init__.py", "")
    _write(tmp_path / "liblumice_testapi.so", "")
    bin_dir = tmp_path / "bin"
    _write(bin_dir / "ctest", "#!/bin/sh\nexit 0\n")
    # Pin python3 to the interpreter providing pytest and its TOML dependencies.
    _write(bin_dir / "python3", f'#!/bin/sh\nexec "{sys.executable}" "$@"\n')
    _write(bin_dir / "pytest", f"#!{sys.executable}\n" + '''import json
from pathlib import Path
import subprocess
import sys

args = sys.argv[1:]
log = Path("calls.jsonl")
index = len(log.read_text().splitlines()) if log.exists() else 0
collect_args = []
i = 0
while i < len(args):
    if args[i] == "-n":
        i += 2
    else:
        collect_args.append(args[i])
        i += 1
result = subprocess.run([sys.executable, "-m", "pytest", *collect_args,
                         "--collect-only", "-q"], capture_output=True, text=True)
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
ids = [line for line in result.stdout.splitlines() if "::" in line]
with log.open("a") as out:
    out.write(json.dumps({"argv": args, "ids": ids, "exit": result.returncode}) + "\\n")
overrides = Path("exit-codes.json")
codes = json.loads(overrides.read_text()) if overrides.exists() else {}
sys.exit(result.returncode or codes.get(str(index), 0))
''')
    for path in bin_dir.iterdir():
        path.chmod(0o755)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("PYTEST_")}
    env.update(PATH=str(bin_dir) + os.pathsep + env["PATH"], CI="1",
               LUMICE_LIB=str(tmp_path / "liblumice_testapi.so"),
               PYTEST_DISABLE_PLUGIN_AUTOLOAD="1")
    return tmp_path, env


def _collect(root: Path, env: dict, *args: str) -> set[str]:
    result = subprocess.run([sys.executable, "-m", "pytest", *args,
                             "-m", "slow", "--collect-only", "-q"],
                            cwd=root, env=env, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return {line for line in result.stdout.splitlines() if "::" in line}


def _run(tree, scope="pr"):
    root, env = tree
    result = subprocess.run(["bash", "scripts/test.sh", scope], cwd=root, env=env,
                            capture_output=True, text=True, timeout=30)
    log = root / "calls.jsonl"
    calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
    return result, calls


def test_slow_pools_partition_configured_roots(tree):
    root, env = tree
    baseline = _collect(root, env)
    performance = {node for node in baseline if node.startswith("test/performance/")}
    assert performance, baseline
    ignored = _collect(root, env, "--ignore=test/performance")
    assert ignored == baseline, "positive control must reproduce the configured-root bypass"

    result, calls = _run(tree)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RESULT: PASS" in result.stdout
    assert len(calls) == 3, calls
    fast, correctness, serial = calls
    assert all(call["exit"] == 0 for call in calls), calls
    first, second = set(correctness["ids"]), set(serial["ids"])
    assert not first & performance, correctness
    assert second == performance
    assert not first & second
    assert first | second == baseline
    assert not set(fast["ids"]) & baseline
    assert len(fast["ids"]) == len(ROOTS)
    assert correctness["argv"] == [ROOTS[0], ROOTS[2], "-n", "3", "-m", "slow"]
    assert serial["argv"] == ["test/performance", "-m", "slow"]
    for name in ("correctness", "performance"):
        assert re.search(rf"slow-e2e/{name}: exit=0 elapsed=\d+s", result.stdout)


@pytest.mark.parametrize("rc1,rc2", [(4, 0), (0, 3), (4, 3)])
def test_slow_pools_do_not_fail_fast_and_keep_error_priority(tree, rc1, rc2):
    root, _ = tree
    _write(root / "exit-codes.json", json.dumps({"1": rc1, "2": rc2}))
    result, calls = _run(tree)
    assert len(calls) == 3, result.stdout + result.stderr
    assert result.returncode == 1  # scope aggregates failures; layer retains the real code
    assert "RESULT: FAIL" in result.stdout
    assert re.search(rf"\[FAIL\] slow-e2e\s+\d+s\s+\(exit={rc1 or rc2}\)", result.stdout)
    assert f"slow-e2e/correctness: exit={rc1}" in result.stdout
    assert f"slow-e2e/performance: exit={rc2}" in result.stdout


@pytest.mark.parametrize("roots", [[], [ROOTS[0]], [*ROOTS, "test/performance"], ["test/performance"]])
def test_invalid_pool_roots_fail_before_any_layer(tree, roots):
    root, _ = tree
    _config(root, roots)
    result, calls = _run(tree)
    assert result.returncode == 2, result.stdout + result.stderr
    assert "testpaths" in result.stderr
    assert not calls
    assert "[RUN]" not in result.stdout


@pytest.mark.parametrize("scope", ["quick", "full"])
def test_non_pr_scopes_do_not_require_performance_root(tree, scope):
    root, _ = tree
    _config(root, [ROOTS[0]])
    result, calls = _run(tree, scope)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RESULT: PASS" in result.stdout
    assert len(calls) == 1


def test_testpaths_uses_toml_semantics_not_line_layout(tree):
    root, _ = tree
    config = root / "pyproject.toml"
    config.write_text("[tool.pytest.ini_options]\n"
                      "testpaths = ['test/e2e-correctness', 'test/performance', 'test/extra correctness'] # roots\n"
                      'addopts = ["-m", "not slow"]\nmarkers = ["slow: slow pool"]\n', encoding="utf-8")
    result, calls = _run(tree)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(calls) == 3
    assert calls[1]["argv"] == [ROOTS[0], ROOTS[2], "-n", "3", "-m", "slow"]


@pytest.mark.parametrize("setting", ["", 'testpaths = "test/performance"\n', 'testpaths = [\n'])
def test_unreadable_testpaths_fails_before_any_layer(tree, setting):
    root, _ = tree
    _write(root / "pyproject.toml", '[tool.pytest.ini_options]\naddopts = ["-m", "not slow"]\n' + setting)
    result, calls = _run(tree)
    assert result.returncode == 2, result.stdout + result.stderr
    assert "could not read pyproject.toml testpaths" in result.stderr
    assert not calls
    assert "[RUN]" not in result.stdout
