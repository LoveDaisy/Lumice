"""Real-pipe regression tests for the reference driver's external text boundary."""

import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import pytest
from PIL import UnidentifiedImageError


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("regen_gui_test_refs", ROOT / "scripts/regen_gui_test_refs.py")
regen = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = regen
SPEC.loader.exec_module(regen)


@pytest.fixture
def emitter(tmp_path, monkeypatch):
    """Replace only the command target, retaining real subprocess pipe/decoding behavior."""
    script = tmp_path / "emit.py"
    real_run = subprocess.run
    calls = []

    def install(stdout, stderr, rc=0):
        script.write_text(
            "import sys\n"
            f"sys.stdout.buffer.write({stdout!r})\n"
            f"sys.stderr.buffer.write({stderr!r})\n"
            f"sys.exit({rc})\n",
            encoding="utf-8",
        )
        command = [sys.executable, "-I", str(script)]

        def run(cmd, **kwargs):
            calls.append((cmd, kwargs))
            return real_run(command, **kwargs)

        monkeypatch.setattr(regen.subprocess, "run", run)
        return command, real_run

    return install, calls


@pytest.mark.parametrize("stream", ["stdout", "stderr"])
@pytest.mark.parametrize("rc", [0, 7])
def test_strict_capture_negative_control(emitter, stream, rc):
    install, _ = emitter
    bad = b"first\r\n\xff\nlast\r"
    outputs = {"stdout": b"valid out", "stderr": b"valid err", stream: bad}
    command, real_run = install(**outputs, rc=rc)
    raw = real_run(command, capture_output=True)
    assert raw.returncode == rc
    assert raw.stdout == outputs["stdout"]
    assert raw.stderr == outputs["stderr"]
    with pytest.raises(UnicodeDecodeError):
        real_run(command, capture_output=True, text=True, encoding="utf-8")


@pytest.mark.parametrize("entry", ["gui", "git"])
@pytest.mark.parametrize("stream", ["stdout", "stderr", None])
@pytest.mark.parametrize("rc", [0, 7])
def test_capture_preserves_streams_text_and_exit(emitter, entry, stream, rc):
    install, calls = emitter
    out = "out-first 中文\r\n".encode() + (b"\xff" if stream == "stdout" else b"") + b"out-last\r"
    err = "err-first 中文\r\n".encode() + (b"\xff" if stream == "stderr" else b"") + b"err-last\r"
    install(out, err, rc)
    if entry == "gui":
        actual_rc, actual_err = regen._run("gui-tool", ["--extra"], capture_stderr=True)
        assert calls[0][0] == ["gui-tool"] + regen.SUITE_ARGS + ["--extra"]
    else:
        actual_rc, actual_out, actual_err = regen._git(["diff", "--name-only"])
        assert calls[0][0] == ["git", "diff", "--name-only"]
        assert actual_out == out.decode("utf-8", "replace").replace("\r\n", "\n").replace("\r", "\n")
    assert actual_rc == rc
    assert actual_err == err.decode("utf-8", "replace").replace("\r\n", "\n").replace("\r", "\n")


@pytest.mark.parametrize("rc", [0, 7])
def test_gui_uncaptured_streams_are_inherited(emitter, capfdbinary, rc):
    install, calls = emitter
    install(b"inherited out\xff", b"inherited err\xfe", rc)
    assert regen._run("gui-tool", []) == (rc, "")
    captured = capfdbinary.readouterr()
    assert captured.out == b"inherited out\xff"
    assert captured.err == b"inherited err\xfe"
    assert calls[0][1]["capture_output"] is False


def test_launch_error_is_not_swallowed(tmp_path):
    with pytest.raises(FileNotFoundError):
        regen._run(str(tmp_path / "missing-tool"), [], capture_stderr=True)
