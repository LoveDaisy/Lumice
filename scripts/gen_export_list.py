#!/usr/bin/env python3
"""Generate a shared library's export list from the C headers that define its surface.

The single authority on what each of Lumice's shared libraries exports. Every library —
liblumice (the src/include/lumice_*.h capability headers and the analytic capability),
liblumice_testapi (the same plus test/support/lumice_test_api.h) and liblumice_analytic
(src/include/lumice_analytic.h over its core header), as cmake/export_surfaces.cmake lists them —
gets its list from this script, called by the root CMakeLists.txt's lumice_apply_export_list();
nobody writes a list by hand. The same
objects (lumice_obj) go into all three libraries, so what a library exports cannot be decided by
an attribute compiled into those objects — it is decided at link time, per library, by the list
this script writes:

    --format gnu     GNU ld / lld version script:  { global: NAME; ... local: *; };
    --format darwin  ld64 -exported_symbols_list:  one _NAME per line (Mach-O C mangling)
    --format def     Windows module-definition:    EXPORTS / NAME per line

What counts as an exported function: a `LUMICE_*` identifier immediately followed by `(` in a
header's code (comments stripped by check_policies.strip_comments, preprocessor directives
dropped). A function-pointer typedef's name, such as `LUMICE_LogCallback` in
`typedef void (*LUMICE_LogCallback)(...)`, never matches this shape and needs no separate
filter: the name is followed by `)` (closing the `(*NAME)` declarator), not `(`, so it is
excluded by construction. The scan does not follow #include: a caller passes every header whose
functions the library exports, and the result is their union.

Every such declaration must also carry its header's visibility marker (`LUMICE_API`,
`LUMICE_TEST_API` or `LUMICE_ANALYTIC_API`). The engine objects are compiled with
-fvisibility=hidden in Release, and a hidden symbol cannot be exported by any list, so a
declaration that lost its marker would vanish from the shared library on GCC/Clang while still
appearing in the list — and a test comparing the library against this script's own parse would
not notice, since both sides would come from the same headers. So a missing marker is an error
here, at build time, on every platform, rather than a symbol that is silently absent.

The output file is rewritten only when its content changes, so an unrelated edit to a header (or
to check_policies.py, which this script imports) does not relink the libraries: Ninja restats a
custom command's output.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_policies import strip_comments  # noqa: E402

# A `LUMICE_*` identifier immediately followed (whitespace allowed) by an opening parenthesis.
# A pointer return type (`LUMICE_Server* LUMICE_CreateServer(void)`) still matches here — the
# `*` sits before the identifier, not between it and `(` — so it is correctly kept as a function.
_CALL_SHAPE = re.compile(r"\b(LUMICE_[A-Za-z0-9_]*)\s*\(")
# The visibility markers the three public headers define. LUMICE_API_VERSION and friends are
# not markers: \b after API rules them out.
_MARKER = re.compile(r"\bLUMICE_(?:TEST_|ANALYTIC_)?API\b")

FORMATS = ("gnu", "darwin", "def")


class ExportListError(Exception):
    pass


def _drop_preprocessor(code: str) -> str:
    """Blank every preprocessor directive, continuation lines included, keeping offsets.

    Directives are not declarations, and some of them spell a marker (`#define LUMICE_API ...`),
    which would otherwise satisfy the marker check for the declaration that follows them.
    """
    out = []
    continuing = False
    for line in code.split("\n"):
        if continuing or line.lstrip().startswith("#"):
            continuing = line.rstrip().endswith("\\")
            out.append(" " * len(line))
        else:
            out.append(line)
    return "\n".join(out)


def parse_header_text(text: str, origin: str = "<text>") -> list[str]:
    """Return the exported function names declared in one header's text, in order of appearance.

    Raises ExportListError if a declaration lacks a visibility marker.
    """
    code = _drop_preprocessor(strip_comments(text))
    names: list[str] = []
    unmarked: list[str] = []
    for m in _CALL_SHAPE.finditer(code):
        before = code[: m.start()].rstrip()
        name = m.group(1)
        # The declaration statement starts after the previous `;`, `{` or `}`.
        start = max(before.rfind(";"), before.rfind("{"), before.rfind("}")) + 1
        if not _MARKER.search(code[start : m.start()]):
            line = code.count("\n", 0, m.start()) + 1
            unmarked.append(f"{origin}:{line}: {name}")
        if name not in names:
            names.append(name)
    if unmarked:
        raise ExportListError(
            "declaration(s) without a visibility marker (LUMICE_API / LUMICE_TEST_API / "
            "LUMICE_ANALYTIC_API) — they would be hidden in the shared library:\n  "
            + "\n  ".join(unmarked)
        )
    return names


def parse_headers(paths: list[Path]) -> list[str]:
    """Sorted union of the exported function names declared across `paths`."""
    names: set[str] = set()
    for path in paths:
        names.update(parse_header_text(path.read_text(encoding="utf-8"), str(path)))
    return sorted(names)


def render(names: list[str], fmt: str) -> str:
    if not names:
        raise ExportListError("no exported function found — refusing to write an empty export list")
    if fmt == "gnu":
        body = "".join(f"    {n};\n" for n in names)
        return "{\n  global:\n" + body + "  local:\n    *;\n};\n"
    if fmt == "darwin":
        return "".join(f"_{n}\n" for n in names)
    if fmt == "def":
        return "EXPORTS\n" + "".join(f"  {n}\n" for n in names)
    raise ExportListError(f"unknown format {fmt!r}")


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(content)
    return True


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--format", required=True, choices=FORMATS)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("headers", nargs="+", type=Path)
    args = ap.parse_args(argv)
    try:
        content = render(parse_headers(args.headers), args.format)
    except (ExportListError, OSError) as e:
        print(f"gen_export_list.py: error: {e}", file=sys.stderr)
        return 1
    write_if_changed(args.output, content)
    return 0


if __name__ == "__main__":
    sys.exit(main())
