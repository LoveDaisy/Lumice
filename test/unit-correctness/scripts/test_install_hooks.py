"""Exercise hook installation and checkout delegation in isolated real Git repos."""
from __future__ import annotations

import os
import shutil
import stat
import subprocess
from pathlib import Path

import pytest


INSTALLER = Path(__file__).resolve().parents[3] / "scripts/install-hooks.sh"


def run(cwd: Path, env: dict[str, str], *args: str, check: bool = True):
    return subprocess.run(
        args, cwd=cwd, env=env, capture_output=True, text=True,
        encoding="utf-8", errors="replace", check=check,
    )


def git(cwd: Path, env: dict[str, str], *args: str) -> str:
    return run(cwd, env, "git", *args).stdout.strip()


def write_hook(repo: Path, identity: str, code: int = 0) -> Path:
    hook = repo / "scripts/hooks/pre-commit"
    hook.parent.mkdir(parents=True, exist_ok=True)
    hook.write_text(
        f'#!/bin/sh\nprintf \'%s\\n\' "{identity}" "$@" >&2\nexit {code}\n',
        encoding="utf-8",
    )
    # The delegator invokes sh; installation must not chmod tracked sources.
    hook.chmod(0o644)
    return hook


@pytest.fixture
def sandbox(tmp_path: Path):
    # No inherited Git location/config/index injection, real user hooks, templates,
    # signing policy, or system/global config may escape this temporary boundary.
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    home = tmp_path / "home"
    home.mkdir()
    env.update(
        HOME=str(home), USERPROFILE=str(home), XDG_CONFIG_HOME=str(home),
        GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=str(home / "gitconfig"),
        GIT_TEMPLATE_DIR=str(home / "empty-templates"),
        GIT_AUTHOR_NAME="Test", GIT_AUTHOR_EMAIL="test@example.invalid",
        GIT_COMMITTER_NAME="Test", GIT_COMMITTER_EMAIL="test@example.invalid",
        GIT_TERMINAL_PROMPT="0",
    )
    Path(env["GIT_TEMPLATE_DIR"]).mkdir()
    repo = tmp_path / "main checkout"
    repo.mkdir()
    git(repo, env, "init", "-q", "-b", "main")
    git(repo, env, "config", "commit.gpgsign", "false")
    (repo / "scripts").mkdir()
    shutil.copyfile(INSTALLER, repo / "scripts/install-hooks.sh")
    write_hook(repo, "MAIN")
    # Isolate the installer's existing optional settings report from user files.
    (repo / "scripts/hooks/worktree-guard.py").write_text(
        'import sys\nassert sys.argv[1:] == ["check-settings"]\n', encoding="utf-8",
    )
    git(repo, env, "add", ".")
    git(repo, env, "commit", "-qm", "seed")
    linked = tmp_path / "linked checkout"
    git(repo, env, "worktree", "add", "-qb", "linked", str(linked))
    hook_dir = Path(git(repo, env, "rev-parse", "--path-format=absolute", "--git-common-dir")) / "hooks"
    assert hook_dir.is_relative_to(tmp_path)
    hook_dir.mkdir(exist_ok=True)
    return repo, linked, env, hook_dir


def install(repo: Path, env: dict[str, str], hook_dir: Path):
    # Guard the write boundary before invoking the production installer.
    common = Path(git(repo, env, "rev-parse", "--path-format=absolute", "--git-common-dir"))
    assert common / "hooks" == hook_dir
    result = run(repo, env, "sh", "scripts/install-hooks.sh")
    assert (hook_dir / "pre-commit").exists(), result
    return result


def snapshot(path: Path):
    info = path.lstat()
    return path.read_bytes(), stat.S_IMODE(info.st_mode), info.st_ino, (
        os.readlink(path) if path.is_symlink() else None
    )


