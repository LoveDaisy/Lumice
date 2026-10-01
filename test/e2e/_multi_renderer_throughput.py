"""The dual-renderer throughput gate, shared by its Metal and CUDA test files.

Proposition. A GPU session that serves two renderers (the user's GUI document:
a preview projection plus an export projection) traces every ray once and
projects it twice, so its rays/s should be close to — not half of — the same
scene rendered with either renderer alone. Before N-plane accumulation landed,
such a config fell back to the legacy CPU path outright (one order of magnitude
slower); the gate pins that it stays on device AND that serving the second
plane costs no more than the design-study figure.

Threshold. ``T_DUAL_VS_SINGLE = 0.85``: dual rays/s ≥ 85% of EACH single
renderer's rays/s. The number comes from the multi-renderer design study's
prototype (a single-renderer engine projecting N times inside its exit tail),
and was re-measured on the landed backends with this scene by this gate:
Metal 0.950 / 0.908 and CUDA 0.906 / 0.992 by ratio of medians (n = 21). It
is a design target the landed code met, not a number fitted to the landed
code.

Statistic, and why it is not one sample per arm. A drain-aligned GPU rate on
this scene has a per-sample CoV of 6–10% on Metal (15 interleaved reps:
dual 23.2–30.1M, single 26.8–32.1M rays/s) and 10–17% on CUDA (clocks boost
freely, 180→2407 MHz) — a single dual/single pair can read 0.74 on a machine
whose true ratio is 0.93. So each arm is sampled repeatedly with the
three arms INTERLEAVED (dual, single_a, single_b, dual, ...) so that a thermal
or clock drift over the run lands on all three. Metal ``precise`` and the
Metal-only ``ci`` profile compare arm medians (21 and 5 reps respectively).
CUDA ``precise`` instead forms each interleaved rep's dual/single ratio and
uses a 10% winsorized mean over 63 reps: on 14 idle same-commit runs the old
ratio of medians crossed 0.85 once (estimated joint false-red rate 8.4%), while
resampling the measured paired triples puts this statistic's joint false-red
rate at about 0.7%. The same resampling gives 80% single-run power for a 10.7%
true ratio regression. A controlled 6.7 ms multi-plane-only window-tail delay
at that boundary was detected in 5/5 idle runs; the unmodified control passed
6/6. The threshold remains the design target; only CUDA's noisy estimator gets
backend-specific precision.

Denominator. Legacy CPU on the same dual config (finite 5M rays, the
committed fixture as-is; the GPU arms run its ``ray_num = "infinite"`` twin
because a finite ray_num is drain-quantized on the GPU route and reads a
noisy ~0.1 s window — see ``benchmark_cli.write_infinite_variant``). The
dual-vs-legacy ratio is printed as context and held only to a loose
``T_LEGACY_SANITY`` floor (2.0×) whose one job is to catch the whole session
falling back to the CPU path (which reads ~1.0×); it is not a precise gate,
because the denominator moves with every CPU-path optimisation. Measured:
Metal 4.2× (27.9M vs 6.7M rays/s), CUDA 21× (282.8M vs 13.4M rays/s) — on
their reference machines.

The gate's own routing guard is the [BENCHMARK] JSON's ``backend`` /
``fell_back`` (the C API's answer, printed by the CLI), on every sample.

Two profiles: the precise gate and the CI disaster gate. Everything above
describes the ``precise`` profile (``T_DUAL_VS_SINGLE`` with backend-specific
sampling), which is
the default: every caller that names no profile — the reference machines, the
CUDA file, the local scheduled gate (``scripts/local_throughput_gate.py``) —
gets it. The ``ci`` profile (``T_DISASTER`` x ``N_REPS_DISASTER``) exists
because a GitHub-hosted macOS runner is not the machine 0.85 was calibrated on:
its per-arm CoV measured 0.155-0.229 (two runs; absolute rate ~7M rays/s
against the reference machine's 28M), and a bootstrap of those samples puts
P(median ratio < 0.85) at 0.77% for 5 reps and 0.08% for 9 — a real 0.845 was
read on a PR with nothing wrong. So CI holds a loose floor instead: 0.75 over 5
interleaved reps, whose measured power is >= 99.9% against a true ratio of 0.6
and 91% against 0.7. It catches a disaster (the second plane costing a large
fraction of the session) and deliberately cannot see a 10% regression; that is
the precise gate's job, run on the reference machines on a schedule
(``doc/performance-testing.md``, "Precise throughput gate (local schedule)").
The power figures cover within-run variance only: the two runs' median ratios
differed by 0.17, which the bootstrap does not model.

The profile is chosen by the caller, never by the environment: the pytest option
``--dual-gate-profile`` (``test/performance/conftest.py``), passed explicitly
by the CI step that runs this gate. Each profile's numbers are defined once,
as the module constants below; ``profile_params`` reads them at call time, so
the local gate's threshold-override plugin (which rebinds
``T_DUAL_VS_SINGLE``) still reaches the precise profile.
"""

