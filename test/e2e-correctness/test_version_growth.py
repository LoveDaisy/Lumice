"""Unit tests for scripts/version.py's ``growth`` subcommand helpers.

The release chore's insert:delete growth report rests on three pure functions:
``git diff --shortstat`` parsing, the ratio arithmetic (including the unbounded
no-deletions case), and baseline-ref resolution. These cover them directly; the
git call itself is exercised by the chore's live first use, not here.
"""

import importlib.util
import unittest
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("version", _REPO_ROOT / "scripts" / "version.py")
version = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(version)


class ParseShortstatTest(unittest.TestCase):
    def test_full_form(self):
        self.assertEqual(
            version.parse_shortstat(" 27 files changed, 1234 insertions(+), 567 deletions(-)\n"),
            (27, 1234, 567),
        )

    def test_singular_nouns(self):
        self.assertEqual(
            version.parse_shortstat(" 1 file changed, 2 insertions(+), 1 deletion(-)"),
            (1, 2, 1),
        )

    def test_insertions_only(self):
        self.assertEqual(version.parse_shortstat(" 3 files changed, 40 insertions(+)"), (3, 40, 0))

    def test_deletions_only(self):
        self.assertEqual(version.parse_shortstat(" 2 files changed, 7 deletions(-)"), (2, 0, 7))

    def test_changed_files_only(self):
        self.assertEqual(version.parse_shortstat(" 5 files changed"), (5, 0, 0))

    def test_empty_diff(self):
        self.assertEqual(version.parse_shortstat(""), (0, 0, 0))
        self.assertEqual(version.parse_shortstat("\n"), (0, 0, 0))

    def test_unrecognized_output_rejected(self):
        with self.assertRaises(ValueError):
            version.parse_shortstat("fatal: bad revision 'v0.0.0'")


class GrowthRatioTest(unittest.TestCase):
    def test_plain_ratio(self):
        self.assertAlmostEqual(version.growth_ratio(40, 10), 4.0)

    def test_rounds_to_one_decimal_on_render(self):
        self.assertEqual(version.format_ratio(version.growth_ratio(20, 3)), "6.7")

    def test_no_deletions_is_unbounded(self):
        self.assertIsNone(version.growth_ratio(100, 0))
        self.assertEqual(version.format_ratio(None), "∞")

    def test_threshold_verdict(self):
        self.assertFalse(version.above_growth_threshold(20.0))  # at the threshold, not above
        self.assertTrue(version.above_growth_threshold(20.1))
        self.assertFalse(version.above_growth_threshold(0.5))
        # No deletions at all is the most lopsided case there is.
        self.assertTrue(version.above_growth_threshold(None))


class ResolveBaseRefTest(unittest.TestCase):
    def test_default_is_latest_tag(self):
        self.assertEqual(version.resolve_base_ref(None, "v4.7.3"), "v4.7.3")

    def test_bare_version_gains_v_prefix(self):
        self.assertEqual(version.resolve_base_ref("4.7.2", "v4.7.3"), "v4.7.2")

    def test_tag_passes_through_verbatim(self):
        self.assertEqual(version.resolve_base_ref("v4.7.2", "v4.7.3"), "v4.7.2")

    def test_branch_and_sha_pass_through_verbatim(self):
        self.assertEqual(version.resolve_base_ref("main", "v4.7.3"), "main")
        self.assertEqual(version.resolve_base_ref("4f5f873d", "v4.7.3"), "4f5f873d")


if __name__ == "__main__":
    unittest.main()
