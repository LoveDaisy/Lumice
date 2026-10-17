"""Regression net for `scripts/run_bench_probe.py`.

The probe is the instrument that will anchor machine-independent e2e
enforcement, so its failure modes must fail the way the design says: an
unusable reading is recorded as `available: false` with a reason (and the
script still exits 0 — shadow mode never fails the job), while the derived
config carries exactly the fixed ray budget the tier reading depends on.
Every case here drives the real script end to end against a fake binary that
prints canned `[BENCHMARK]` lines — no engine involved.
"""
from __future__ import annotations

import json
import os
import stat
import subprocess
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
PROBE = SCRIPTS / "run_bench_probe.py"

VALID_SINGLE = {
    "mode": "single", "workers": 1, "cores": 4, "rays": 3000000,
    "wall_sec": 8.0, "setup_sec": 0.05, "active_sec": 7.9,
    "rays_per_sec": 375000.0, "rate_basis": "steady", "isa": "baseline",
}
VALID_MULTI = {
    "mode": "multi", "workers": 4, "cores": 4, "rays": 12000000,
    "wall_sec": 8.1, "setup_sec": 0.06, "active_sec": 8.0,
    "rays_per_sec": 1480000.0, "rate_basis": "steady", "isa": "baseline",
}


def write_fake_binary(tmp_path: Path, lines: list[str], *, exit_code: int = 0,
                      expect_ray_num: int | None = None) -> Path:
    """A fake Lumice that prints `lines` on stdout and optionally asserts the
    ray budget of the config it was handed (proving the derivation wiring)."""
    statements = [
        "#!/usr/bin/env python3",
        "import json, sys",
        "args = sys.argv[1:]",
        "assert args[:2] == ['benchmark', '-f'], args",
    ]
    if expect_ray_num is not None:
        statements += [
            "cfg = json.load(open(args[2]))",
            f"if cfg['scene']['ray_num'] != {expect_ray_num}:",
            "    print('ray_num mismatch:', cfg['scene']['ray_num']); sys.exit(3)",
        ]
    statements += [f"print({line!r})" for line in lines]
    statements.append(f"sys.exit({exit_code})")
    path = tmp_path / "fake_lumice.py"
    path.write_text("\n".join(statements) + "\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


def write_base_config(tmp_path: Path, ray_num: int = 10_000_000) -> Path:
    path = tmp_path / "bench_config.json"
    path.write_text(json.dumps({"scene": {"ray_num": ray_num}}), encoding="utf-8")
    return path


def bench_line(mode: str, **overrides) -> str:
    line = dict(VALID_SINGLE if mode == "single" else VALID_MULTI)
    line["mode"] = mode
    line.update(overrides)
    return "[BENCHMARK] " + json.dumps(line)


def run_probe(tmp_path: Path, binary: Path, base_config: Path) -> tuple[dict, str, subprocess.CompletedProcess]:
    output = tmp_path / "probe.json"
    proc = subprocess.run(
        [sys.executable, str(PROBE), "--binary", str(binary),
         "--base-config", str(base_config), "--ray-num", "3000000",
         "--output", str(output)],
        capture_output=True, text=True, check=False,
    )
    data = json.loads(output.read_text(encoding="utf-8"))
    return data, output.name, proc


def test_valid_probe_is_available_with_both_lines_and_a_notice(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single"), bench_line("multi")])
    data, _, proc = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert proc.returncode == 0, proc.stderr
    assert data["available"] is True
    assert data["single"]["rays_per_sec"] == 375000.0
    assert data["multi"]["workers"] == 4
    assert "::notice::bench probe: single=375000 rays/s" in proc.stdout


def test_probe_derives_the_config_with_the_fixed_ray_budget(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single"), bench_line("multi")],
                               expect_ray_num=3000000)
    data, _, proc = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert proc.returncode == 0, proc.stderr
    assert data["available"] is True


def test_wall_fallback_basis_is_unusable_and_still_exits_zero(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single", rate_basis="wall_fallback"),
                                          bench_line("multi")])
    data, _, proc = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert proc.returncode == 0
    assert data["available"] is False
    assert "rate_basis" in data["reason"]
    assert "::warning::bench probe unavailable:" in proc.stdout


def test_wrong_isa_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single", isa="native"), bench_line("multi")])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "isa" in data["reason"]


def test_fell_back_pass_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single", fell_back=True), bench_line("multi")])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "fell back" in data["reason"]


def test_nonpositive_rate_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single", rays_per_sec=0), bench_line("multi")])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "rays_per_sec" in data["reason"]


def test_missing_multi_pass_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single")])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "missing ['multi']" in data["reason"]


def test_failing_binary_is_recorded_with_its_return_code(tmp_path):
    binary = write_fake_binary(tmp_path, ["Error: something broke"], exit_code=1)
    data, _, proc = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert proc.returncode == 0
    assert data["available"] is False
    assert data["returncode"] == 1
    assert "exited 1" in data["reason"]


def test_output_without_benchmark_lines_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, ["nothing to see here"])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "missing" in data["reason"]


def test_unparseable_benchmark_line_is_unusable(tmp_path):
    binary = write_fake_binary(tmp_path, ["[BENCHMARK] {not json", bench_line("multi")])
    data, _, _ = run_probe(tmp_path, binary, write_base_config(tmp_path))
    assert data["available"] is False
    assert "unparseable" in data["reason"]


def test_output_lands_in_a_created_directory(tmp_path):
    binary = write_fake_binary(tmp_path, [bench_line("single"), bench_line("multi")])
    output = tmp_path / "nested" / "dir" / "probe.json"
    proc = subprocess.run(
        [sys.executable, str(PROBE), "--binary", str(binary),
         "--base-config", str(write_base_config(tmp_path)), "--ray-num", "3000000",
         "--output", str(output)],
        capture_output=True, text=True, check=False,
    )
    assert proc.returncode == 0, proc.stderr
    assert json.loads(output.read_text(encoding="utf-8"))["available"] is True
