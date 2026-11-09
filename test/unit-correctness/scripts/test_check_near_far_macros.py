"""Near/far macro gate: incident-shaped tokens, lexical exemptions and git scope."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))

import check_near_far_macros as gate  # noqa: E402
import check_new_refs  # noqa: E402
import check_policies  # noqa: E402


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    for module in (gate, check_new_refs, check_policies):
        monkeypatch.setattr(module, "REPO_ROOT", tmp_path)
    git(tmp_path, "init", "-q", "-b", "main")
    git(tmp_path, "config", "user.email", "t@example.com")
    git(tmp_path, "config", "user.name", "T")
    # Fixtures must not inherit an installed global hook or signing policy.
    git(tmp_path, "config", "core.hooksPath", str(tmp_path / "no-hooks"))
    git(tmp_path, "config", "commit.gpgsign", "false")
    (tmp_path / "seed.txt").write_text("seed\n", encoding="utf-8")
    git(tmp_path, "add", ".")
    git(tmp_path, "commit", "-qm", "seed")
    return tmp_path


def git(repo: Path, *args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=repo, capture_output=True, text=True, check=True
    ).stdout.strip()


def run_cli(monkeypatch: pytest.MonkeyPatch, *args: str) -> int:
    monkeypatch.setattr(sys, "argv", ["check_near_far_macros.py", *args])
    return gate.main()


@pytest.mark.parametrize("source", [
    "const ImVec2 near = {};",
    "MaskDir far = MaskDir{};",
    "void F(int near, int far);",
    "struct S { int near; };",
    "const auto near = [focal](double r) { return PsiGlobe(r, focal); };",
    "NumericOmega(near, rho_axis, h);",
    "enum E { near, far };",
    "void far();",
])
def test_identifiers_rejected_on_added_lines(repo: Path, source: str) -> None:
    (repo / "sample.cpp").write_text(f"// heading\n{source}\n", encoding="utf-8")
    git(repo, "add", ".")
    violations, _ = gate.diff_verdict(["--cached"], "")
    assert [(v.line, v.rule) for v in violations] == [(2, "near-far-macro")]
    assert "windows.h" in violations[0].render()
    assert "empty macros" in violations[0].message
    assert "near_pt / far_dir" in violations[0].message


@pytest.mark.parametrize("source", [
    'ctx->LogInfo("far=%zu near=%zu", far_count, near_count);',
    'Log("escaped \\\" near \\\" and // far");',
    "// near\n/* far\nnear */",
    "const char c = '\"'; const char d = '\\'';",
    "obj.near; ptr->far; Type::near;",
    "obj . /* comment */ near; ptr ->\n far; Type :: near;",
    "int near_pt, far_dir, far_wavelength, Near, Far;",
    "#undef near\n#define far\n#ifdef near\n#endif",
    "#define NAME \\\n near far\n",
    'const char* text = "near\\\nfar";',
    'int n = 100\'000; Log("so far");',
    'int n = 0xAB\'CD; Log("near");',
    'const char* doc = R"JSON({"caliber":"a class near-TIR weight"})JSON";',
    'const char* doc = u8R"tag(near " // far\nnear)tag";',
])
def test_exempt_regions_do_not_block_commits(repo: Path, source: str) -> None:
    (repo / "sample.cpp").write_text(source + "\n", encoding="utf-8")
    git(repo, "add", ".")
    assert gate.diff_verdict(["--cached"], "")[0] == []


@pytest.mark.parametrize("space", ["\f", "\v"])
def test_directive_exemption_uses_git_physical_lines(repo: Path, space: str) -> None:
    path = repo / "sample.cpp"
    path.write_text(f"int a{space}b;\n#undef near\nint far;\n", encoding="utf-8")
    git(repo, "add", ".")
    violations, _ = gate.diff_verdict(["--cached"], "")
    assert [v.line for v in violations] == [3]


@pytest.mark.parametrize("newline", ["\n", "\r\n"])
def test_directive_continuation_is_decided_before_comment_removal(
    repo: Path, newline: str
) -> None:
    text = newline.join(["#define X /* comment \\", "*/ near", "int far;", ""])
    path = repo / "sample.cpp"
    path.write_bytes(text.encode("utf-8"))
    git(repo, "add", ".")
    violations, _ = gate.diff_verdict(["--cached"], "")
    assert [v.line for v in violations] == [3]


@pytest.mark.parametrize("prefix", [
    "int n = 100'000;", "int n = 0xAB'CD;",
    'Log(R"tag(// " far\nnear)tag");',
    'Log(u8R"tag(// " far\nnear)tag");',
])
def test_lexical_regions_cannot_hide_following_identifiers(repo: Path, prefix: str) -> None:
    text = prefix + "\nint near;\n"
    assert [v.line for v in gate.find_violations_in_text("x.cpp", text)] == [
        text.count("\n")
    ]


def test_multiline_literals_and_comments_preserve_hit_line(repo: Path) -> None:
    text = '/* near\nfar */\nLog("near"); int far = 1; // near\n'
    assert [v.line for v in gate.find_violations_in_text("x.cu", text)] == [3]
    assert len(gate.identifier_code(text)) == len(text)


@pytest.mark.parametrize("suffix", [".cpp", ".hpp", ".inl", ".mm", ".cu", ".metal"])
def test_tree_scan_uses_tracked_c_family_files_only(repo: Path, suffix: str) -> None:
    (repo / f"bad{suffix}").write_text("int near;\n", encoding="utf-8")
    (repo / "untracked.cpp").write_text("int far;\n", encoding="utf-8")
    (repo / "data.json").write_text('{"near": "far"}\n', encoding="utf-8")
    git(repo, "add", f"bad{suffix}", "data.json")
    violations, _ = gate.tree_verdict(repo)
    assert [(v.path.name, v.line) for v in violations] == [(f"bad{suffix}", 1)]


@pytest.mark.parametrize("args", [(), ("--staged",)])
def test_cli_red_green_reads_index_not_unstaged_fix(
    repo: Path, monkeypatch: pytest.MonkeyPatch, args: tuple[str, ...]
) -> None:
    path = repo / "sample.cpp"
    path.write_text("const ImVec2 near = {};\n", encoding="utf-8")
    git(repo, "add", ".")
    path.write_text("const ImVec2 near_pt = {};\n", encoding="utf-8")
    assert run_cli(monkeypatch, *args) == 1
    git(repo, "add", ".")
    assert run_cli(monkeypatch, *args) == 0


def test_range_and_worktree_seams_select_the_right_content(
    repo: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    base = git(repo, "rev-parse", "HEAD")
    path = repo / "sample.cpp"
    path.write_text("int near;\n", encoding="utf-8")
    git(repo, "add", ".")
    git(repo, "commit", "-qm", "incident")
    path.write_text("int near_pt;\n", encoding="utf-8")
    assert run_cli(monkeypatch, "--range", f"{base}..HEAD") == 1
    assert gate.diff_verdict([base], None)[0] == []
    assert run_cli(monkeypatch, "--root") == 0


def test_unchanged_identifier_not_billed_and_pure_rename_not_added(repo: Path) -> None:
    path = repo / "old.cpp"
    path.write_text("int near;\nint x;\n", encoding="utf-8")
    git(repo, "add", ".")
    git(repo, "commit", "-qm", "old content")
    git(repo, "mv", "old.cpp", "new.cpp")
    assert gate.diff_verdict(["--cached"], "")[0] == []
    (repo / "new.cpp").write_text("int near;\nint y;\n", encoding="utf-8")
    git(repo, "add", ".")
    assert gate.diff_verdict(["--cached"], "")[0] == []


@pytest.mark.parametrize("args", [
    ("--range", "HEAD"), ("--range", "main...HEAD"), ("--range", ""),
    ("--range", "..HEAD"), ("--range", "main.."),
    ("--staged", "--root"), ("--staged", "--range", "main..HEAD"),
])
def test_invalid_cli_modes_fail_before_git(
    repo: Path, monkeypatch: pytest.MonkeyPatch, args: tuple[str, ...]
) -> None:
    with pytest.raises(SystemExit) as exc:
        run_cli(monkeypatch, *args)
    assert exc.value.code == 2
