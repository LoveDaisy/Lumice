"""Unit tests for `scripts/check_export_surface.py`, the one reader of a built library's export
table (used by test/e2e-correctness/test_export_symbol_scope.py and by the release workflow).
No build and no toolchain needed: the parsers get fixed samples of each tool's output, and
`check` gets an injected reader, so the Windows parser is exercised on every platform too. Whether
the real tools print what these samples say is the e2e test's business, on each CI platform.
"""
from __future__ import annotations

import io
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import check_export_surface as ces  # noqa: E402

# `nm -D --defined-only` on a GNU ld shared object: two exports plus the symbols ld adds itself.
NM_LINUX = """\
0000000000012340 T LUMICE_A
0000000000012350 T LUMICE_B
0000000000040000 B __bss_start
0000000000040000 D _edata
0000000000040008 B _end
0000000000001000 T _fini
0000000000000800 T _init
"""

# `nm -gU` on a Mach-O dylib: C names carry a leading underscore.
NM_DARWIN = """\
0000000000001234 T _LUMICE_A
0000000000001240 T _LUMICE_B
"""

# `dumpbin /nologo /exports`: header rows that look numeric must not read as exports; a forwarder
# row keeps only its exported name.
DUMPBIN = """\

Dump of file lumice-engine.baseline.dll

File Type: DLL

  Section contains the following exports for lumice-engine.baseline.dll

    00000000 characteristics
    FFFFFFFF time date stamp
        0.00 version
           1 ordinal base
           2 number of functions
           2 number of names

    ordinal hint RVA      name

          1    0 00001000 LUMICE_A
          2    1 00001010 LUMICE_B = LUMICE_B_impl

  Summary

        1000 .data
"""


def test_parse_nm_linux_drops_only_the_named_gnu_linker_symbols() -> None:
    assert ces.parse_nm(NM_LINUX, "linux") == {"LUMICE_A", "LUMICE_B"}
    # Anything else unexpected is kept, so it fails the comparison instead of vanishing.
    assert ces.parse_nm("0000000000001000 T _ZN6lumice3FooEv\n", "linux") == {"_ZN6lumice3FooEv"}


def test_parse_nm_darwin_strips_the_mach_o_underscore() -> None:
    assert ces.parse_nm(NM_DARWIN, "darwin") == {"LUMICE_A", "LUMICE_B"}


def test_parse_dumpbin_reads_only_table_rows() -> None:
    assert ces.parse_dumpbin(DUMPBIN) == {"LUMICE_A", "LUMICE_B"}


def test_compare_reports_both_directions() -> None:
    assert ces.compare({"A", "B"}, {"A", "B"}) == ([], [])
    assert ces.compare({"A", "X"}, {"A", "B"}) == (["X"], ["B"])


def _check(files: list[str], tables: dict[str, set[str]]) -> tuple[int, str]:
    def read(path: Path) -> set[str]:
        if str(path) not in tables:
            raise ces.ExportTableError(f"{path}: no such file")
        return tables[str(path)]

    out = io.StringIO()
    return ces.check("ANALYTIC", [Path(f) for f in files], read=read, out=out), out.getvalue()


def test_check_passes_every_matching_file_and_prints_one_line_each() -> None:
    want = ces.expected("ANALYTIC")
    rc, text = _check(["a.so", "b.so"], {"a.so": set(want), "b.so": set(want)})
    assert rc == 0, text
    assert [line.split()[0] for line in text.splitlines()] == ["OK", "OK"]


def test_check_fails_on_a_missing_name_and_names_it() -> None:
    want = ces.expected("ANALYTIC")
    dropped = sorted(want)[0]
    rc, text = _check(["a.so"], {"a.so": want - {dropped}})
    assert rc == 1 and dropped in text and text.startswith("FAIL")


def test_check_fails_on_an_undeclared_export() -> None:
    rc, text = _check(["a.so"], {"a.so": ces.expected("ANALYTIC") | {"LUMICE_ANALYTIC_Leaked"}})
    assert rc == 1 and "LUMICE_ANALYTIC_Leaked" in text


def test_one_bad_file_among_good_ones_fails_the_run() -> None:
    """Not just the first file: the release passes both ISA engines in one call."""
    want = ces.expected("ANALYTIC")
    rc, text = _check(["good.so", "bad.so"], {"good.so": set(want), "bad.so": set()})
    assert rc == 1
    assert text.splitlines()[0].startswith("OK") and text.splitlines()[1].startswith("FAIL")


def test_missing_file_and_empty_file_list_fail() -> None:
    assert _check(["gone.so"], {})[0] == 1
    rc, text = _check([], {})
    assert rc == 1 and "nothing was checked" in text


def test_every_declared_surface_has_a_command_line_name() -> None:
    assert set(ces.surfaces()) == set(ces.SURFACE_KEY.values())
    with pytest.raises(SystemExit):
        ces.main(["nonsense", "a.so"])
