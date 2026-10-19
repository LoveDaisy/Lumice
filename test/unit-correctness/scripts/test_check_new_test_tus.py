"""Regression net for `scripts/check_new_test_tus.py`.

Same shape as `test_check_new_refs.py` and for the same reason: this gate is
set algebra over a unified-diff parser plus a JSON ledger, and every defect
such a gate can have fails *silently* — a wrong set difference reads as a
fresh declaration, an empty-set `sum()` reads as zero claimed quota, a
mis-parsed deps payload reads as "no hub headers touched". The checker stays
green while enforcing nothing, so its own tests are the only place a lost
tooth is visible.

Each test states the mechanism it pins. The ledger used below is fictional
payload-wise (invented reasons), but the counts are real git diffs over real
commits in a throwaway repo — the checker's whole authority is that its
numbers come from git, so the tests exercise git, not fixtures of git.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))

import check_new_refs  # noqa: E402
import check_new_test_tus  # noqa: E402
import check_policies  # noqa: E402


class Repo:
    """A throwaway git repo the checker is pointed at via its REPO_ROOT global."""

    def __init__(self, root: Path):
        self.root = root

    def _git(self, *args: str) -> str:
        return subprocess.run(
            ["git", *args], cwd=self.root, capture_output=True, check=True, text=True
        ).stdout

    def write(self, rel: str, text: str) -> None:
        p = self.root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")

    def commit(self, msg: str = "wip") -> str:
        self._git("add", "-A")
        self._git("commit", "-q", "-m", msg)
        return self._git("rev-parse", "HEAD").strip()

    def stage(self) -> None:
        self._git("add", "-A")

    def add_test_tus(self, n: int, prefix: str = "new_case") -> None:
        for i in range(n):
            self.write(f"test/{prefix}_{i}.cpp", f"// added test TU #{i}\n")

    def write_ledger(self, declarations: list[dict]) -> None:
        self.write(
            check_new_test_tus.LEDGER_RELPATH,
            json.dumps({"version": 1, "declarations": declarations}, indent=2) + "\n",
        )

    def verdict(self) -> check_new_test_tus.Verdict:
        """Run the TU half over the staged diff, as the pre-commit hook does."""
        self.stage()
        return check_new_test_tus.check_tu_half(["--cached"], "", "HEAD")

    def messages(self) -> list[str]:
        return [v.message for v in self.verdict().violations]


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Repo:
    r = Repo(tmp_path)
    r._git("init", "-q", "-b", "main")
    r._git("config", "user.email", "t@example.com")
    r._git("config", "user.name", "T")
    # Three bindings, not one: the git helpers and the blob reader use the
    # copy imported into check_new_refs, the subprocess cwd uses the copy in
    # check_new_test_tus, and Violation.render() resolves the name in the
    # module defining it.
    monkeypatch.setattr(check_new_refs, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_new_test_tus, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    r.write("seed.txt", "seed\n")
    r.commit("seed")
    return r


# --- case 1 + 2: the quota, its boundary, and the skeleton in the red message --


def test_free_quota_boundary_five_green_six_red(repo: Repo) -> None:
    """Five added test TUs pass with no ledger; six do not.

    Pinned at the boundary because the quota is a constant a tidying pass
    could nudge in either direction without anything else noticing: one
    higher silently widens the free mass, one lower puts ordinary PRs
    through the ledger.
    """
    repo.add_test_tus(5)
    v = repo.verdict()
    assert v.new_tus == 5
    assert v.violations == []

    repo.add_test_tus(1, prefix="one_more")
    v = repo.verdict()
    assert v.new_tus == 6
    assert len(v.violations) == 1


def test_quota_red_message_carries_skeleton_and_bookkeeping_rules(repo: Repo) -> None:
    """The red message must be actionable without reading the checker.

    The skeleton pre-fills `new_test_tus` with the diff's actual added TU
    count (computed from git here, so the number in the message is the number
    to declare), marks the reason with the placeholder the schema rejects, and
    states the append-only rule — editing an existing entry is the tempting
    wrong fix and must be named as unavailable.
    """
    repo.add_test_tus(6)
    (message,) = repo.messages()
    assert '"new_test_tus": 6' in message
    assert '"reason": "TODO"' in message
    assert "never edit existing entries" in message
    assert check_new_test_tus.LEDGER_RELPATH in message


# --- case 3 + 11: the cross-checked sum ----------------------------------------


def test_declaration_sum_equal_to_actual_is_green(repo: Repo) -> None:
    """A diff that adds entries declaring exactly its own added TU count passes.

    "Exactly" is the invariant: the sum is cross-checked against a count this
    checker computes from git itself, so both directions of dishonesty —
    claiming less to squeeze under the quota, claiming more to stockpile
    audit mass — are red, and only the true number is green.
    """
    repo.add_test_tus(6)
    repo.write_ledger(
        [{"date": "2026-10-19", "new_test_tus": 6, "reason": "fixture: bulk fixture feature"}]
    )
    assert repo.verdict().violations == []


@pytest.mark.parametrize("claimed", [4, 8])
def test_declaration_sum_unequal_to_actual_is_red(repo: Repo, claimed: int) -> None:
    repo.add_test_tus(6)
    repo.write_ledger(
        [{"date": "2026-10-19", "new_test_tus": claimed, "reason": "fixture: wrong on purpose"}]
    )
    (message,) = repo.messages()
    assert f"declare {claimed} new test TU(s)" in message
    assert f"the diff adds 6" in message


# --- case 4 + 10 + 13 + 14: entries do not carry across PRs; the ledger is append-only


def test_existing_entry_does_not_cover_a_new_excess(repo: Repo) -> None:
    """A declaration merged in an earlier PR buys this PR nothing.

    The key anti-leak case: base has an entry, head leaves it untouched (so
    the append-only invariant is satisfied), and the diff adds six fresh TUs
    with no new entry. If `claimed` were computed over the whole head ledger
    instead of the diff's new entries, this would pass — one merged bulk PR
    would permanently exempt every later one.
    """
    repo.write_ledger(
        [{"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: earlier bulk addition"}]
    )
    repo.commit("bulk PR lands")
    repo.add_test_tus(6)
    (message,) = repo.messages()
    assert "free quota" in message
    assert "declared entries cover 0" in message


def test_editing_an_existing_entry_is_red_and_names_it(repo: Repo) -> None:
    """Append-only: rewording a merged entry's reason is a red that lists it.

    This is the mint-new-quota attack the invariant exists for: an edited
    entry surfaces in the set difference as a "new declaration", so without
    this check a PR could claim fresh TUs by touching old prose.
    """
    repo.write_ledger(
        [{"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: earlier reason"}]
    )
    repo.commit("bulk PR lands")
    repo.add_test_tus(6)
    repo.write_ledger(
        [{"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: reworded reason"}]
    )
    (message,) = repo.messages()
    assert "append-only" in message
    assert "fixture: earlier reason" in message


def test_steady_state_small_pr_with_existing_ledger_is_green(repo: Repo) -> None:
    """The exemption behind the cross-check's applicability condition.

    After the first declaration the ledger exists forever, and ordinary PRs
    keep adding one or two TUs with no entry. A cross-check implemented as a
    bare `sum(entries) == new_tus` reads `sum([]) == 0 != 2` here and turns
    the free quota red for every PR after the first — the highest-frequency
    path through the gate, pinned explicitly.
    """
    repo.write_ledger(
        [{"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: earlier bulk addition"}]
    )
    repo.commit("bulk PR lands")
    repo.add_test_tus(2)
    assert repo.verdict().violations == []


def test_legitimate_append_keeps_old_entries_and_passes(repo: Repo) -> None:
    """Positive control for the append-only check: adding entries is fine.

    The old entry survives verbatim, two new entries (declared in non-sorted
    order, because the comparison is over sets and must not care about JSON
    ordering) sum to the diff's actual six added TUs — everything green.
    """
    repo.write_ledger(
        [{"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: earlier bulk addition"}]
    )
    repo.commit("bulk PR lands")
    repo.add_test_tus(6)
    repo.write_ledger(
        [
            {"date": "2026-10-01", "new_test_tus": 6, "reason": "fixture: earlier bulk addition"},
            {"date": "2026-10-19", "new_test_tus": 2, "reason": "fixture: new batch one"},
            {"date": "2026-10-19", "new_test_tus": 4, "reason": "fixture: new batch two"},
        ]
    )
    assert repo.verdict().violations == []


# --- case 5: renames and copies -------------------------------------------------


def test_rename_is_not_a_new_tu_but_a_copy_is(repo: Repo) -> None:
    """`git mv` relocates a TU; `cp` creates one.

    Same stance as the prose gate: a rename's content was authored elsewhere
    and is not this diff's new compile mass, while a copy puts a new path —
    and a new TU — into the tree, whoever typed it.
    """
    repo.write("test/renamed.cpp", "int f() { return 0; }\n")
    repo.commit()
    repo._git("mv", "test/renamed.cpp", "test/moved.cpp")
    assert repo.verdict().new_tus == 0

    repo.write("test/copied.cpp", "int f() { return 0; }\n")
    assert repo.verdict().new_tus == 1


# --- case 6: the glob constants, from both directions ---------------------------


def test_is_test_tu_reads_one_constant_set() -> None:
    """The predicate is the rule's single definition — pin its membership.

    A negative direction missing from the tests is how a constants edit
    (or an invisible-character typo inside one) silently widens the gate to
    sources, or narrows it off part of the test tree.
    """
    for suffix in (".cpp", ".cc", ".mm", ".cu"):
        assert check_new_test_tus.is_test_tu(f"test/thing{suffix}")
    for rel in ("src/thing.cpp", "test/thing.py", "test/thing.json", "test/thing.hpp", "docs/x.cpp"):
        assert not check_new_test_tus.is_test_tu(rel)


def test_gate_counts_only_test_tree_sources(repo: Repo) -> None:
    """The same membership, seen through the actual diff path.

    A .cpp under src/ and non-source suffixes under test/ contribute zero;
    the .mm positive pins that the suffix set — not just .cpp — reaches the
    gate (the two Metal test sources in this repo are the real instances).
    """
    repo.write("src/thing.cpp", "int f() { return 0; }\n")
    repo.write("test/thing.py", "x = 1\n")
    repo.write("test/data.json", "{}\n")
    assert repo.verdict().new_tus == 0

    repo.write("test/thing.mm", "int f() { return 0; }\n")
    assert repo.verdict().new_tus == 1


# --- case 7 + 12: schema is validated on every run -------------------------------


def _entry(**overrides) -> dict:
    entry = {"date": "2026-10-19", "new_test_tus": 6, "reason": "fixture: valid entry"}
    entry.update(overrides)
    return entry


@pytest.mark.parametrize(
    "payload,label",
    [
        ({"version": 2, "declarations": []}, "wrong version"),
        ({"version": 1}, "missing declarations"),
        ({"version": 1, "declarations": [], "extra": 1}, "extra top-level key"),
        ({"version": 1, "declarations": [_entry(extra_field=1)]}, "extra entry field"),
        ({"version": 1, "declarations": [{"date": "2026-10-19", "new_test_tus": 6}]}, "missing reason"),
        ({"version": 1, "declarations": [_entry(reason="  ")]}, "blank reason"),
        ({"version": 1, "declarations": [_entry(reason="TODO")]}, "placeholder reason"),
        (
            {
                "version": 1,
                "declarations": [
                    _entry(),
                    {"date": "2026-10-19", "new_test_tus": 6, "reason": "fixture: valid entry"},
                ],
            },
            "duplicate entries",
        ),
        ("not an object", "non-object top level"),
    ],
)
def test_malformed_ledger_is_red(repo: Repo, payload, label: str) -> None:
    """A broken ledger is a broken audit record wherever it came from.

    Schema validation is whole-file and unconditional — it does not wait for
    the PR to exceed the quota, because the set algebra that decides quota
    coverage is only meaningful over well-formed entries.
    """
    repo.add_test_tus(6)
    repo.write(
        check_new_test_tus.LEDGER_RELPATH,
        payload if isinstance(payload, str) else json.dumps(payload, indent=2),
    )
    messages = repo.messages()
    assert messages, f"expected a schema red for: {label}"


@pytest.mark.parametrize("tus", [None, 0, -2, True, "6", 1.5])
def test_new_test_tus_value_domain(repo: Repo, tus) -> None:
    """`new_test_tus` is a count: integer, >= 1, and not a bool.

    `True` is an `int` in Python and would slip a boolean past a bare
    isinstance check — the one value that must be excluded explicitly. A
    float like `1.5` is not a count of anything.
    """
    repo.add_test_tus(6)
    entry = _entry()
    if tus is None:
        del entry["new_test_tus"]
    else:
        entry["new_test_tus"] = tus
    repo.write_ledger([entry])
    assert repo.messages(), f"expected a schema red for new_test_tus={tus!r}"


# --- case 8: the hub half --------------------------------------------------------

def deps_fixture(root: Path, closures: dict[str, int]) -> str:
    """Synthetic `ninja -t deps` stdout: one TU block per (header, incidence)."""
    blocks: list[str] = []
    i = 0
    for header, count in closures.items():
        for _ in range(count):
            blocks.append(f"{root}/build/o{i}.o: #deps 1\n    {root}/{header}\n")
            i += 1
    return "".join(blocks)


def test_deps_closure_threshold_and_header_pattern(tmp_path: Path) -> None:
    """Closure aggregation and the notice threshold, plus the header glob.

    The header pattern is pinned from both directions like the TU glob: a
    listed suffix under src/ or test/ counts, an unlisted suffix (.hh) and a
    path outside both trees do not — otherwise the notice half silently
    widens to third-party headers or shrinks off part of the tree.
    """
    hub = "src/include/hub_header.h"
    small = "test/support/small_header.hpp"
    dot_hh = "src/core/odd_header.hh"
    outside = "doc/not_a_header.h"
    text = deps_fixture(tmp_path, {hub: 160, small: 5, dot_hh: 200, outside: 200})
    tus, closures = check_new_test_tus.parse_deps(text, str(tmp_path))
    assert tus == 160 + 5 + 200 + 200
    assert closures[hub] == 160
    assert closures[small] == 5
    assert dot_hh not in closures
    assert outside not in closures

    touched = [hub, small, dot_hh, outside]
    assert check_new_test_tus.hub_notices(touched, closures) == [(hub, 160)]
    assert [rel for rel in touched if check_new_test_tus.is_hub_header(rel)] == [hub, small]


def test_is_hub_header_membership() -> None:
    assert check_new_test_tus.is_hub_header("src/a.h")
    assert check_new_test_tus.is_hub_header("test/b.hpp")
    assert not check_new_test_tus.is_hub_header("src/a.hh")
    assert not check_new_test_tus.is_hub_header("build/generated.h")
    assert not check_new_test_tus.is_hub_header("cmake/a.hpp")


def test_deps_range_notice_end_to_end(
    repo: Repo, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The full --range + --deps path: a touched hub header comes out as a notice.

    Integration beyond the unit halves above: the touched-header list must
    come from the real diff (the header edit below), the closure from the
    deps payload, and the pair into a `::notice` line carrying both the path
    and the number — a notice without the number cannot be checked by a
    reviewer at all.
    """
    repo.write("src/include/hub_header.h", "// hub\n")
    repo.commit()
    base = repo._git("rev-parse", "HEAD").strip()
    repo.write("src/include/hub_header.h", "// hub\n// touched\n")
    repo.commit("touch the hub")

    deps_file = tmp_path / "all_deps.txt"
    deps_file.write_text(deps_fixture(tmp_path, {"src/include/hub_header.h": 160}), encoding="utf-8")

    monkeypatch.setattr(
        sys, "argv", ["check_new_test_tus.py", "--range", f"{base}..HEAD", "--deps", str(deps_file)]
    )
    assert check_new_test_tus.main() == 0
    out = capsys.readouterr().out
    assert "::notice" in out
    assert "src/include/hub_header.h" in out
    assert "160" in out


