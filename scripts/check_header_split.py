#!/usr/bin/env python3
"""Invariants of the engine's capability headers: the lumice_*.h family that lumice.h gathers.

The engine's C API is declared across one header per capability (base, scene, render, editor,
engine, raypath). Which headers they are is not restated here: it is the engine's export surface
(LUMICE_ENGINE_SURFACE_HEADERS in cmake/export_surfaces.cmake) minus the analytic headers it shares
with liblumice_analytic. Three invariants hold for that family, none of which depends on how many
declarations there are, so the API can grow without touching this script:

  1. Every name is declared by exactly one header. Kinds: exported functions (the same parse that
     generates the export lists, scripts/gen_export_list.py), typedef / struct / enum / union
     names, and macros (include guards aside). C accepts a compatible redeclaration of a function,
     an identical typedef (C11) and an identical #define without a word, so a declaration copied
     into a second header instead of moved would compile, export and run — and then the two
     copies would drift. This is the check that notices.
  2. The #include graph between the family's headers and lumice.h has no cycle.
  3. lumice.h, while it exists, declares nothing and includes every header of the family: it is
     only a gathering point for the includers not yet moved to a specific header.

`--against <git-rev>` additionally compares the family's declarations with a single
src/include/lumice.h at that revision — a one-off reconciliation for the commit that split the
header, not a standing check (it names a revision, and the API is allowed to grow after it).

Exit status 0 when everything holds, 1 otherwise; each finding is printed on its own line.
"""
from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import gen_export_list  # noqa: E402
from check_policies import EXPORT_SURFACES_REL, parse_export_surfaces, strip_comments  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent
UMBRELLA_REL = "src/include/lumice.h"

_DEFINE = re.compile(r"^\s*#\s*define\s+(\w+)")
_IFNDEF = re.compile(r"^\s*#\s*ifndef\s+(\w+)")
_QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
_EXTERN_C_OPEN = re.compile(r'extern\s*"C"\s*\{')
_FN_POINTER_NAME = re.compile(r"\(\s*\*\s*(\w+)\s*\)")
_TAGGED_DEFINITION = re.compile(r"^(?:struct|enum|union)\s+(\w+)\s*\{")
_IDENT = re.compile(r"\b([A-Za-z_]\w*)\b")

KINDS = ("function", "type", "macro")


def family_headers(repo_root: Path) -> list[str]:
    """Repo-relative capability headers: the engine surface minus the analytic surface."""
    surfaces = parse_export_surfaces((repo_root / EXPORT_SURFACES_REL).read_text(encoding="utf-8"))
    analytic = set(surfaces.get("ANALYTIC", []))
    return [h for h in surfaces["ENGINE"] if h not in analytic]


def _include_guard(lines: list[str]) -> str | None:
    """The guard macro: the first directive is `#ifndef X`, the next line `#define X`."""
    for i, line in enumerate(lines):
        if not line.strip():
            continue
        m = _IFNDEF.match(line)
        if m and i + 1 < len(lines):
            d = _DEFINE.match(lines[i + 1])
            if d and d.group(1) == m.group(1):
                return m.group(1)
        return None
    return None


def _type_names(code: str) -> list[str]:
    """typedef / tagged-definition names declared at file scope, in order.

    `code` has comments and directives blanked. Statements are split at `;` outside braces; the
    `extern "C" {` wrapper is not a scope for this purpose, and its closing brace is ignored.
    """
    code = _EXTERN_C_OPEN.sub(" ", code)
    names: list[str] = []
    depth = 0
    buf: list[str] = []
    for ch in code:
        if ch == "{":
            depth += 1
        elif ch == "}":
            if depth == 0:
                continue
            depth -= 1
        if ch == ";" and depth == 0:
            stmt = " ".join("".join(buf).split())
            buf = []
            if stmt.startswith("typedef"):
                m = _FN_POINTER_NAME.search(stmt)
                if m:
                    names.append(m.group(1))
                else:
                    idents = _IDENT.findall(re.sub(r"\[[^\]]*\]", " ", stmt))
                    names.append(idents[-1])
            else:
                m = _TAGGED_DEFINITION.match(stmt)
                if m:
                    names.append(m.group(1))
            continue
        buf.append(ch)
    return names


def declarations(text: str, origin: str = "<text>") -> dict[str, list[str]]:
    """{kind: [name, ...]} for one header's text. Macros keep repeats (a macro defined in two
    branches of an #if counts twice); functions are listed once each."""
    lines = text.split("\n")
    guard = _include_guard(lines)
    macros = []
    for line in strip_comments(text).split("\n"):
        m = _DEFINE.match(line)
        if m and m.group(1) != guard:
            macros.append(m.group(1))
    code = gen_export_list._drop_preprocessor(strip_comments(text))
    return {
        "function": gen_export_list.parse_header_text(text, origin),
        "type": _type_names(code),
        "macro": macros,
    }


