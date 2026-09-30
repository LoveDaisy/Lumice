#!/usr/bin/env python3
"""Invariants of the engine's capability headers: the lumice_*.h family under src/include/.

The engine's C API is declared across one header per capability (base, scene, render, editor,
engine, raypath); there is no header gathering them, and an includer names the capability headers
declaring what it uses. Which headers they are is not restated here: it is the engine's export
surface (LUMICE_ENGINE_SURFACE_HEADERS in cmake/export_surfaces.cmake) minus the analytic headers
it shares with liblumice_analytic. Two invariants hold for that family, neither of which depends on
how many declarations there are, so the API can grow without touching this script:

  1. Every name is declared by exactly one header. Kinds: exported functions (the same parse that
     generates the export lists, scripts/gen_export_list.py), typedef / struct / enum / union
     names, and macros (include guards aside). C accepts a compatible redeclaration of a function,
     an identical typedef (C11) and an identical #define without a word, so a declaration copied
     into a second header instead of moved would compile, export and run — and then the two
     copies would drift. This is the check that notices.
  2. The #include graph between the family's headers has no cycle.

Exit status 0 when everything holds, 1 otherwise; each finding is printed on its own line.
"""
from __future__ import annotations

import argparse
import collections
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import gen_export_list  # noqa: E402
from check_policies import EXPORT_SURFACES_REL, parse_export_surfaces, strip_comments  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR_REL = "src/include"

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
    """Findings for the two standing invariants (empty when they hold)."""
    out: list[str] = []
    family = family_headers(repo_root)
    texts: dict[str, str] = {}
    for rel in family:
        path = repo_root / rel
        if not path.is_file():
            out.append(f"{rel}: listed in {EXPORT_SURFACES_REL} but does not exist")
            continue
        texts[rel] = path.read_text(encoding="utf-8")

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
    for (kind, name), rels in sorted(owners.items()):
        if len(rels) > 1:
            out.append(f"{kind} {name} is declared in {len(rels)} headers: {', '.join(rels)}")

    # 2. acyclic include graph, within src/include/
    include_dir = (repo_root / INCLUDE_DIR_REL).resolve()
    graph: dict[str, list[str]] = {}
    for rel, text in texts.items():
        graph[rel] = []
        for target in quoted_includes(text):
            path = (repo_root / rel).parent / target
            if path.is_file() and path.resolve().parent == include_dir:
                graph[rel].append(path.resolve().relative_to(repo_root.resolve()).as_posix())
    cycle = _find_cycle(graph)
    if cycle:
        out.append("include cycle: " + " -> ".join(cycle))
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    args = ap.parse_args(argv)
    findings = check(args.repo_root)
    for f in findings:
        print(f"check_header_split.py: {f}", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
