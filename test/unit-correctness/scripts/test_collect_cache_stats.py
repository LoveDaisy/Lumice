"""Regression net for `scripts/collect_cache_stats.py`.

The collector is the measurement half of the per-miss build budget: whatever it
writes into `cache-stats.json` is the number the duration checker judges the
build phase by. Every rule below is pinned red-next-to-green: a trusted count
writes a report and exits zero; an untrusted count (tool failure, unknown
statistics shape, ccache internal error) exits non-zero and writes nothing,
because a missing report fails the consumer closed while a wrong number would
fail it open.

The sccache fixture is the shape sccache 0.17.0 actually emits (its
`cache_misses` is a per-language object whose total is `counts.values().sum()`,
`PerLanguageCount::all` in its server.rs — `adv_counts` keys the same events by
compiler kind and must not be summed in).
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import collect_cache_stats as ccs  # noqa: E402

# Same sample texts the seed audit's own tests pin: the collector reads the
# identical ccache --print-stats shape.
CCACHE_SAMPLE = "internal_error\t0\ncache_miss\t7\nlocal_storage_write\t13"
CCACHE_INTERNAL_ERROR = "internal_error\t2\ncache_miss\t0\nlocal_storage_write\t0"

SCCACHE_SAMPLE = json.dumps(
    {
        "stats": {
            "compile_requests": 3,
            "cache_hits": {"counts": {"C/C++": 0}, "adv_counts": {}},
            "cache_misses": {"counts": {"C/C++": 2}, "adv_counts": {"C/C++": 5}},
            "cache_write_errors": 0,
            "cache_writes": 2,
        },
        "version": "0.17.0",
    }
)


def run_collect(tmp_path: Path, tool: str = "ccache", report_name: str = "cache-stats.json") -> int:
    report = tmp_path / report_name
    return ccs.main(["--tool", tool, "--phase", "build", "--report", str(report)])


def read_report(tmp_path: Path, report_name: str = "cache-stats.json") -> dict:
    return json.loads((tmp_path / report_name).read_text(encoding="utf-8"))


def test_ccache_sample_writes_the_closed_three_field_report(tmp_path, monkeypatch):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=CCACHE_SAMPLE, stderr=""),
    )
    assert run_collect(tmp_path) == 0
    assert read_report(tmp_path) == {"phase": "build", "tool": "ccache", "misses": 7}


def test_ccache_internal_error_is_fatal_and_writes_nothing(tmp_path, monkeypatch):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=CCACHE_INTERNAL_ERROR, stderr=""),
    )
    assert run_collect(tmp_path) == 1
    assert not (tmp_path / "cache-stats.json").exists()


@pytest.mark.parametrize("raw", ["", "cache_miss 2", "internal_error\tnot-a-number"])
def test_ccache_unknown_shape_is_fatal(tmp_path, monkeypatch, raw):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=raw, stderr=""),
    )
    assert run_collect(tmp_path) == 1
    assert not (tmp_path / "cache-stats.json").exists()


def test_tool_failure_is_fatal(tmp_path, monkeypatch):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 2, stdout="", stderr="tool failed"),
    )
    assert run_collect(tmp_path) == 1
    assert not (tmp_path / "cache-stats.json").exists()


def test_sccache_counts_only_the_per_language_counts(tmp_path, monkeypatch):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=SCCACHE_SAMPLE, stderr=""),
    )
    assert run_collect(tmp_path, tool="sccache") == 0
    # counts sums to 2; adv_counts keys the same events by compiler kind and is
    # never part of the total.
    assert read_report(tmp_path) == {"phase": "build", "tool": "sccache", "misses": 2}


def test_sccache_write_errors_warn_but_do_not_fail(tmp_path, monkeypatch, capsys):
    payload = json.loads(SCCACHE_SAMPLE)
    payload["stats"]["cache_write_errors"] = 3
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=json.dumps(payload), stderr=""),
    )
    assert run_collect(tmp_path, tool="sccache") == 0
    assert "3 cache write error" in capsys.readouterr().err
    assert read_report(tmp_path)["misses"] == 2


def test_sccache_missing_or_malformed_miss_counts_are_fatal(tmp_path, monkeypatch):
    payload = json.loads(SCCACHE_SAMPLE)
    del payload["stats"]["cache_misses"]
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=json.dumps(payload), stderr=""),
    )
    assert run_collect(tmp_path) == 1

    payload["stats"]["cache_misses"] = {"counts": {"C/C++": "many"}, "adv_counts": {}}
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=json.dumps(payload), stderr=""),
    )
    assert run_collect(tmp_path) == 1
    assert not (tmp_path / "cache-stats.json").exists()


def test_existing_report_is_not_overwritten(tmp_path, monkeypatch):
    report = tmp_path / "cache-stats.json"
    report.write_text("sentinel", encoding="utf-8")
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=CCACHE_SAMPLE, stderr=""),
    )
    assert run_collect(tmp_path) == 1
    assert report.read_text(encoding="utf-8") == "sentinel"


def test_report_lands_in_a_created_directory(tmp_path, monkeypatch):
    monkeypatch.setattr(
        subprocess, "run",
        lambda *a, **k: subprocess.CompletedProcess(a[0], 0, stdout=CCACHE_SAMPLE, stderr=""),
    )
    assert ccs.main(
        ["--tool", "ccache", "--phase", "build",
         "--report", str(tmp_path / "ci-duration" / "cache-stats.json")]
    ) == 0
    assert (tmp_path / "ci-duration" / "cache-stats.json").exists()


def test_empty_phase_name_is_rejected(tmp_path):
    with pytest.raises(SystemExit):
        ccs.main(["--tool", "ccache", "--phase", "  ", "--report", str(tmp_path / "r.json")])