from __future__ import annotations

import statistics
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from test.e2e.benchmark_cli import BenchmarkResult, run_benchmark, write_infinite_variant

T_DUAL_VS_SINGLE = 0.85
T_LEGACY_SANITY = 2.0
N_REPS = 21
# CUDA reference-machine calibration: 14 idle same-commit runs, 70,000
# run-level resamples -> about 0.7% joint false red and 80% power at a 10.7%
# true ratio regression.  Metal keeps the independently calibrated 21 reps.
N_REPS_CUDA_PRECISE = 63
N_LEGACY_REPS = 3

CUDA_PRECISE_WINSOR_FRACTION = 0.10

T_DISASTER = 0.75
N_REPS_DISASTER = 5

PROFILES = ("precise", "ci")
DEFAULT_PROFILE = "precise"

_ARMS = ("dual", "single_a", "single_b")


def fixture_path(configs_dir: Path, arm: str) -> Path:
    return configs_dir / f"multi_renderer_throughput_{arm}.json"


def _assert_on_device(r: BenchmarkResult, expected_backend: str, arm: str, rep: int) -> None:
    assert r.backend == expected_backend and not r.fell_back, (
        f"{arm} rep {rep}: {expected_backend} was requested but the measured pass ran on "
        f"{r.backend!r} (fell_back={r.fell_back}); the throughput ratio is meaningless. "
        f"stderr tail: {r.stderr[-400:]!r}"
    )
    assert r.multi_basis == "drain_aligned", (
        f"{arm} rep {rep}: rate_basis={r.multi_basis!r}, expected 'drain_aligned' — the GPU arm "
        f"must run the infinite-ray_num variant so the rate is measured across whole drains."
    )


def profile_params(profile: str, backend_env: Optional[str] = None) -> Tuple[float, int]:
    """(dual-vs-single threshold, reps per arm), read at call time.

    The backend is an orthogonal precision dimension, not another profile:
    only CUDA precise needs more samples.  Reading the threshold dynamically
    preserves the local gate's test-only threshold override.
    """
    if profile == "precise":
        return T_DUAL_VS_SINGLE, N_REPS_CUDA_PRECISE if backend_env == "cuda" else N_REPS
    if profile == "ci":
        return T_DISASTER, N_REPS_DISASTER
    raise ValueError(f"unknown dual-renderer gate profile {profile!r}; expected one of {PROFILES}")


def ratio_verdict(medians: Dict[str, float], profile: str) -> Tuple[float, float, bool]:
    """(dual/single_a, dual/single_b, passes) for the three arms' medians under a profile."""
    threshold, _ = profile_params(profile)
    ratio_a = medians["dual"] / medians["single_a"]
    ratio_b = medians["dual"] / medians["single_b"]
    return ratio_a, ratio_b, ratio_a >= threshold and ratio_b >= threshold


def _winsorized_mean(values: List[float], fraction: float) -> float:
    """Mean after clamping each tail to its nearest retained observation."""
    ordered = sorted(values)
    trim_each_tail = int(len(ordered) * fraction)
    if trim_each_tail == 0:
        return statistics.mean(ordered)
    retained_low = ordered[trim_each_tail]
    retained_high = ordered[-trim_each_tail - 1]
    winsorized = (
        [retained_low] * trim_each_tail
        + ordered[trim_each_tail:-trim_each_tail]
        + [retained_high] * trim_each_tail
    )
    return statistics.mean(winsorized)


