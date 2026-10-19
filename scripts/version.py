#!/usr/bin/env python3
"""Version management script for Lumice.

Usage:
    python scripts/version.py check              # Compare CMakeLists.txt version with latest git tag
    python scripts/version.py check --tag v4.0.0 # Compare CMakeLists.txt version with specified tag (CI mode)
    python scripts/version.py set 4.1.0          # Bump CMakeLists.txt version; validate the CHANGELOG.md section
    python scripts/version.py extract-notes 4.1.0  # Print one CHANGELOG.md version section to stdout
    python scripts/version.py growth [--base <ref>]  # insert:delete growth ratio since a baseline (advisory)

``set`` writes two files: it bumps the version in ``CMakeLists.txt`` *and* checks that
``CHANGELOG.md`` already carries a dated ``## [X.Y.Z] - YYYY-MM-DD`` section for the new
version with at least one bullet, then adds the matching ``[X.Y.Z]: .../compare/...``
link definition. The section itself is written by hand, from the mechanical PR
enumeration in CHANGELOG.md's "Sourcing" rule, *before* ``set`` runs — the file has no
accumulator section to cut from (see the "Release Process" in CONTRIBUTING.md).
Everything is computed and validated in memory first, so a failed check leaves both
files untouched rather than half-updated.

``check`` verifies two independent things — that ``CMakeLists.txt`` agrees with the tag,
and that ``CHANGELOG.md`` has a section for that version. Both are always evaluated and
reported before the command exits, so one CI run surfaces both problems at once.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CMAKELISTS = REPO_ROOT / "CMakeLists.txt"
CHANGELOG = REPO_ROOT / "CHANGELOG.md"
CMAKE_VERSION_RE = re.compile(r"(project\s*\(\s*Lumice\s+VERSION\s+)(\d+\.\d+\.\d+)(\s*\))")
SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")

# CHANGELOG.md structure anchors. A section runs from its own "## [...]" heading up to
# the next one, so NEXT_SECTION_RE is deliberately *not* required to be unique: the real
# file has one match per released version.
NEXT_SECTION_RE = r"^## \["
# A released version's heading carries its UTC date. ``[ \t]*`` rather than ``\s*``:
# ``\s`` would eat the newline (and the blank line after it), which silently moves
# where a section body starts.
DATED_HEADING_RE = re.compile(r"^## \[(\d+\.\d+\.\d+)\] - \d{4}-\d{2}-\d{2}[ \t]*$", re.MULTILINE)
# One "[X.Y.Z]: <base>/compare/vPREV...vX.Y.Z" link definition. The file lists them
# newest-first, so the first match is the latest released version — which is how ``set``
# learns the previous version without a git tag lookup (the new tag does not exist yet).
VERSION_LINK_RE = re.compile(
    r"^\[(\d+\.\d+\.\d+)\]: (https://\S+)/compare/v(\S+)\.\.\.v(\S+)$", re.MULTILINE
)

# The insert:delete ratio above which the release chore must name where the growth went
# (capability / tests / data assets) and whether a subtraction candidate exists. Advisory
# only — the census verdict was that this is a weak signal, too weak to gate a release.
GROWTH_REVIEW_RATIO_THRESHOLD = 20
# One ``git diff --shortstat`` line. The insertion and deletion clauses are each optional
# (a diff can change only files, or only add, or only delete).
SHORTSTAT_RE = re.compile(
    r"^(?P<files>\d+) files? changed"
    r"(?:, (?P<ins>\d+) insertions?\(\+\))?"
    r"(?:, (?P<del>\d+) deletions?\(-\))?[ \t]*$"
)


def version_heading_re(version: str) -> str:
    """Regex matching the CHANGELOG heading of one released version.

    Single owner of "what a version heading looks like", shared by section slicing
    (``extract-notes``, ``set``) and by the existence check (``check``).
    """
    return rf"^## \[{re.escape(version)}\]"


def read_cmake_version() -> str:
    """Read version from CMakeLists.txt."""
    text = CMAKELISTS.read_text(encoding="utf-8")
    m = CMAKE_VERSION_RE.search(text)
    if not m:
        print(f"Error: cannot extract version from {CMAKELISTS}", file=sys.stderr)
        print("Expected pattern: project(Lumice VERSION X.Y.Z)", file=sys.stderr)
        sys.exit(1)
    return m.group(2)


def render_cmake_version(text: str, version: str) -> str:
    """Return CMakeLists.txt text with the version replaced. Exits if count != 1."""
    new_text, count = CMAKE_VERSION_RE.subn(rf"\g<1>{version}\g<3>", text)
    if count != 1:
        print(f"Error: expected exactly 1 replacement, got {count}", file=sys.stderr)
        sys.exit(1)
    return new_text


def read_changelog_text() -> str:
    """Read CHANGELOG.md."""
    return CHANGELOG.read_text(encoding="utf-8")


def find_section(text: str, heading_re: str, label: str) -> tuple[int, int, str]:
    """Slice one "## [...]" section out of CHANGELOG.md.

    Returns ``(start, end, body)`` where ``start`` is the offset of the heading line,
    ``end`` the offset of the next section heading (or end of text for the last
    section), and ``body`` everything between the heading line and ``end``.

    ``heading_re`` must match exactly once in the whole file; ``NEXT_SECTION_RE`` is
    searched only after it and the first hit wins, since it matches every released
    version. No match after the heading is not an error — the section simply runs to
    the end of the file.
    """
    matches = list(re.finditer(heading_re, text, re.MULTILINE))
    if len(matches) != 1:
        print(
            f"Error: expected exactly 1 match for section {label}, got {len(matches)}",
            file=sys.stderr,
        )
        sys.exit(1)
    start = matches[0].start()
    line_end = text.find("\n", matches[0].end())
    body_start = len(text) if line_end < 0 else line_end + 1
    nxt = re.search(NEXT_SECTION_RE, text[body_start:], re.MULTILINE)
    end = body_start + nxt.start() if nxt else len(text)
    return start, end, text[body_start:end]


def changelog_has_bullets(body: str) -> bool:
    """Whether a section body carries at least one bullet."""
    return re.search(r"^- ", body, re.MULTILINE) is not None


def changelog_has_version(text: str, version: str) -> bool:
    """Whether CHANGELOG.md has a section for ``version``."""
    return re.search(version_heading_re(version), text, re.MULTILINE) is not None


def semver_key(version: str) -> tuple[int, int, int]:
    """Sort key for an ``X.Y.Z`` string."""
    major, minor, patch = version.split(".")
    return int(major), int(minor), int(patch)


def latest_version_link(text: str) -> tuple[str, str, int]:
    """Return ``(version, base_url, offset)`` of the topmost version link definition.

    CHANGELOG.md keeps its link definitions newest-first, so the first ``[X.Y.Z]:`` line
    names the latest released version. ``offset`` is where that line starts, which is
    where the next version's definition is inserted. The newest-first order is checked,
    not assumed: a hand edit that moved an older definition to the top would otherwise
    make ``set`` derive the wrong previous version and write a compare link spanning the
    wrong range, silently.
    """
    matches = list(VERSION_LINK_RE.finditer(text))
    if not matches:
        print("Error: CHANGELOG.md has no [X.Y.Z]: .../compare/... link definition", file=sys.stderr)
        sys.exit(1)
    top = matches[0]
    newest = max(matches, key=lambda m: semver_key(m.group(1)))
    if newest is not top:
        print(
            f"Error: the topmost link definition is [{top.group(1)}] but [{newest.group(1)}] "
            "is newer — CHANGELOG.md's link definitions must be ordered newest-first",
            file=sys.stderr,
        )
        sys.exit(1)
    return top.group(1), top.group(2), top.start()


def validate_changelog(text: str, new_version: str, allow_empty: bool) -> str:
    """Check the hand-written section for ``new_version`` and add its link definition, in memory.

    The section must exist exactly once, its heading must carry a date
    (``## [X.Y.Z] - YYYY-MM-DD``), and it must hold at least one bullet unless
    ``allow_empty`` says this release genuinely has no user-perceptible change.
    """
    start, end, body = find_section(text, version_heading_re(new_version), f"[{new_version}]")
    line_end = text.find("\n", start)
    heading = text[start:] if line_end < 0 else text[start:line_end]
    if not DATED_HEADING_RE.match(heading):
        print(
            f"Error: CHANGELOG.md heading {heading!r} lacks a date — expected "
            f"'## [{new_version}] - YYYY-MM-DD'",
            file=sys.stderr,
        )
        sys.exit(1)
    if not allow_empty and not changelog_has_bullets(body):
        print(
            f"Error: the [{new_version}] section is empty — write the user-perceptible "
            "changes first, or pass --allow-empty-changelog to confirm this release has none",
            file=sys.stderr,
        )
        sys.exit(1)

    if re.search(rf"^\[{re.escape(new_version)}\]: ", text, re.MULTILINE):
        print(
            f"Error: CHANGELOG.md already has a link definition for [{new_version}]",
            file=sys.stderr,
        )
        sys.exit(1)
    prev_version, base_url, offset = latest_version_link(text)
    if semver_key(new_version) <= semver_key(prev_version):
        print(
            f"Error: {new_version} is not newer than the latest released version {prev_version}",
            file=sys.stderr,
        )
        sys.exit(1)
    new_line = f"[{new_version}]: {base_url}/compare/v{prev_version}...v{new_version}\n"
    return text[:offset] + new_line + text[offset:]


def read_latest_tag() -> str:
    """Read the latest reachable git tag, verbatim (with its 'v' prefix).

    Uses ``git describe --tags --abbrev=0``, which returns the most recent tag
    reachable from HEAD. This may differ from the chronologically newest tag
    if the current branch has diverged.
    """
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0"],
            stderr=subprocess.PIPE,
            text=True,
        ).strip()
    except subprocess.CalledProcessError:
        print("Error: no git tags found", file=sys.stderr)
        sys.exit(1)


def read_git_tag_version() -> str:
    """Read version from the latest reachable git tag (strips 'v' prefix)."""
    return strip_tag_prefix(read_latest_tag())


def resolve_base_ref(explicit: str | None, latest_tag: str) -> str:
    """Pick the diff baseline for ``growth``: an explicit ``--base``, else the latest tag.

    An explicit ref is used verbatim — branch names and SHAs pass through — except that a
    bare ``X.Y.Z`` version is accepted as shorthand for its ``v``-prefixed tag, since the
    version number is the form the rest of this script and the changelog speak in.
    """
    if explicit is None:
        return latest_tag
    if SEMVER_RE.match(explicit):
        return f"v{explicit}"
    return explicit


def parse_shortstat(text: str) -> tuple[int, int, int]:
    """Parse ``git diff --shortstat`` output into ``(files, insertions, deletions)``.

    Empty output (no differences) parses as ``(0, 0, 0)``; a clause git omits
    (all-insertions, all-deletions, changed-files-only) reads as 0.
    """
    text = text.strip()
    if not text:
        return 0, 0, 0
    m = SHORTSTAT_RE.match(text)
    if not m:
        raise ValueError(f"unrecognized --shortstat output: {text!r}")
    return int(m.group("files")), int(m.group("ins") or 0), int(m.group("del") or 0)


def growth_ratio(insertions: int, deletions: int) -> float | None:
    """The insertions:deletions ratio; ``None`` when nothing was deleted (unbounded)."""
    if deletions == 0:
        return None
    return insertions / deletions


def above_growth_threshold(ratio: float | None) -> bool:
    """Whether a ratio crosses the review threshold. An unbounded ratio (no deletions) does."""
    return ratio is None or ratio > GROWTH_REVIEW_RATIO_THRESHOLD


def format_ratio(ratio: float | None) -> str:
    """One-decimal ratio text; ``∞`` for the no-deletions case."""
    return "∞" if ratio is None else f"{ratio:.1f}"


def strip_tag_prefix(tag: str) -> str:
    """Strip 'v' prefix and pre-release suffix (e.g. '-alpha', '-rc1') from a tag string."""
    ver = tag.removeprefix("v")
    # Strip pre-release suffix: "4.1.7-alpha" → "4.1.7"
    dash = ver.find("-")
    if dash >= 0:
        ver = ver[:dash]
    return ver


def cmd_check(args: argparse.Namespace) -> None:
    """Check that CMakeLists.txt and CHANGELOG.md both agree with the git tag."""
    cmake_ver = read_cmake_version()

    if args.tag:
        tag_ver = strip_tag_prefix(args.tag)
        source = f"--tag {args.tag}"
    else:
        tag_ver = read_git_tag_version()
        source = f"git describe (v{tag_ver})"

    # Both checks always run: a release blocked on one of them should not hide the other.
    failed = False

    if cmake_ver == tag_ver:
        print(f"OK: CMakeLists.txt version ({cmake_ver}) matches {source}")
    else:
        print(f"MISMATCH: CMakeLists.txt version ({cmake_ver}) != {source} ({tag_ver})", file=sys.stderr)
        failed = True

    if changelog_has_version(read_changelog_text(), tag_ver):
        print(f"OK: CHANGELOG.md has a section for {tag_ver}")
    else:
        print(f"MISMATCH: CHANGELOG.md has no section for {tag_ver}", file=sys.stderr)
        failed = True

    if failed:
        sys.exit(1)


def cmd_set(args: argparse.Namespace) -> None:
    """Bump the version in CMakeLists.txt and validate the CHANGELOG section written for it."""
    version = args.version
    if not SEMVER_RE.match(version):
        print(f"Error: '{version}' is not a valid semver (expected X.Y.Z)", file=sys.stderr)
        sys.exit(1)

    # Compute and validate both files before writing either, so a rejected CHANGELOG
    # never leaves CMakeLists.txt already bumped.
    old_ver = read_cmake_version()
    new_cmake = render_cmake_version(CMAKELISTS.read_text(encoding="utf-8"), version)
    new_changelog = validate_changelog(read_changelog_text(), version, args.allow_empty_changelog)

    CMAKELISTS.write_text(new_cmake, encoding="utf-8")
    CHANGELOG.write_text(new_changelog, encoding="utf-8")

    print(f"Updated CMakeLists.txt version: {old_ver} -> {version}")
    print(f"Validated CHANGELOG.md section [{version}] and added its link definition")


def cmd_extract_notes(args: argparse.Namespace) -> None:
    """Print one CHANGELOG version section body (without its heading) to stdout."""
    version = strip_tag_prefix(args.version)
    _, _, body = find_section(
        read_changelog_text(), version_heading_re(version), f"[{version}]"
    )
    print(body.strip())


def cmd_growth(args: argparse.Namespace) -> None:
    """Report the insert:delete growth ratio since a baseline ref. Advisory only, never blocks."""
    base = resolve_base_ref(args.base, read_latest_tag())
    proc = subprocess.run(
        ["git", "diff", "--shortstat", f"{base}..HEAD"],
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"Error: git diff {base}..HEAD failed:\n{proc.stderr.strip()}", file=sys.stderr)
        sys.exit(1)
    files, ins, dels = parse_shortstat(proc.stdout)
    if files == 0 and ins == 0 and dels == 0:
        print(f"Growth {base}..HEAD: no differences")
        return
    ratio = growth_ratio(ins, dels)
    verdict = "ABOVE" if above_growth_threshold(ratio) else "within"
    ratio_text = "∞ (no deletions)" if ratio is None else f"{format_ratio(ratio)}:1"
    print(f"Growth {base}..HEAD: {files} files changed, {ins} insertions(+), {dels} deletions(-)")
    print(f"Ratio: {ratio_text} — {verdict} the {GROWTH_REVIEW_RATIO_THRESHOLD}:1 review threshold")
    if verdict == "ABOVE":
        print(
            "Advisory only (this does not block): name where the growth went — capability, "
            "tests, or data assets — and whether a subtraction candidate exists."
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="Lumice version management")
    sub = parser.add_subparsers(dest="command", required=True)

    check_parser = sub.add_parser("check", help="Check version consistency")
    check_parser.add_argument("--tag", help="Compare against this tag (CI mode, e.g. v4.0.0)")

    set_parser = sub.add_parser(
        "set", help="Set version in CMakeLists.txt and validate its CHANGELOG.md section"
    )
    set_parser.add_argument("version", help="Version to set (X.Y.Z)")
    set_parser.add_argument(
        "--allow-empty-changelog",
        action="store_true",
        help="Accept a version section with no bullets (only for a release with no user-perceptible changes)",
    )

    notes_parser = sub.add_parser("extract-notes", help="Print one CHANGELOG.md version section")
    notes_parser.add_argument("version", help="Version or tag to extract (X.Y.Z or vX.Y.Z)")

    growth_parser = sub.add_parser(
        "growth",
        help="Insert:delete growth ratio since a baseline (advisory, never blocks)",
    )
    growth_parser.add_argument(
        "--base",
        help="Baseline ref (default: the latest tag); a bare X.Y.Z is shorthand for its v-tag",
    )

    args = parser.parse_args()
    if args.command == "check":
        cmd_check(args)
    elif args.command == "set":
        cmd_set(args)
    elif args.command == "extract-notes":
        cmd_extract_notes(args)
    elif args.command == "growth":
        cmd_growth(args)


if __name__ == "__main__":
    main()
