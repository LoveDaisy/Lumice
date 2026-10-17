#!/usr/bin/env python3
"""Run the fixed-budget bench probe and record the runner's compute tier.

The e2e step's 10-minute budget is a workload budget, and the same workload
takes a different wall clock on each tier of the hosted-runner pool (measured
2026-10-17: 3-4 discrete CPU tiers, ~1.5x between slowest and fastest). A
shadow probe — a small fixed-`ray_num` `Lumice benchmark` run on the same
runner, just before the suite — records which tier a given run landed on, so
`(tier, suite seconds)` pairs can be collected across runs before any
machine-independent enforcement is switched on.

The probe config is derived from `examples/bench_config.json` (the same scene
the gh-pages benchmark history uses) with `scene.ray_num` overridden to a
fixed constant: one scene authority, one knob. The tier reading itself — the
`mode:single` pass — is engine-fixed at 2M rays / 1 worker
(`kBenchmarkSingleRays` in src/main.cpp), which makes it the same measurement
the gh-pages curve records; `--ray-num` sizes only the corroborating
`mode:multi` pass. The derived config is a temp file and never tracked.

Shadow mode (the only mode today): the script never fails the job. An
unusable probe — binary exits nonzero, output unparseable, a pass not on a
steady rate basis, wrong ISA — is recorded as `available: false` with the
reason and announced with a `::warning::`. The enforcement consumer decides
what an unavailable probe means (fail-closed there, not here).
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile

# [BENCHMARK] {"mode": "single", "workers": 1, "cores": 4, ...} — the emit site
# is `RunBenchmarkPass` in src/main.cpp; the gh-pages collector greps the same
# marker. The brace is claimed without requiring a closing one so a truncated
# emit fails loudly as unparseable instead of vanishing into "missing pass".
_BENCH_RE = re.compile(r"^\[BENCHMARK\]\s*(\{.*)")

# The only bases that measure a sustained trace rate (doc/testing/performance-testing.md,
# "Only trust rate_basis in {steady, drain_aligned}"); the fallback bases fold
# setup into the denominator and would read the tier too slow.
_STEADY_BASES = {"steady", "drain_aligned"}

# The e2e job builds the binary this probe runs at LUMICE_ISA_LEVEL=baseline;
# any other value means the probe is not measuring the build the budget was
# calibrated against.
_EXPECTED_ISA = "baseline"

_PROBE_VERSION = 1


def _line_is_usable(line: dict) -> str | None:
    """Return a reason string if the pass is not a usable tier reading, else None."""
    basis = line.get("rate_basis")
    if basis not in _STEADY_BASES:
        return f"rate_basis={basis!r} is not a steady trace rate"
    if line.get("isa") != _EXPECTED_ISA:
        return f"isa={line.get('isa')!r} is not {_EXPECTED_ISA!r}"
    if line.get("fell_back") is True:
        return "backend fell back"
    rate = line.get("rays_per_sec")
    if not isinstance(rate, (int, float)) or isinstance(rate, bool) or not math.isfinite(rate) or rate <= 0:
        return f"rays_per_sec={rate!r} is not a positive finite number"
    return None


def run_probe(binary: str, base_config: str, ray_num: int, timeout: float = 300.0) -> dict:
    # The benchmark runs with its own scratch cwd, so a caller-supplied
    # relative binary path must be resolved before that switch.
    binary = os.path.abspath(binary)
    with open(base_config, encoding="utf-8") as f:
        config = json.load(f)
    config["scene"]["ray_num"] = ray_num

    result: dict = {"probe_version": _PROBE_VERSION, "ray_num": ray_num, "available": False}

    with tempfile.TemporaryDirectory(prefix="lumice-bench-probe-") as tmp:
        config_path = os.path.join(tmp, "probe_config.json")
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f)
        try:
            proc = subprocess.run(
                [binary, "benchmark", "-f", config_path],
                capture_output=True, text=True, timeout=timeout, cwd=tmp,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            result["reason"] = f"benchmark run failed to execute: {error}"
            return result

    result["returncode"] = proc.returncode
    if proc.returncode != 0:
        result["reason"] = f"benchmark exited {proc.returncode}"
        result["stderr_tail"] = proc.stderr[-2000:]
        return result

    lines = []
    for stdout_line in proc.stdout.splitlines():
        match = _BENCH_RE.match(stdout_line)
        if match:
            try:
                lines.append(json.loads(match.group(1)))
            except json.JSONDecodeError:
                result["reason"] = f"unparseable [BENCHMARK] line: {stdout_line[:200]}"
                return result

    by_mode = {line.get("mode"): line for line in lines}
    missing = {"single", "multi"} - set(by_mode)
    if missing:
        result["reason"] = f"expected single+multi passes on the legacy CPU route, missing {sorted(missing)}"
        result["stderr_tail"] = proc.stderr[-2000:]
        return result

    for mode in ("single", "multi"):
        reason = _line_is_usable(by_mode[mode])
        if reason:
            result["reason"] = f"{mode} pass unusable: {reason}"
            return result

    result["available"] = True
    result["single"] = by_mode["single"]
    result["multi"] = by_mode["multi"]
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--binary", required=True, help="Lumice binary to probe")
    parser.add_argument("--base-config", required=True, help="bench scene the probe derives from")
    parser.add_argument("--ray-num", type=int, required=True, help="fixed probe ray budget")
    parser.add_argument("--output", required=True, help="probe.json destination path")
    args = parser.parse_args(argv)

    result = run_probe(args.binary, args.base_config, args.ray_num)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1, sort_keys=True)
        f.write("\n")

    if result["available"]:
        single = result["single"]["rays_per_sec"]
        print(
            f"::notice::bench probe: single={single:.0f} rays/s"
            f" (rate_basis={result['single']['rate_basis']}, isa={result['single']['isa']})"
        )
    else:
        print(f"::warning::bench probe unavailable: {result.get('reason', 'unknown reason')}")
    print(f"probe written to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