def sample_ratio_verdict(
    samples: Dict[str, List[float]], medians: Dict[str, float], backend_env: str, profile: str
) -> Tuple[float, float, bool, str]:
    """Return the backend/profile statistic and its human-readable name."""
    threshold, _ = profile_params(profile, backend_env)
    if backend_env == "cuda" and profile == "precise":
        ratio_a = _winsorized_mean(
            [dual / single for dual, single in zip(samples["dual"], samples["single_a"])],
            CUDA_PRECISE_WINSOR_FRACTION,
        )
        ratio_b = _winsorized_mean(
            [dual / single for dual, single in zip(samples["dual"], samples["single_b"])],
            CUDA_PRECISE_WINSOR_FRACTION,
        )
        statistic = (
            f"10% winsorized mean of {len(samples['dual'])} interleaved paired ratios"
        )
    else:
        ratio_a, ratio_b, _ = ratio_verdict(medians, profile)
        statistic = f"ratio of medians over {len(samples['dual'])} interleaved samples"
    return ratio_a, ratio_b, ratio_a >= threshold and ratio_b >= threshold, statistic


def run_dual_renderer_gate(
    configs_dir: Path,
    tmp_dir: Path,
    backend_env: str,
    timeout_sec: int,
    label: str,
    profile: str = DEFAULT_PROFILE,
) -> Dict[str, float]:
    """Sample the three GPU arms interleaved, ratio the medians, assert the gate.

    Returns the summary (medians, ratios) the caller may print or record.
    """
    threshold, n_reps = profile_params(profile, backend_env)
    infinite = {arm: write_infinite_variant(fixture_path(configs_dir, arm), tmp_dir) for arm in _ARMS}
    samples: Dict[str, List[float]] = {arm: [] for arm in _ARMS}
    for rep in range(n_reps):
        for arm in _ARMS:
            r = run_benchmark(infinite[arm], backend_env, timeout_sec)
            _assert_on_device(r, backend_env, arm, rep)
            samples[arm].append(r.multi_rps)
        print(
            f"[{label}] rep={rep + 1}/{n_reps} "
            f"dual_rps={samples['dual'][-1]:.6f} "
            f"single_a_rps={samples['single_a'][-1]:.6f} "
            f"single_b_rps={samples['single_b'][-1]:.6f}"
        )

    medians = {arm: statistics.median(v) for arm, v in samples.items()}
    covs = {arm: statistics.stdev(v) / statistics.mean(v) for arm, v in samples.items()}
    ratio_a, ratio_b, ratios_pass, statistic = sample_ratio_verdict(
        samples, medians, backend_env, profile
    )

    legacy_samples = []
    for rep in range(N_LEGACY_REPS):
        r = run_benchmark(fixture_path(configs_dir, "dual"), None, timeout_sec)
        assert r.backend == "cpu" and not r.fell_back, (
            f"legacy rep {rep}: expected the CPU route, got backend={r.backend!r}"
        )
        legacy_samples.append(r.multi_rps)
    legacy_median = statistics.median(legacy_samples)
    vs_legacy = medians["dual"] / legacy_median

    for arm in _ARMS:
        print(
            f"[{label}] {arm}: median={medians[arm] / 1e6:.2f}M rays/s CoV={covs[arm]:.3f} "
            f"min={min(samples[arm]) / 1e6:.2f}M max={max(samples[arm]) / 1e6:.2f}M n={n_reps}"
        )
    print(
        f"[{label}] dual/single_a={ratio_a:.3f} dual/single_b={ratio_b:.3f} (gate >= {threshold}, "
        f"profile={profile}, statistic={statistic}); "
        f"legacy dual median={legacy_median / 1e6:.2f}M rays/s, dual {backend_env}/legacy={vs_legacy:.2f}x "
        f"(sanity >= {T_LEGACY_SANITY})"
    )

    assert vs_legacy >= T_LEGACY_SANITY, (
        f"dual {backend_env} is only {vs_legacy:.2f}x legacy CPU (floor {T_LEGACY_SANITY}) — "
        f"the multi-renderer session is not delivering the GPU route's throughput at all."
    )
    assert ratios_pass, (
        f"dual-renderer throughput {ratio_a:.3f}x / {ratio_b:.3f}x of the single-renderer arms "
        f"({statistic}; {profile} profile, gate >= {threshold}). Serving the "
        f"second plane costs more than the design study's bound — check the exit tail's "
        f"per-renderer loop and the per-renderer landed-weight reduction."
    )
    return {
        "dual": medians["dual"],
        "single_a": medians["single_a"],
        "single_b": medians["single_b"],
        "ratio_a": ratio_a,
        "ratio_b": ratio_b,
        "legacy": legacy_median,
        "vs_legacy": vs_legacy,
    }
