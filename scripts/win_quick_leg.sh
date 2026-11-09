#!/usr/bin/env bash
# Optional native-Windows closing leg. See testing-architecture.md §7.11.
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
cd "${ROOT_DIR}"
HOST=${LUMICE_WIN_QUICK_HOST:-home-win}
WS=${LUMICE_WIN_QUICK_WS:-C:/lumice-test/ws-winleg}
WAIT=${LUMICE_WIN_QUICK_LOCK_WAIT:-600}
LIMIT=${LUMICE_WIN_QUICK_TIMEOUT:-3600}
LABELS=${1:-unit-correctness|composition-correctness|parity|golden-analytic}
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=10 -o ServerAliveInterval=15 -o ServerAliveCountMax=3)
SCP=(scp -o BatchMode=yes -o ConnectTimeout=10 -o ServerAliveInterval=15 -o ServerAliveCountMax=3)
# These values cross the cmd/PowerShell boundary; reject metacharacters rather
# than invent a second command-line quoting language.
if [[ ! ${WS} =~ ^[A-Za-z]:/[A-Za-z0-9_./-]+$ || ${WS} == */../* || ${WS} == */.. ||
      ! ${WAIT} =~ ^[0-9]+$ || ! ${LIMIT} =~ ^[1-9][0-9]*$ ||
      ! ${LABELS} =~ ^[A-Za-z0-9_\|.-]+$ ]]; then
  printf 'WIN_LEG_RESULT: FAIL stage=sync reason=invalid-argument\n'
  exit 1
fi
# Destructive full-sync recovery is confined to a dedicated leaf, never a clone
# or a drive root. The remote side independently checks the ownership sentinel.
if [[ ${WS} != */ws-winleg* ]]; then
  printf 'WIN_LEG_RESULT: FAIL stage=sync reason=workspace-not-dedicated\n'
  exit 1
fi
if ! "${SSH[@]}" "${HOST}" 'cmd /c exit 0' >/dev/null 2>&1; then
  printf 'WIN_LEG_RESULT: SKIPPED unreachable\n'
  exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT
fail() { printf 'WIN_LEG_RESULT: FAIL stage=sync reason=%s\n' "$1"; exit 1; }
REV=$(git rev-parse HEAD)
printf 'win-leg: commit=%s host=%s workspace=%s\n' "${REV}" "${HOST}" "${WS}"
# Snapshot all identities once: neither a concurrent commit nor a dirty worktree
# can change the tree being certified halfway through packaging.
if [[ -n $(git status --porcelain --untracked-files=no) ]]; then
  printf 'win-leg: synced=HEAD (worktree dirty)\n'
fi
LFS_DIRTY=$(git status --porcelain --untracked-files=no -- resources/ test/fixtures/li-parity/)
if [[ -n ${LFS_DIRTY} ]]; then
  printf 'WIN_LEG_RESULT: SKIPPED lfs-dirty %s\n' "${LFS_DIRTY//$'\n'/; }"
  exit 0