@pytest.mark.parametrize("old_kind", ["symlink", "copy"])
@pytest.mark.parametrize("install_from", ["main", "linked"])
def test_migration_is_idempotent_and_preserves_sources_and_lfs(
    sandbox, old_kind: str, install_from: str,
) -> None:
    repo, linked, env, hook_dir = sandbox
    target = repo / "scripts/hooks/pre-commit"
    original = {path: snapshot(path) for path in (target, linked / "scripts/hooks/pre-commit")}
    entry = hook_dir / "pre-commit"
    if old_kind == "symlink":
        entry.symlink_to("../../scripts/hooks/pre-commit")
    else:
        shutil.copyfile(target, entry)
        entry.chmod(0o755)
    # Include a linked sentinel: preserving bytes alone would miss replacement.
    for name in ("post-checkout", "post-commit", "post-merge", "pre-push"):
        sentinel = hook_dir / name
        if name == "pre-push":
            sentinel.symlink_to(target)
        else:
            sentinel.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            sentinel.chmod(0o751)
    sentinels = {p: snapshot(p) for p in hook_dir.iterdir() if p != entry}
    checkout = repo if install_from == "main" else linked
    install(checkout, env, hook_dir)
    assert not entry.is_symlink(), "installed entry must be a fixed regular delegator"
    assert entry.is_file() and os.access(entry, os.X_OK)
    content = entry.read_bytes()
    assert str(repo).encode() not in content and str(linked).encode() not in content
    run(checkout, env, "sh", "-n", str(entry))
    install(checkout, env, hook_dir)
    assert not entry.is_symlink() and entry.read_bytes() == content
    assert set(hook_dir.iterdir()) == {entry, *sentinels}
    assert {path: snapshot(path) for path in original} == original
    assert {path: snapshot(path) for path in sentinels} == sentinels


def test_runtime_routes_each_checkout_and_propagates_arguments_and_failure(sandbox) -> None:
    repo, linked, env, hook_dir = sandbox
    install(repo, env, hook_dir)
    entry = hook_dir / "pre-commit"
    write_hook(linked, "LINKED", 23)
    nested = linked / "sub directory"
    nested.mkdir()
    rejected = run(nested, env, "sh", str(entry), "one argument", "", "*", check=False)
    assert rejected.returncode == 23
    assert rejected.stderr == "LINKED\none argument\n\n*\n"
    accepted = run(repo, env, "sh", str(entry))
    assert accepted.stderr == "MAIN\n"

    # Real commits prove Git reaches the shared entry, not merely a direct sh call.
    before = git(linked, env, "rev-parse", "HEAD")
    rejected = run(linked, env, "git", "commit", "--allow-empty", "-qm", "reject", check=False)
    assert rejected.returncode != 0 and "LINKED" in rejected.stderr
    assert git(linked, env, "rev-parse", "HEAD") == before
    accepted = run(repo, env, "git", "commit", "--allow-empty", "-qm", "accept")
    assert "MAIN" in accepted.stderr
    assert git(repo, env, "rev-parse", "HEAD") != before


def test_missing_orchestrator_is_loud_success_not_a_clean_check(sandbox) -> None:
    repo, linked, env, hook_dir = sandbox
    install(repo, env, hook_dir)
    (linked / "scripts/hooks/pre-commit").unlink()
    result = run(linked, env, "sh", str(hook_dir / "pre-commit"))
    assert "scripts/hooks/pre-commit" in result.stderr
    assert "not in this checkout" in result.stderr and "skipped" in result.stderr
    assert not result.stdout
    assert "passed" not in result.stderr.lower() and "clean" not in result.stderr.lower()


@pytest.mark.parametrize("scope", ["global", "local", "own-hooks"])
def test_hookspath_warning_does_not_mutate_external_hooks_or_config(sandbox, scope: str) -> None:
    repo, linked, env, hook_dir = sandbox
    external = repo.parent / "external hooks"
    external.mkdir()
    external_entry = external / "pre-commit"
    external_entry.write_text("#!/bin/sh\nexit 17\n", encoding="utf-8")
    external_entry.chmod(0o751)
    original = snapshot(external_entry)
    config_scope = "--global" if scope == "global" else "--local"
    configured_path = str(hook_dir if scope == "own-hooks" else external)
    git(repo, env, "config", config_scope, "core.hooksPath", configured_path)
    global_config = Path(env["GIT_CONFIG_GLOBAL"])
    local_config = hook_dir.parent / "config"
    configs = {p: p.read_bytes() if p.exists() else None for p in (global_config, local_config)}
    result = install(linked, env, hook_dir)
    assert ("core.hooksPath" in result.stderr) == (scope != "own-hooks")
    assert snapshot(external_entry) == original
    assert {p: p.read_bytes() if p.exists() else None for p in configs} == configs
    assert git(repo, env, "config", config_scope, "--get", "core.hooksPath") == configured_path
