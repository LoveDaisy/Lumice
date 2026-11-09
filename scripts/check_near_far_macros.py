#!/usr/bin/env python3
"""Diff-scoped gate for windows.h's empty near/far macros.

Rule (the checker is the rule)
------------------------------
Reject bare, case-sensitive `near` / `far` identifier tokens on added lines of
tracked C-family sources (C/C++/Objective-C/CUDA/Metal, including headers and
.inl files). This includes declarations, parameters, member declarations and
uses, not just variables: windows.h/minwindef.h expands either name to nothing.
PR #424 and PR #579 both paid for this on the MSVC compile leg after other
platforms passed. An include-chain change can activate a previously safe name.
Rename it with a descriptive suffix, e.g. `near_pt` / `far_dir`.

Comments, ordinary string/character literals, suffixed names and case variants
are not identifiers in this rule. Member/qualified accesses preceded by `.`,
`->` or `::` are exempt, as are preprocessor directives (including continued
lines, so `#undef near` remains writable). These are textual exemptions, not a
claim that qualified tokens cannot be macro-expanded. There is no inline waiver.

The scope is deliberately limited to the twice-observed empty-macro family:
min/max are function-like macros neutralized by the MSVC NOMINMAX build define;
RGB is function-like and has no recorded incident. Neither is added speculatively.

Lexical scope: comments, ordinary/escaped literals, C++ raw strings (including
encoding prefixes) and digit-separated numbers are recognized in one pass.
This is not a C++ parser: inactive #if branches are still scanned, and translation
phase line-splicing outside literals is not modeled. Only added lines are gated;
--root is a diagnostic scan of tracked worktree files, not part of the hook/CI
steady-state path.

Usage: --staged (also the default), --range BASE..HEAD, or --root [DIR].
Exit 0 when clean, 1 on violations, 2 on invalid arguments.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_new_refs import C_FAMILY_SUFFIXES, added_lines, blob  # noqa: E402
from check_policies import REPO_ROOT, Violation  # noqa: E402

RULE = "near-far-macro"
IDENTIFIER = re.compile(r"\b(?:near|far)\b")
FIX_HINT = (
    "windows.h/minwindef.h defines near/far as empty macros, erasing this "
    "identifier on MSVC. Use a descriptive suffix, e.g. near_pt / far_dir."
)


# Match lexical regions together: a comment marker inside a string (and a quote
# inside a comment) cannot change the interpretation of the following source.
# pp-numbers must precede char literals, otherwise 100'000 opens a bogus char.
LEXICAL_REGION = re.compile(
    r"(?P<number>\b[0-9](?:[\w.']|[eEpP][+-])*)"
    r"|//[^\n]*|/\*.*?\*/"
    r'|(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"'
    r"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)


def identifier_code(text: str) -> str:
    """Blank non-code regions without changing offsets/newlines."""
    return LEXICAL_REGION.sub(
        lambda match: match.group(0) if match.group("number") is not None
        else re.sub(r"[^\n]", " ", match.group(0)),
        text,
    )


def find_violations_in_text(rel: str, text: str) -> list[Violation]:
    """One diagnostic per offending line, before optional diff intersection."""
    if PurePosixPath(rel).suffix not in C_FAMILY_SUFFIXES:
        return []
    code = identifier_code(text)
    directive_lines: set[int] = set()
    continued = False
    # Git hunks and diagnostics count physical LF lines, not Python's broader
    # splitlines separators (form-feed/vertical-tab are C++ whitespace).
    for lineno, (line, original) in enumerate(zip(code.split("\n"), text.split("\n")), 1):
        if lineno == 1:
            line = line.removeprefix("﻿")
        directive = continued or line.lstrip().startswith("#")
        if directive:
            directive_lines.add(lineno)
        # Line splicing precedes comment removal: a trailing backslash inside
        # a comment still continues the directive onto the next physical line.
        continued = directive and original.rstrip("\r").endswith("\\")

    violations: list[Violation] = []
    reported: set[int] = set()
    # Search the whole file so a member-access operator on the preceding line
    # remains visible; comments have become whitespace rather than token glue.
    lineno, previous_end = 1, 0
    for match in IDENTIFIER.finditer(code):
        lineno += code.count("\n", previous_end, match.start())
        previous_end = match.start()
        if lineno in directive_lines or lineno in reported:
            continue
        prefix = code[:match.start()].rstrip()
        if prefix.endswith((".", "->", "::")):
            continue
        violations.append(Violation(REPO_ROOT / rel, lineno, RULE, FIX_HINT))
        reported.add(lineno)
    return violations


def diff_verdict(
    diff_args: list[str], content_ref: str | None
) -> tuple[list[Violation], int]:
    """Read exactly the index, commit or worktree named by the caller."""
    violations: list[Violation] = []
    scanned = 0
    for rel, linenos in sorted(added_lines(diff_args).items()):
        if PurePosixPath(rel).suffix not in C_FAMILY_SUFFIXES:
            continue
        scanned += len(linenos)
        violations.extend(
            v for v in find_violations_in_text(rel, blob(content_ref, rel))
            if v.line in linenos
        )
    return violations, scanned


def tree_verdict(root: Path) -> tuple[list[Violation], int]:
    """Diagnostic mode: tracked files only, including staged new paths."""
    paths = subprocess.run(
        ["git", "ls-files", "-z", "--", str(root.resolve())],
        cwd=REPO_ROOT, capture_output=True, check=True,
    ).stdout.decode("utf-8", errors="replace").split("\0")
    violations: list[Violation] = []
    scanned = 0
    for rel in sorted(set(paths) - {""}):
        if PurePosixPath(rel).suffix not in C_FAMILY_SUFFIXES:
            continue
        text = blob(None, rel)
        scanned += text.count("\n") + int(bool(text) and not text.endswith("\n"))
        violations.extend(find_violations_in_text(rel, text))
    return violations, scanned


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--staged", action="store_true", help="check the index (default)")
    group.add_argument("--range", dest="rng", metavar="BASE..HEAD")
    group.add_argument("--root", nargs="?", const=str(REPO_ROOT), type=Path)
    args = parser.parse_args()
    if args.root is not None:
        violations, scanned = tree_verdict(args.root)
    elif args.rng is not None:
        if ".." not in args.rng:
            parser.error("--range expects BASE..HEAD")
        if "..." in args.rng:
            parser.error(
                "--range expects the two-dot form BASE..HEAD; for merge-base "
                "semantics pass `$(git merge-base A B)..B` explicitly"
            )
        base, head = args.rng.split("..", 1)
        if not base or not head:
            parser.error("--range expects BASE..HEAD")
        violations, scanned = diff_verdict([base, head], head)
    else:
        violations, scanned = diff_verdict(["--cached"], "")
    if violations:
        print("Near/far macro check FAILED:\n", file=sys.stderr)
        for v in violations:
            print(v.render(), file=sys.stderr)
        print(f"\n{len(violations)} violation(s).", file=sys.stderr)
        return 1
    print(f"Near/far macro check passed ({scanned} source line(s) scanned).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
