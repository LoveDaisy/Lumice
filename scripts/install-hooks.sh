#!/bin/sh
#
# Install Lumice's local git hooks into THIS REPOSITORY's hooks directory:
# `<git-common-dir>/hooks` (not assumed to be .git/hooks — in a git worktree,
# .git is a file, not a directory, and hooks live in the main checkout's
# .git/hooks, shared across all worktrees). Deliberately NOT
# `git rev-parse --git-path hooks`: that honours `core.hooksPath`, and with a
# *global* hooksPath (a per-user hook chain) it names a directory outside the
# repository — the previous version of this script overwrote the user's global
# `pre-commit` there with a symlink that dangled. A repo hook belongs in the
# repo; if a global hooksPath is set, git only runs the repo hook when the
# global one chains back to `<git-common-dir>/hooks`, or when the repo sets
# `core.hooksPath` locally to its own `.git/hooks` — this script says so and
# leaves that choice to the user rather than editing config.
# Installs a fixed pre-commit delegator: each invocation runs the committing
# checkout's scripts/hooks/pre-commit (guard + policy + format), not the copy in
# the checkout that installed it. A checkout without that script loudly skips.
# The existing git-lfs hooks (post-checkout / post-commit / post-merge / pre-push)
# are left untouched — we only add pre-commit, which git-lfs does not use.
#
# Run after cloning, or rerun THIS installer to migrate old symlink/copy entries:
#   ./scripts/install-hooks.sh
# Merging does not reinstall; running an older installer restores its mechanism.
# Bypass the hook for one commit:  git commit --no-verify

set -e
REPO_ROOT=$(git rev-parse --show-toplevel)
HOOK_DIR="$(git -C "$REPO_ROOT" rev-parse --path-format=absolute --git-common-dir)/hooks"
HOOK_DST="$HOOK_DIR/pre-commit"
mkdir -p "$HOOK_DIR"

GLOBAL_HOOKS_PATH=$(git config --global --get core.hooksPath || true)
LOCAL_HOOKS_PATH=$(git -C "$REPO_ROOT" config --local --get core.hooksPath || true)
if [ -n "$LOCAL_HOOKS_PATH" ]; then
  if [ "$(cd "$REPO_ROOT" && cd "$LOCAL_HOOKS_PATH" 2>/dev/null && pwd -P)" != "$(cd "$HOOK_DIR" && pwd -P)" ]; then
    echo "note: this repo sets core.hooksPath=$LOCAL_HOOKS_PATH, which is not $HOOK_DIR;" >&2
    echo "      git will not run the hook installed below unless that directory chains back to it." >&2
  fi
elif [ -n "$GLOBAL_HOOKS_PATH" ]; then
  echo "note: a global core.hooksPath is set ($GLOBAL_HOOKS_PATH); git will run the hook installed" >&2
  echo "      below only if that directory chains back to $HOOK_DIR, or if you point this" >&2
  echo "      repo at its own hooks:  git config core.hooksPath \"$HOOK_DIR\"" >&2
fi

# Replace the directory entry, never write through an old symlink into its
# checkout. Stage beside the destination so the rename leaves no partial hook.
HOOK_TMP=$(mktemp "$HOOK_DIR/pre-commit.XXXXXX")
trap 'rm -f "$HOOK_TMP"' EXIT
trap 'exit 1' HUP INT TERM
cat > "$HOOK_TMP" <<'EOF'
#!/bin/sh
REPO_ROOT=$(git rev-parse --show-toplevel) || exit 1
if [ ! -f "$REPO_ROOT/scripts/hooks/pre-commit" ]; then
  echo "pre-commit: scripts/hooks/pre-commit not in this checkout; checks skipped." >&2
  exit 0
fi
exec sh "$REPO_ROOT/scripts/hooks/pre-commit" "$@"
EOF
chmod +x "$HOOK_TMP"
mv -f "$HOOK_TMP" "$HOOK_DST"

echo "Installed fixed pre-commit delegator in $HOOK_DST"
echo "Done. Each commit runs its checkout's scripts/hooks/pre-commit and checkers."
echo "A checkout without that script reports that checks were skipped."

# The guard's Claude Code side lives in a git-ignored settings file that no diff
# can show is current; ask the script, which owns the canonical content. Its
# exit code is reported, not enforced: a stale or absent file is something to
# fix by hand (see the script's docstring), not a reason to undo the git hook.
if command -v python3 >/dev/null 2>&1; then
  (cd "$REPO_ROOT" && python3 scripts/hooks/worktree-guard.py check-settings) || \
    echo "(Claude Code PreToolUse line not current — see the message above.)" >&2
fi
