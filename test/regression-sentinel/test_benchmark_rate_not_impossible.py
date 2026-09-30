"""Regression guard: `Lumice benchmark` must never report a rate the run had no time for.

Fix: `RunBenchmarkPass` (`src/main.cpp`) — the `rate_basis=active_short` branch
used to compute `rays_per_sec = r_end / active_sec`.

Root cause (diagnosed white-box by tracing `(t, sim_ray_num, server_state)` at
every poll): on a GPU backend `sim_ray_num` advances in whole drain quanta
(`kDefaultXyzDrainBatches` * dispatch_size = 64 * 32768 = 2,097,152 rays with the
Metal default). A config whose entire `ray_num` is below one quantum reads 0 at
every poll and then publishes once, at the very end, so the run yields exactly ONE
observation and there is no window to measure. `active_short` is precisely the
branch that detects this (`r_end == rays_at_active_start`), yet it divided by
`active_sec` — which in that state is the gap between the poll that saw the
publish and the poll that saw IDLE, i.e. IDLE-detection latency. The published
rate was therefore `ray_num / (k * poll_interval)`, unbounded above as k -> 1:
measured 266-390 M rays/s against the same scene's true 13.9 M rays/s.

Why it needed a guard rather than just a fix: the error is one-directional
(always upward) and the only single-sample consumer, `test_metal_throughput.py`,
compares the Metal rate against a FLOOR. A noise source that can only turn a red
green produces no complaints, so nothing else in the tree would report it.

The invariant asserted here is deliberately basis-agnostic and physical rather
than a re-statement of the formula: **the rays the estimator claims per second,
multiplied by the whole run's wall clock, cannot exceed the rays the run actually
traced by more than a small factor.** Every honest basis lands near 1.0 (measured:
`wall_fallback` and `active_short` are exactly 1.0 by construction — the only two
bases `_HEAVY_CONFIGS` can reach, since their fixed ray_num stays below the drain
quantum; `steady` measured 0.99-1.0 during the diagnosis with `ray_num` temporarily
overridden to 20M/200M, a case THIS test's fixed configs never exercise), while the
defect scored ~20. `_MAX_WORK_RATIO` sits between them with room on both sides.

Scope, stated rather than implied: this is macOS + Metal only, because the defect
needs a drain quantum coarse enough to swallow a whole run — legacy CPU publishes
per batch, and at ray_num=200,000 it still reports `steady`, so it cannot reach
the branch. CUDA is untested here (no local device); its quantum differs.

What this file guards, and what it no longer does: the rate ladder itself —
including `active_short`, which a real run reaches only on a scheduling race
(measured at 4-8% of runs) — is decided by `EstimateBenchmarkRate`
(`src/util/benchmark_rate.hpp`) and covered deterministically by
`test/unit-correctness/util/test_benchmark_rate.cpp`, which states the degenerate
input as a literal instead of repeating whole benchmark runs until the race comes
up. What a unit test cannot see is the wiring: whether `RunBenchmarkPass` hands
the estimator the right observations (a wrong `t_active_start`, a swapped
`wall_sec`) and emits its answer into the JSON. That is what the few runs here
check, through the same basis-agnostic invariant on whichever basis each run
lands on. It does NOT claim to exercise `active_short` — with this few runs it
usually will not — and it must not be grown back into a branch-reaching sweep:
that coverage belongs to the unit test.

@pytest.mark.slow — needs the release binary and a Metal device; sits beside the
other perf-measurement gate (test/performance/) rather than in the fast leg.
"""

from __future__ import annotations

import json
import os
import platform
import subprocess

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root
from test.performance.test_metal_throughput import _HEAVY_CONFIGS

pytestmark = pytest.mark.skipif(
    platform.system() != "Darwin", reason="the coarse-drain-quantum path needs Metal (macOS)"
)

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"

# Below the 2,097,152-ray Metal drain quantum, the regime the defect lived in.
# Reuses the throughput gate's own `_HEAVY_CONFIGS` (rather than a second
# hardcoded list) so the two tests cannot silently drift apart on what they are
# measuring; one config is enough to check the wiring.
_CONFIG = _HEAVY_CONFIGS[0]

# Wiring, not branch reach: a few runs so a single odd sample does not stand
# alone. Branch coverage is the unit test's job (module docstring).
_RUNS = 3

# Every value the JSON may carry in `rate_basis` (RateBasisName in
# src/util/benchmark_rate.hpp); anything else means the wiring emitted garbage.
_RATE_BASES = {"drain_aligned", "too_few_drains", "steady", "active_short", "wall_fallback"}

# Honest bases score ~1.0; the defect scored ~20. See the module docstring.
_MAX_WORK_RATIO = 3.0

_TIMEOUT = 120


def _benchmark_rows(config_name: str) -> list[dict]:
    """One `Lumice benchmark` invocation on Metal; return its parsed [BENCHMARK] rows."""
    env = dict(os.environ)
    env["LUMICE_TRACE_BACKEND"] = "metal"
    proc = subprocess.run(
        [
            str(find_lumice_binary()),
            "benchmark",
            "-f",
            str(_CONFIGS_DIR / f"{config_name}.json"),
        ],
        capture_output=True,
        text=True,
        timeout=_TIMEOUT,
        env=env,
    )
    assert proc.returncode == 0, f"{config_name}: benchmark exited {proc.returncode}\n{proc.stderr}"
    return [
        json.loads(line.split("[BENCHMARK]", 1)[1].strip())
        for line in proc.stdout.splitlines()
        if "[BENCHMARK]" in line
    ]


@pytest.mark.slow
def test_reported_rate_is_physically_possible() -> None:
    config_name = _CONFIG
    seen_basis: dict[str, int] = {}
    worst = None  # (work_ratio, row) over every sample, reported either way.

    for _ in range(_RUNS):
        for row in _benchmark_rows(config_name):
            basis = str(row.get("rate_basis", "?"))
            seen_basis[basis] = seen_basis.get(basis, 0) + 1
            assert basis in _RATE_BASES, f"{config_name}: unknown rate_basis {basis!r} in {row}"
            wall_sec = float(row["wall_sec"])
            if wall_sec <= 0.0:
                continue  # rounded-away wall clock carries no bound; not a failure
            work_ratio = float(row["rays_per_sec"]) * wall_sec / float(row["rays"])
            if worst is None or work_ratio > worst[0]:
                worst = (work_ratio, row)
            # Reported inside the loop so the failing SAMPLE is in the message,
            # and so a red stops here instead of driving more runs against an
            # already-established failure.
            assert work_ratio <= _MAX_WORK_RATIO, (
                f"{config_name}: [BENCHMARK] claims {row['rays_per_sec']:,.0f} rays/s over a "
                f"{wall_sec}s run that traced only {row['rays']:,} rays — that is "
                f"{work_ratio:.1f}x more work than the run had time for "
                f"(limit {_MAX_WORK_RATIO}). rate_basis={basis}, active_sec={row.get('active_sec')}. "
                f"The estimator is dividing by something that is not a trace duration."
            )

    assert worst is not None, f"{config_name}: no usable [BENCHMARK] sample in {_RUNS} runs"
    # Which bases the wiring was checked on this time; `active_short` showing up
    # here is incidental, not required (module docstring).
    print(
        f"[rate-sanity] {config_name}: {_RUNS} runs, bases={seen_basis}, "
        f"worst work_ratio={worst[0]:.2f} (limit {_MAX_WORK_RATIO})"
    )
