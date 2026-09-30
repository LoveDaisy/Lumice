#!/usr/bin/env bash
# Install (or remove) the launchd agent that runs scripts/local_throughput_gate.py
# on a daily schedule on this Mac — the Metal reference machine role.
# What the gate is and how to read its results: doc/performance-testing.md,
# "Precise throughput gate (local schedule)".
#
# Usage:
#   scripts/install_throughput_gate.sh --python /path/to/python3 [--remote home-wsl] [--hour 3 --minute 30]
#   scripts/install_throughput_gate.sh --uninstall
#
# The agent runs a COPY of the gate script taken at install time, so deleting the
# checkout you installed from does not break it; re-run this script to update it.
# --python is required and must be an interpreter that has pytest: launchd has no
# login shell, so "whatever python3 is first on PATH" is not a thing it can know.
set -euo pipefail

LABEL="com.lumice.throughput-gate"
HERE="$(cd "$(dirname "$0")" && pwd)"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
SHARE_DIR="$HOME/.local/share/lumice-throughput-gate"
STATE_DIR="$HOME/.local/state/lumice-throughput-gate"
DOMAIN="gui/$(id -u)"

PYTHON=""
REMOTE="home-wsl"
HOUR=3
MINUTE=30
UNINSTALL=0
while [ $# -gt 0 ]; do
  case "$1" in
    --python) PYTHON="$2"; shift 2 ;;
    --remote) REMOTE="$2"; shift 2 ;;
    --hour) HOUR="$2"; shift 2 ;;
    --minute) MINUTE="$2"; shift 2 ;;
    --uninstall) UNINSTALL=1; shift ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [ "$UNINSTALL" = 1 ]; then
  launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
  rm -f "$PLIST"
  echo "removed $LABEL (results kept in $STATE_DIR)"
  exit 0
fi

if [ -z "$PYTHON" ]; then
  echo "--python is required (an interpreter with pytest installed)" >&2
  exit 2
fi
if ! "$PYTHON" -m pytest --version >/dev/null 2>&1; then
  echo "$PYTHON cannot run pytest; pick an interpreter that can" >&2
  exit 2
fi
# gh and ssh are looked up on this PATH inside launchd; record where they are now.
for tool in gh ssh git rsync; do
  command -v "$tool" >/dev/null || { echo "$tool not found on PATH" >&2; exit 2; }
done
AGENT_PATH="$(dirname "$(command -v gh)"):$(dirname "$(command -v git)"):/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

mkdir -p "$SHARE_DIR" "$STATE_DIR" "$(dirname "$PLIST")"
NEW_PLIST="$(mktemp "${TMPDIR:-/tmp}/$LABEL.XXXXXX")"
trap 'rm -f "$NEW_PLIST"' EXIT
sed -e "s|@PYTHON@|$PYTHON|g" \
    -e "s|@SCRIPT@|$SHARE_DIR/local_throughput_gate.py|g" \
    -e "s|@REMOTE@|$REMOTE|g" \
    -e "s|@HOUR@|$HOUR|g" \
    -e "s|@MINUTE@|$MINUTE|g" \
    -e "s|@PATH@|$AGENT_PATH|g" \
    -e "s|@STATE_DIR@|$STATE_DIR|g" \
    "$HERE/launchd/$LABEL.plist.template" > "$NEW_PLIST"
plutil -lint "$NEW_PLIST" >/dev/null
if grep -qE 'threshold-override|issue-namespace' "$NEW_PLIST"; then
  echo "refusing: the rendered plist carries a test-only argument" >&2
  exit 1
fi
# Everything is validated; only now touch the running agent, so a failed re-install
# leaves the previous one loaded (there is no dead-man switch to notice it missing).
cp "$HERE/local_throughput_gate.py" "$SHARE_DIR/local_throughput_gate.py"
mv "$NEW_PLIST" "$PLIST"
launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
launchctl bootstrap "$DOMAIN" "$PLIST"
echo "installed $LABEL: daily at $(printf '%02d:%02d' "$HOUR" "$MINUTE"), remote=$REMOTE"
echo "check: launchctl print $DOMAIN/$LABEL | grep -E 'state|last exit'; tail -n 2 $STATE_DIR/results.jsonl"