def quoted_includes(text: str) -> list[str]:
    return [m.group(1) for line in strip_comments(text).split("\n") if (m := _QUOTED_INCLUDE.match(line))]


def _find_cycle(graph: dict[str, list[str]]) -> list[str] | None:
    state: dict[str, int] = {}
    stack: list[str] = []

    def visit(node: str) -> list[str] | None:
        state[node] = 1
        stack.append(node)
        for nxt in graph.get(node, []):
            if state.get(nxt) == 1:
                return stack[stack.index(nxt) :] + [nxt]
            if nxt not in state:
                found = visit(nxt)
                if found:
                    return found
        stack.pop()
        state[node] = 2
        return None

    for node in sorted(graph):
        if node not in state:
            found = visit(node)
            if found:
                return found
    return None


def check(repo_root: Path) -> list[str]:
    """Findings for the three standing invariants (empty when they hold)."""
    out: list[str] = []
    family = family_headers(repo_root)
    umbrella = repo_root / UMBRELLA_REL
    texts: dict[str, str] = {}
    for rel in family:
        path = repo_root / rel
        if not path.is_file():
            out.append(f"{rel}: listed in {EXPORT_SURFACES_REL} but does not exist")
            continue
        texts[rel] = path.read_text(encoding="utf-8")
    if umbrella.is_file():
        texts[UMBRELLA_REL] = umbrella.read_text(encoding="utf-8")

    # 1. one declaring header per name
    owners: dict[tuple[str, str], list[str]] = collections.defaultdict(list)
    for rel, text in texts.items():
        try:
            decls = declarations(text, rel)
        except gen_export_list.ExportListError as e:
            out.append(str(e))
            continue
        for kind in KINDS:
            for name in sorted(set(decls[kind])):
                owners[(kind, name)].append(rel)
        if rel == UMBRELLA_REL and any(decls[k] for k in KINDS):
            named = ", ".join(n for k in KINDS for n in decls[k])
            out.append(f"{rel}: declares {named}; it only gathers the capability headers")
    for (kind, name), rels in sorted(owners.items()):
        if len(rels) > 1:
            out.append(f"{kind} {name} is declared in {len(rels)} headers: {', '.join(rels)}")

    # 2. acyclic include graph, within src/include/
    include_dir = (repo_root / UMBRELLA_REL).parent
    graph: dict[str, list[str]] = {}
    for rel, text in texts.items():
        graph[rel] = []
        for target in quoted_includes(text):
            path = (repo_root / rel).parent / target
            if path.is_file() and path.resolve().parent == include_dir.resolve():
                graph[rel].append(path.resolve().relative_to(repo_root.resolve()).as_posix())
    cycle = _find_cycle(graph)
    if cycle:
        out.append("include cycle: " + " -> ".join(cycle))

    # 3. the umbrella gathers every capability header
    if UMBRELLA_REL in texts:
        missing = [rel for rel in family if rel in texts and rel not in graph[UMBRELLA_REL]]
        for rel in missing:
            out.append(f"{UMBRELLA_REL}: does not include {Path(rel).name}")
    return out


def reconcile(repo_root: Path, rev: str) -> tuple[list[str], dict[str, tuple[int, int]]]:
    """Compare the family's declarations with src/include/lumice.h at `rev`.

    Returns (findings, {kind: (count at rev, count now)}). Counts are occurrences: a macro
    defined in two #if branches counts twice on both sides, so they must match exactly.
    """
    old_text = subprocess.run(
        ["git", "-C", str(repo_root), "show", f"{rev}:{UMBRELLA_REL}"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    old = declarations(old_text, f"{rev}:{UMBRELLA_REL}")
    new: dict[str, list[str]] = {k: [] for k in KINDS}
    rels = family_headers(repo_root) + ([UMBRELLA_REL] if (repo_root / UMBRELLA_REL).is_file() else [])
    for rel in rels:
        path = repo_root / rel
        if path.is_file():
            for kind, names in declarations(path.read_text(encoding="utf-8"), rel).items():
                new[kind].extend(names)
    out: list[str] = []
    counts: dict[str, tuple[int, int]] = {}
    for kind in KINDS:
        before, after = collections.Counter(old[kind]), collections.Counter(new[kind])
        counts[kind] = (sum(before.values()), sum(after.values()))
        for name in sorted((before - after).keys()):
            out.append(f"{kind} {name}: declared at {rev}, missing now")
        for name in sorted((after - before).keys()):
            out.append(f"{kind} {name}: not declared at {rev}, or declared more often now")
    return out, counts


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    ap.add_argument("--against", metavar="REV", help="one-off: reconcile with src/include/lumice.h at REV")
    args = ap.parse_args(argv)
    findings = check(args.repo_root)
    if args.against:
        more, counts = reconcile(args.repo_root, args.against)
        findings += more
        for kind, (before, after) in counts.items():
            print(f"{kind}: {before} at {args.against}, {after} now")
    for f in findings:
        print(f"check_header_split.py: {f}", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
