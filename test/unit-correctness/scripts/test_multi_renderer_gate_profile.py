"""The dual-renderer throughput gate's two profiles, judged without a GPU.

`test/e2e/_multi_renderer_throughput.py` holds two gates over one statistic: the
precise one (0.85 over 21 reps, reference machines and the local scheduled
gate) and the CI disaster floor (0.75 over 5 reps). These cases pin what each
profile passes and fails, that the precise profile still follows the local
gate's threshold-override plugin (it rebinds the module constant), and that the
line the local gate parses keeps its shape. The benchmark runs are replaced by
a fake that returns fixed rates, so the whole gate function runs end to end.
"""

import importlib
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import local_throughput_gate as lg  # noqa: E402

gate = importlib.import_module("test.e2e._multi_renderer_throughput")


class _FakeResult:
    def __init__(self, rps, backend):
        self.multi_rps = rps
        self.backend = backend
        self.fell_back = False
        self.multi_basis = "drain_aligned" if backend != "cpu" else "finite"


def _run_gate(monkeypatch, tmp_path, ratio, profile=None):
    """Run the gate with every dual sample at `ratio` x the single arms' rate."""
    calls = []

    def fake_run(config_path, backend_env, timeout_sec):
        name = str(config_path)
        calls.append(name)
        if backend_env is None:
            return _FakeResult(1.0e6, "cpu")
        rps = 10.0e6 * ratio if "dual" in name else 10.0e6
        return _FakeResult(rps, backend_env)

    monkeypatch.setattr(gate, "run_benchmark", fake_run)
    monkeypatch.setattr(gate, "write_infinite_variant", lambda path, tmp: path)
    kwargs = {} if profile is None else {"profile": profile}
    gate.run_dual_renderer_gate(tmp_path, tmp_path, "metal", 1, "unit", **kwargs)
    return calls


def test_ci_profile_fails_a_disaster_and_passes_a_mild_regression(monkeypatch, tmp_path):
    with pytest.raises(AssertionError, match="ci profile, gate >= 0.75"):
        _run_gate(monkeypatch, tmp_path, 0.6, "ci")
    _run_gate(monkeypatch, tmp_path, 0.8, "ci")


def test_precise_profile_is_the_default_and_fails_what_ci_passes(monkeypatch, tmp_path):
    with pytest.raises(AssertionError, match="precise profile, gate >= 0.85"):
        _run_gate(monkeypatch, tmp_path, 0.8)
    _run_gate(monkeypatch, tmp_path, 0.9)


def test_profiles_sample_their_own_rep_counts(monkeypatch, tmp_path):
    ci = _run_gate(monkeypatch, tmp_path, 1.0, "ci")
    precise = _run_gate(monkeypatch, tmp_path, 1.0, "precise")
    # three GPU arms per rep, plus the legacy reps (same count in both profiles)
    assert len(ci) == 3 * gate.N_REPS_DISASTER + gate.N_LEGACY_REPS
    assert len(precise) == 3 * gate.N_REPS + gate.N_LEGACY_REPS


def test_threshold_override_reaches_precise_but_not_ci(monkeypatch):
    monkeypatch.setattr(gate, "T_DUAL_VS_SINGLE", 0.95)
    assert gate.profile_params("precise") == (0.95, gate.N_REPS)
    assert gate.profile_params("ci") == (gate.T_DISASTER, gate.N_REPS_DISASTER)
    medians = {"dual": 9.0, "single_a": 10.0, "single_b": 10.0}
    assert gate.ratio_verdict(medians, "precise")[2] is False
    assert gate.ratio_verdict(medians, "ci")[2] is True


def test_unknown_profile_is_refused():
    with pytest.raises(ValueError):
        gate.profile_params("CI")


def test_the_printed_line_still_parses_for_the_local_gate(monkeypatch, tmp_path, capsys):
    _run_gate(monkeypatch, tmp_path, 0.9, "precise")
    parsed = lg.extract_ratios(capsys.readouterr().out)
    assert parsed["ratio_a"] == pytest.approx(0.9)
    assert parsed["ratio_b"] == pytest.approx(0.9)
    assert parsed["threshold"] == gate.T_DUAL_VS_SINGLE
    assert parsed["median_mrps_dual"] == pytest.approx(9.0)
    assert parsed["median_mrps_legacy"] == pytest.approx(1.0)


def test_each_interleaved_rep_prints_a_machine_readable_triplet(monkeypatch, tmp_path, capsys):
    monkeypatch.setattr(gate, "N_REPS", 2)
    _run_gate(monkeypatch, tmp_path, 0.9, "precise")
    rep_lines = [line for line in capsys.readouterr().out.splitlines() if " rep=" in line]
    assert rep_lines == [
        "[unit] rep=1/2 dual_rps=9000000.000000 single_a_rps=10000000.000000 "
        "single_b_rps=10000000.000000",
        "[unit] rep=2/2 dual_rps=9000000.000000 single_a_rps=10000000.000000 "
        "single_b_rps=10000000.000000",
    ]
