"""Regression guard: sentinel-overflow in the raw-XYZ result read.

Fix commit: 5287efe (fix(capi-sentinel-overflow): guard sentinel write past max_count)

Root cause: the C API bridge (then src/server/c_api.cpp) wrote a sentinel at
out[count] when count == max_count, overflowing the caller's buffer by one LUMICE_RawXyzResult (72 bytes). With
3 distinct configs in rotation, the heap layout evolved until the overflow hit
a live allocation and the process died with SIGSEGV. The fix's investigation
saw that at approximately lifecycle 31.

Trigger conditions:
- max_count == 1 (caller passes a size-1 array, the common case via capi_runner)
- >= 3 distinct configs alternated across lifecycles (single-config loops survived
  because the heap layout remained stable across repeated same-config lifecycles)

The trigger is the lifecycle count and the config rotation, not the work done
inside one lifecycle, so each config runs at 100k rays instead of its own budget.
Re-measured on the pre-fix tree (5287efe^ with its own capi_runner, macOS arm64,
2026-09-30): the process
SIGSEGVs during lifecycle 2 at the configs' original 5M rays and at 100k alike,
3 runs out of 3 each. (A 10k run crashing during lifecycle 1 comes from the
explore inventory, not from this re-measurement.) So the smaller budget does
not delay the crash on that machine. Whether ~31 still describes other platforms
was not re-measured. 36 lifecycles are kept regardless: they cover the
originally reported point and cost ~10 s at this budget.

Requires shared-lib build: ./scripts/build.sh -sj release
Run: pytest -v -m slow
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from test.e2e.capi_runner import run_scene_capi
from test.e2e.runner import get_project_root

# TODO: relocate configs when follow-up task completes
_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"

_SRC_CONFIGS = [
    str(_CONFIGS_DIR / "raypath_symmetry_4_6.json"),
    str(_CONFIGS_DIR / "raypath_symmetry_4_6_nofilter.json"),
    str(_CONFIGS_DIR / "raypath_symmetry_7_3.json"),
]

# Rays per lifecycle, overriding each config's own budget (why: module docstring).
_RAY_NUM = 100_000


def _shrunk(paths, out_dir):
    out = []
    for src in paths:
        with open(src, encoding="utf-8") as fp:
            doc = json.load(fp)
        doc["scene"]["ray_num"] = _RAY_NUM
        dst = out_dir / Path(src).name
        dst.write_text(json.dumps(doc), encoding="utf-8")
        out.append(str(dst))
    return out


# 12 rounds × 3 configs = 36 lifecycles, past the ~31 the original investigation saw
_REGRESSION_ROUNDS = 12


@pytest.mark.slow
def test_capi_sentinel_overflow_regression(tmp_path):
    """3-config rotation across 36 server lifecycles must not crash or produce invalid data.

    Each lifecycle calls CreateServer / CommitConfigFromFile / poll-until-IDLE /
    DestroyServer with a size-1 LUMICE_RawXyzResult array (max_count=1). Before
    fix 5287efe the sentinel write at out[max_count] overflowed this array and
    the process crashed within these 36 lifecycles (see the module docstring for
    where, and on what).
    """
    lifecycle_configs = _shrunk(_SRC_CONFIGS, tmp_path)
    n_configs = len(lifecycle_configs)
    total = _REGRESSION_ROUNDS * n_configs
    completed = 0
    for _ in range(_REGRESSION_ROUNDS):
        for cfg in lifecycle_configs:
            result = run_scene_capi(cfg)
            completed += 1
            assert result.has_valid_data, (
                f"lifecycle {completed}/{total}: no valid data ({cfg})"
            )
    assert completed == total