fi
WS_CMD=${WS//\//\\}
STAMP=$("${SSH[@]}" "${HOST}" "cmd /c type ${WS_CMD}\\.lumice-winleg-sync-rev" 2>/dev/null | tr -d '\r\n') || STAMP=""
MODE=full
if [[ ${STAMP} =~ ^[0-9a-f]{40}$ ]] && git merge-base --is-ancestor "${STAMP}" "${REV}" 2>/dev/null; then
  if git diff --no-renames --name-status -z "${STAMP}" "${REV}" > "${TMP}/diff"; then MODE=diff; fi
fi
git archive "${REV}" -o "${TMP}/head.tar" || fail archive
python3 - "${TMP}" "${REV}" "${STAMP}" "${MODE}" "${LABELS}" "${WS}" "${WAIT}" "${LIMIT}" <<'PY' || fail package-exception
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile

out = Path(sys.argv[1])
rev, stamp, mode, labels, ws, wait, limit = sys.argv[2:]
manifest = {}
with tarfile.open(out / 'head.tar') as source, tarfile.open(out / 'source.tar', 'w') as dest:
    for member in source:
        if not member.isfile():
            continue
        name = member.name
        if name.startswith('build/') or name.startswith('.lumice-winleg-'):
            raise RuntimeError(f'reserved workspace path: {name}')
        data = source.extractfile(member).read()
        if name.startswith(('resources/', 'test/fixtures/li-parity/')) and data.startswith(b'version https://git-lfs.github.com/spec/v1\n'):
            pointer = dict(line.split(' ', 1) for line in data.decode().splitlines()[1:])
            data = Path(name).read_bytes()
            if hashlib.sha256(data).hexdigest() != pointer['oid'].removeprefix('sha256:') or len(data) != int(pointer['size']):
                raise RuntimeError(f'LFS content does not match HEAD: {name}')
        manifest[name] = hashlib.sha256(data).hexdigest()
        member.size = len(data)
        dest.addfile(member, io.BytesIO(data))
expected = set(subprocess.check_output(['git', 'ls-tree', '-r', '-z', '--name-only', rev]).decode('utf-8').split('\0')[:-1])
if set(manifest) != expected:
    raise RuntimeError('HEAD contains unsupported non-regular tracked entries')
changes, deletes = [], []
if mode == 'diff':
    parts = (out / 'diff').read_bytes().split(b'\0')
    for status, path in zip(parts[0::2], parts[1::2]):
        name = path.decode('utf-8')
        (deletes if status == b'D' else changes).append(name)
request = dict(rev=rev, stamp=stamp, mode=mode, labels=labels, workspace=ws,
               wait=int(wait), timeout=int(limit), manifest=manifest,
               changes=changes, deletes=deletes)
(out / 'request.json').write_text(json.dumps(request), encoding='utf-8')
for name in ('win_quick_leg_remote.cmd', 'win_quick_leg_remote.ps1'):
    (out / name).write_bytes(subprocess.check_output(['git', 'show', f'{rev}:scripts/{name}']))
PY
TOKEN="${REV:0:12}-$(basename "${TMP}")"
REMOTE="C:/lumice-test/winleg-runs/${TOKEN}"
REMOTE_CMD=${REMOTE//\//\\}
"${SSH[@]}" "${HOST}" "cmd /c mkdir ${REMOTE_CMD}" >/dev/null || fail staging
"${SCP[@]}" "${TMP}/source.tar" "${TMP}/request.json" "${TMP}/win_quick_leg_remote.cmd" "${TMP}/win_quick_leg_remote.ps1" "${HOST}:${REMOTE}/" || fail transfer
# Remote stdout is a control channel. Build/test output lives on the remote disk
# first, so a dropped SSH connection cannot erase the diagnostic evidence.
rc=0
"${SSH[@]}" "${HOST}" "cmd /c powershell.exe -NoProfile -ExecutionPolicy Bypass -File ${REMOTE}/win_quick_leg_remote.ps1 ${REMOTE}/request.json" > "${TMP}/control.log" 2>&1 || rc=$?
python3 - "${TMP}/control.log" <<'PY'
from pathlib import Path
import sys
print(Path(sys.argv[1]).read_text(errors='replace'), end='')
PY
"${SCP[@]}" "${HOST}:${REMOTE}/run.log" "${TMP}/run.log" >/dev/null 2>&1 || true
"${SCP[@]}" "${HOST}:${REMOTE}/run.err" "${TMP}/run.err" >/dev/null 2>&1 || true
# Preserve the complete log locally; test.sh prints only the failure excerpt.
for log in run.log run.err; do
  if [[ -f ${TMP}/${log} ]]; then
    python3 - "${TMP}/${log}" <<'PY'
from pathlib import Path
import sys
print('\n'.join(line for line in Path(sys.argv[1]).read_text(errors='replace').splitlines()
                if not line.startswith('WIN_LEG_RESULT:')), end='\n')
PY
  fi
done
# A transport failure or absent result is never a skip once work has started.
if [[ ${rc} -ne 0 ]]; then
  printf 'WIN_LEG_RESULT: FAIL stage=transport reason=remote-exit-%s\n' "${rc}"
  exit 1
fi
RESULT=$(grep '^WIN_LEG_RESULT:' "${TMP}/control.log" | tr -d '\r') || RESULT=""
case "${RESULT}" in
  'WIN_LEG_RESULT: PASS') exit 0 ;;
  'WIN_LEG_RESULT: SKIPPED '*) exit 0 ;;
  'WIN_LEG_RESULT: FAIL '*) exit 1 ;;
  *) printf 'WIN_LEG_RESULT: FAIL stage=transport reason=missing-result\n'; exit 1 ;;
esac
