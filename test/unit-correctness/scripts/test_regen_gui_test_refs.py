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
def test_capture_preserves_streams_text_and_exit(emitter, monkeypatch, entry, stream, rc):
    # Model a non-UTF-8 locale without relying on installed OS locales.
    monkeypatch.setattr(subprocess, "_text_encoding", lambda: "ascii")
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


def phase_args(tmp_path, **overrides):
    values = dict(binary="gui-tool", refs_dir=str(tmp_path), group="lens_proj", scene=None,
                  n=None, n_calib=2, quality=85)
    values.update(overrides)
    return argparse.Namespace(**values)


def calibration_output():
    return (
        b"[lens_proj] fisheye_equal_area_120: PSNR=42.5 dB\r\n"
        b"[lens_proj] fisheye_equal_area_120: n_diff=0 maxcc=0 dmax=0 (tau=0, K=0)\r\n"
        b"[other_group] ignored: PSNR=99 dB\n"
        b"[lens_proj] sky_colour_ea_180: PSNR=inf dB\n"
        b"[lens_proj] sky_colour_ea_180: n_diff=0 maxcc=0 dmax=0 (tau=0, K=0)\n"
    )


@pytest.mark.parametrize("stream", ["stdout", "stderr"])
@pytest.mark.parametrize("scene", [None, "fisheye_equal_area_120"])
def test_phase_b_real_capture_preserves_samples_and_unselected_data(emitter, tmp_path, stream, scene):
    install, calls = emitter
    payload = calibration_output()
    if stream == "stderr":
        payload = payload.replace(b"[other_group]", b"diagnostic\xff\n[other_group]")
    install(b"out-first\xffout-last" if stream == "stdout" else b"out", payload, rc=7)
    untouched_group = {"generated_at": "original", "scenes": {"other": {"threshold": 12}}}
    saved_scene = {"threshold": 23, "n_samples": 10}
    thresholds = tmp_path / "_thresholds.json"
    thresholds.write_text(json.dumps({"groups": {
        "other_group": untouched_group,
        "lens_proj": {"scenes": {"linear": saved_scene, "sky_colour_ea_180": saved_scene}},
    }}))
    image = tmp_path / "unrelated.png"
    image.write_bytes(b"untouched reference sentinel")

    regen.phase_b(phase_args(tmp_path, scene=scene))

    assert len(calls) == 2
    assert all(cmd == ["gui-tool"] + regen.SUITE_ARGS for cmd, _ in calls)
    result = json.loads(thresholds.read_text())["groups"]
    assert result["other_group"] == untouched_group
    group = result["lens_proj"]
    assert group["n_calib_runs"] == 2
    assert group["n_ref_runs"] == regen.STOCHASTIC_RUNS
    assert group["scenes"]["linear"] == saved_scene
    assert group["scenes"]["fisheye_equal_area_120"] == {
        "psnr_mean": 42.5, "psnr_std": 0.0, "identical_runs": 0, "n_samples": 2,
        "threshold": 41.5, "maxcc_tau": 0, "maxcc_local_max": 0, "maxcc_samples": 2,
    }
    if scene is None:
        assert group["scenes"]["sky_colour_ea_180"] == {
            "psnr_mean": None, "psnr_std": None, "identical_runs": 2, "n_samples": 2,
            "threshold": regen.DETERMINISTIC_FLOOR_DB,
            "maxcc_tau": 0, "maxcc_local_max": 0, "maxcc_samples": 2,
        }
    else:
        assert group["scenes"]["sky_colour_ea_180"] == saved_scene
    assert image.read_bytes() == b"untouched reference sentinel"
    assert {p.name for p in tmp_path.iterdir()} == {"emit.py", "_thresholds.json", "unrelated.png"}


def test_phase_b_no_parseable_samples_still_fails(emitter, tmp_path, capsys):
    install, _ = emitter
    # Valid-looking stdout is not a substitute for the stderr data contract.
    install(calibration_output(), b"noise\xffonly", rc=7)
    with pytest.raises(SystemExit) as exc:
        regen.phase_b(phase_args(tmp_path, n_calib=None, group="capture_harness"))
    assert exc.value.code == 1
    assert "no PSNR output parsed" in capsys.readouterr().err
    assert not (tmp_path / "_thresholds.json").exists()


def test_phase_b_file_error_and_image_error_still_propagate(emitter, tmp_path):
    install, _ = emitter
    install(b"noise\xff", calibration_output())
    with pytest.raises(FileNotFoundError):
        regen.phase_b(phase_args(tmp_path / "missing-refs", n_calib=1))
    corrupt = tmp_path / "corrupt.png"
    corrupt.write_bytes(b"not an image\xff")
    with pytest.raises(UnidentifiedImageError):
        regen._load_as_float32(str(corrupt))


def test_phase_a_nonzero_warns_but_missing_capture_still_fails(emitter, tmp_path, monkeypatch, capsys):
    install, _ = emitter
    install(b"", b"", rc=7)
    monkeypatch.setattr(regen, "STAGING_DIR", str(tmp_path / "staging"))
    with pytest.raises(SystemExit) as exc:
        regen.phase_a(phase_args(tmp_path, group="capture_harness"))
    assert exc.value.code == 1
    err = capsys.readouterr().err
    assert "exited 7" in err
    assert "no captured frame" in err