# --- case 9 + deps failure modes --------------------------------------------------


def test_no_deps_skips_the_hub_half_audibly(
    repo: Repo, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Without --deps the hub half says so out loud instead of passing silently.

    A silent skip would be indistinguishable from "checked, nothing to
    report"; the skip line is what tells a local reader that CI's judging leg
    is the only place this half actually runs.
    """
    monkeypatch.setattr(sys, "argv", ["check_new_test_tus.py", "--staged"])
    assert check_new_test_tus.main() == 0
    assert "skipped locally" in capsys.readouterr().err


def test_empty_deps_fails_loud_with_parse_error_marker(
    repo: Repo, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """A deps payload aggregating to zero TUs is an error, not an empty result.

    Hub misses have no red state of their own: a truncated or empty payload
    that passed as "no headers over threshold" would kill the half for every
    future PR while every check stays green. The failure carries the
    `deps-parse-error` marker so it reads differently from a TU-half red.
    """
    empty = tmp_path / "empty_deps.txt"
    empty.write_text("", encoding="utf-8")
    monkeypatch.setattr(
        sys, "argv", ["check_new_test_tus.py", "--staged", "--deps", str(empty)]
    )
    assert check_new_test_tus.main() == 1
    assert "deps-parse-error" in capsys.readouterr().err


def test_deps_touching_only_cpp_yields_zero_notices_and_zero_exit(
    repo: Repo, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """No header touched with --deps given is a legal empty result, not a parse failure.

    Pins the two states apart: zero touched headers (green, zero notices)
    must not be conflated with an unparsable payload (red). The `.cpp`-only
    diff below exercises the exact CI shape of a normal source-only PR.
    """
    repo.write("src/core/thing.cpp", "int f() { return 0; }\n")
    repo.commit()
    base = repo._git("rev-parse", "HEAD").strip()
    repo.write("src/core/thing.cpp", "int g() { return 1; }\n")
    repo.commit("edit a source file")

    deps_file = tmp_path / "deps.txt"
    deps_file.write_text(
        # Three TUs sharing one header the diff does not touch: a parsed,
        # healthy payload over a source-only diff.
        f"{tmp_path}/build/only.o: #deps 1\n    {tmp_path}/src/core/other.hpp\n" * 3,
        encoding="utf-8",
    )
    monkeypatch.setattr(
        sys, "argv", ["check_new_test_tus.py", "--range", f"{base}..HEAD", "--deps", str(deps_file)]
    )
    assert check_new_test_tus.main() == 0
    out = capsys.readouterr().out
    assert "::notice" not in out
    assert "0 touched header(s)" in out


# --- the CLI contract -------------------------------------------------------------


@pytest.mark.parametrize("rng", ["main...HEAD", "HEAD", ""])
def test_malformed_range_is_refused(repo: Repo, monkeypatch: pytest.MonkeyPatch, rng: str) -> None:
    """Same range contract as the sibling gates, pinned here too.

    The three-dot form splits into a head of `.HEAD` if passed through, and
    an empty range must not silently demote to a staged scan.
    """
    monkeypatch.setattr(sys, "argv", ["check_new_test_tus.py", "--range", rng])
    with pytest.raises(SystemExit) as e:
        check_new_test_tus.main()
    assert e.value.code == 2
