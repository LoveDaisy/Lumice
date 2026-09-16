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
whose true ratio is 0.93. So each arm is sampled ``N_REPS`` times with the
three arms INTERLEAVED (dual, single_a, single_b, dual, ...) so that a thermal
or clock drift over the run lands on all three equally, and the gate compares
MEDIANS. At N_REPS = 21 the standard error of a median is ~1.25·CoV/√21 ≈
2.4% per arm on Metal, ~4% on CUDA; the ratio then sits ~2σ (Metal) above
0.85, and about 1σ on CUDA — the CUDA file says so in its own words rather
than moving the number.

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
"""

from __future__ import annotations

import statistics
from pathlib import Path
from typing import Dict, List

from test.e2e.benchmark_cli import BenchmarkResult, run_benchmark, write_infinite_variant

T_DUAL_VS_SINGLE = 0.85
T_LEGACY_SANITY = 2.0
N_REPS = 21
N_LEGACY_REPS = 3

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


def run_dual_renderer_gate(
    configs_dir: Path, tmp_dir: Path, backend_env: str, timeout_sec: int, label: str
) -> Dict[str, float]:
    """Sample the three GPU arms interleaved, ratio the medians, assert the gate.

    Returns the summary (medians, ratios) the caller may print or record.
    """
    infinite = {arm: write_infinite_variant(fixture_path(configs_dir, arm), tmp_dir) for arm in _ARMS}
    samples: Dict[str, List[float]] = {arm: [] for arm in _ARMS}
    for rep in range(N_REPS):
        for arm in _ARMS:
            r = run_benchmark(infinite[arm], backend_env, timeout_sec)
            _assert_on_device(r, backend_env, arm, rep)
            samples[arm].append(r.multi_rps)

    medians = {arm: statistics.median(v) for arm, v in samples.items()}
    covs = {arm: statistics.stdev(v) / statistics.mean(v) for arm, v in samples.items()}
    ratio_a = medians["dual"] / medians["single_a"]
    ratio_b = medians["dual"] / medians["single_b"]

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
            f"min={min(samples[arm]) / 1e6:.2f}M max={max(samples[arm]) / 1e6:.2f}M n={N_REPS}"
        )
    print(
        f"[{label}] dual/single_a={ratio_a:.3f} dual/single_b={ratio_b:.3f} (gate >= {T_DUAL_VS_SINGLE}); "
        f"legacy dual median={legacy_median / 1e6:.2f}M rays/s, dual {backend_env}/legacy={vs_legacy:.2f}x "
        f"(sanity >= {T_LEGACY_SANITY})"
    )

    assert vs_legacy >= T_LEGACY_SANITY, (
        f"dual {backend_env} is only {vs_legacy:.2f}x legacy CPU (floor {T_LEGACY_SANITY}) — "
        f"the multi-renderer session is not delivering the GPU route's throughput at all."
    )
    assert ratio_a >= T_DUAL_VS_SINGLE and ratio_b >= T_DUAL_VS_SINGLE, (
        f"dual-renderer throughput {ratio_a:.3f}x / {ratio_b:.3f}x of the single-renderer arms "
        f"(medians of {N_REPS} interleaved samples; gate >= {T_DUAL_VS_SINGLE}). Serving the "
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
