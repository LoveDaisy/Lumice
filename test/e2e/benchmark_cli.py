"""Drive `Lumice benchmark` from a test and hand back its parsed `[BENCHMARK]` line.

Shared by the throughput gates under ``test/performance/``. One place owns the
subprocess shape, the env handling and the parse, so a change to what the CLI
prints (or to how a backend is selected) lands in one file rather than in each
gate's private copy.

What comes back is the CLI's own account of the run. In particular ``backend``
and ``fell_back`` are the `[BENCHMARK]` JSON's fields — LUMICE_GetActiveBackend
/ LUMICE_GetBackendFallbackFlag read by ``main.cpp`` after the measured pass —
not a regex over the log stream: a gate that asserts "Metal really ran" asserts
what the CLI tells its user, and a log-wording change cannot silently disarm
it.
"""

from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Optional

from test.e2e.runner import find_lumice_binary


@dataclass
class BenchmarkResult:
    """One `Lumice benchmark` invocation, keyed by pass label ("single" / "multi")."""

    passes: Dict[str, dict] = field(default_factory=dict)
    stdout: str = ""
    stderr: str = ""
    returncode: int = 0

    @property
    def multi(self) -> dict:
        """The measured pass every route emits (the GPU route emits only this one)."""
        if "multi" not in self.passes:
            raise RuntimeError(
                f"no [BENCHMARK] multi pass (rc={self.returncode}); "
                f"stdout tail: {self.stdout[-600:]!r} stderr tail: {self.stderr[-600:]!r}"
            )
        return self.passes["multi"]

    @property
    def multi_rps(self) -> float:
        return float(self.multi["rays_per_sec"])

    @property
    def multi_basis(self) -> str:
        return str(self.multi.get("rate_basis", "?"))

    @property
    def backend(self) -> str:
        """`cpu` / `metal` / `cuda` — what the measured pass ran on."""
        return str(self.multi["backend"])

    @property
    def fell_back(self) -> bool:
        return bool(self.multi["fell_back"])


def run_benchmark(
    config_path: Path,
    backend_env: Optional[str],
    timeout_sec: int = 240,
) -> BenchmarkResult:
    """Run `Lumice benchmark -f config_path` and parse every `[BENCHMARK]` line.

    `backend_env` is the LUMICE_TRACE_BACKEND value to run under (``"metal"``,
    ``"cuda"``), or None for the legacy CPU route (the variable is removed from
    the child's environment — legacy is "env unset", NOT ``cpu_backend``, see
    doc/testing-architecture.md §4.1). The env override is used rather than
    ``--backend`` so this matches ``scripts/bench_throughput.py``'s selection.
    """
    env = dict(os.environ)
    if backend_env is None:
        env.pop("LUMICE_TRACE_BACKEND", None)
    else:
        env["LUMICE_TRACE_BACKEND"] = backend_env
    proc = subprocess.run(
        [str(find_lumice_binary()), "benchmark", "-f", str(config_path)],
        capture_output=True, encoding="utf-8", errors="replace", timeout=timeout_sec, env=env,
    )
    result = BenchmarkResult(stdout=proc.stdout, stderr=proc.stderr, returncode=proc.returncode)
    for line in proc.stdout.splitlines():
        if "[BENCHMARK]" in line:
            data = json.loads(line.split("[BENCHMARK]", 1)[1].strip())
            result.passes[str(data.get("mode"))] = data
    return result


def write_infinite_variant(config_path: Path, out_dir: Path) -> Path:
    """Copy `config_path` with ``scene.ray_num = "infinite"`` into `out_dir`.

    The GPU route's honest rate is the drain-aligned one: a finite ray_num is
    drain-quantized (one drain = 64 dispatches), so a run that observes only a
    few drains lumps most of its tracing into "setup" and a short active window
    reads a noisy rate. Under ``"infinite"`` `main.cpp` measures across N whole
    drains and reports ``rate_basis == "drain_aligned"`` (the same variant
    ``scripts/bench_throughput.py`` builds for its GPU cells). Only ray_num is
    touched, so the scene is the committed fixture's.
    """
    with open(config_path, encoding="utf-8") as f:
        cfg = json.load(f)
    cfg["scene"]["ray_num"] = "infinite"
    out = out_dir / f"{config_path.stem}.infinite.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(cfg, f)
    return out
