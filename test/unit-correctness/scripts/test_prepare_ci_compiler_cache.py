from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import prepare_ci_compiler_cache as cache_ready  # noqa: E402


def test_ccache_stats_require_zero_errors_and_two_writes_per_miss():
    assert cache_ready._parse_ccache_stats("internal_error\t0\ncache_miss\t6\nlocal_storage_write\t12") == {
        "internal_error": 0,
        "cache_miss": 6,
        "local_storage_write": 12,
    }
    with pytest.raises(cache_ready.CacheReadinessError, match="2 internal error"):
        cache_ready._parse_ccache_stats("internal_error\t2\ncache_miss\t0\nlocal_storage_write\t0")
    with pytest.raises(cache_ready.CacheReadinessError, match="2 miss.*only 3 local write"):
        cache_ready._parse_ccache_stats("internal_error\t0\ncache_miss\t2\nlocal_storage_write\t3")


@pytest.mark.parametrize("raw", ["", "cache_miss 2", "internal_error\tnot-a-number", "internal_error\t0\ninternal_error\t0"])
def test_ccache_rejects_unknown_statistics_shapes(raw):
    with pytest.raises(cache_ready.CacheReadinessError):
        cache_ready._parse_ccache_stats(raw)


def test_sccache_stats_require_zero_cache_write_errors():
    payload = {"stats": {"cache_write_errors": 0, "cache_writes": 12}, "version": "0.17.0"}
    assert cache_ready._parse_sccache_stats(json.dumps(payload)) == payload
    with pytest.raises(cache_ready.CacheReadinessError, match="3 cache write error"):
        cache_ready._parse_sccache_stats(json.dumps({"stats": {"cache_write_errors": 3}}))


@pytest.mark.parametrize("payload", ["not json", "{}", '{"stats": {}}', '{"stats": {"cache_write_errors": false}}'])
def test_sccache_rejects_unknown_statistics_shapes(payload):
    with pytest.raises(cache_ready.CacheReadinessError):
        cache_ready._parse_sccache_stats(payload)


def test_tool_failure_is_reported(monkeypatch):
    monkeypatch.setattr(
        subprocess,
        "run",
        lambda *args, **kwargs: subprocess.CompletedProcess(args[0], 2, stdout="", stderr="tool failed"),
    )
    with pytest.raises(cache_ready.CacheReadinessError, match="exited with 2: tool failed"):
        cache_ready.collect_stats("ccache")


def test_missing_or_metadata_only_cache_is_not_ready(tmp_path, monkeypatch):
    monkeypatch.setattr(cache_ready, "collect_stats", lambda tool: ("ccache 4.9.1", {"internal_error": 0}))
    report = tmp_path / "missing.json"
    assert cache_ready.main(["--tool", "ccache", "--cache-dir", str(tmp_path / "missing"), "--report", str(report)]) == 1
    assert json.loads(report.read_text(encoding="utf-8"))["ready"] is False

    cache_dir = tmp_path / "cache"
    cache_dir.mkdir()
    (cache_dir / "stats").write_text("counter data", encoding="utf-8")
    report = tmp_path / "metadata.json"
    assert cache_ready.main(["--tool", "ccache", "--cache-dir", str(cache_dir), "--report", str(report)]) == 1
    assert "no stored compiler entries" in json.loads(report.read_text(encoding="utf-8"))["error"]


def test_real_entry_produces_auditable_ready_report(tmp_path, monkeypatch):
    stats = {"stats": {"cache_write_errors": 0, "cache_writes": 4}}
    monkeypatch.setattr(cache_ready, "collect_stats", lambda tool: ("sccache 0.17.0", stats))
    cache_dir = tmp_path / "cache"
    entry = cache_dir / "a" / "bc" / "entry"
    entry.parent.mkdir(parents=True)
    entry.write_bytes(b"compiler object")
    report = tmp_path / "ready.json"
    assert cache_ready.main(
        ["--tool", "sccache", "--cache-dir", str(cache_dir), "--report", str(report)]
    ) == 0
    payload = json.loads(report.read_text(encoding="utf-8"))
    assert payload == {
        "cache_dir": str(cache_dir),
        "entry_bytes": len(b"compiler object"),
        "entry_files": 1,
        "error": None,
        "ready": True,
        "stats": stats,
        "tool": "sccache",
        "version": "sccache 0.17.0",
    }


def test_existing_report_is_not_overwritten(tmp_path):
    report = tmp_path / "report.json"
    report.write_text("sentinel", encoding="utf-8")
    assert cache_ready.main(
        ["--tool", "ccache", "--cache-dir", str(tmp_path), "--report", str(report)]
    ) == 1
    assert report.read_text(encoding="utf-8") == "sentinel"
