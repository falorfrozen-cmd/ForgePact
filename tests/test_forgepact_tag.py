"""Tests for the tag ForgePact is being cut as, and the notes composed for it.

`tools/forgepact_tag.py` carries why each refusal exists and why the
composition rules are shaped the way they are. What is pinned here is that
each refusal actually fires and each composition rule actually holds, because
everything downstream trusts both: the tree is rewritten to match the tag, the
tag is pushed, and the body this writes becomes what players read on the draft
release before anyone rewrites it.

This mirrors the hub's `tests/test_hub_tag.py` where the shapes match (the `v`
prefix, the taken/downgrade/malformed refusals, the ragged numeric ordering),
and adds what has no hub counterpart: the "below the tree" refusal (ForgePact
did not always tag the newest code, so the highest tag and the tree version
can disagree) and the notes-composition rules.
"""

import subprocess
import sys
import tempfile
import unittest
from datetime import date, datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import forgepact_tag  # noqa: E402
import cut_release  # noqa: E402


REPO = Path(__file__).resolve().parents[1]

# Ragged on purpose, and mixed with tags belonging to other schemes: this
# repository's real tag list is a `v1.0.1 ... v1.3.16` sequence with gaps, and
# the foreign names below (`hub-v9.0.0`, `catalog`, `v2.0.0-rc1`) must not
# block or cap anything they don't actually shape-match.
TAGS = ["v1.2.2", "v1.3.9", "v1.3.10", "v1.3.16", "hub-v9.0.0", "catalog", "v2.0.0-rc1"]


class EitherSpellingIsAccepted(unittest.TestCase):
    def test_the_full_tag(self):
        got = forgepact_tag.plan("v1.3.21", TAGS, tree="1.3.16")
        self.assertEqual(got.tag, "v1.3.21")

    def test_the_bare_version(self):
        got = forgepact_tag.plan("1.3.21", TAGS, tree="1.3.16")
        self.assertEqual(got.tag, "v1.3.21")


class ATakenTagIsRefused(unittest.TestCase):
    def test_an_existing_tag(self):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.plan("v1.3.16", TAGS, tree="1.3.16")
        self.assertIn("v1.3.16", str(caught.exception))

    def test_the_bare_spelling_of_an_existing_tag(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.16", TAGS, tree="1.3.16")

    def test_a_peeled_ref_as_git_prints_it_counts_as_taken(self):
        refs = ["refs/tags/v1.3.16", "refs/tags/v1.3.16^{}"]
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.16", refs, tree="1.3.16")

    def test_the_refusal_names_the_recut_input(self):
        # Someone re-running for a draft they meant to replace has to be told
        # the switch exists, rather than to delete the tag by hand.
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.plan("v1.3.16", TAGS, tree="1.3.16")
        self.assertIn("recut", str(caught.exception))


class ARecutMayReuseItsOwnTag(unittest.TestCase):
    """`recut=True` lifts only the "already exists" refusal, for the tag itself.

    Whether that tag carries nothing but an unpublished draft is the
    workflow's check, not this tool's (it cannot see releases); every other
    refusal still stands, and the tag being recut must never count as its
    own `previous`, or the notes would be composed from an empty range.
    """

    def test_the_existing_tag_is_accepted(self):
        got = forgepact_tag.plan("v1.3.16", TAGS, tree="1.3.16", recut=True)
        self.assertEqual(got.tag, "v1.3.16")
        self.assertEqual(got.version, "1.3.16")

    def test_a_peeled_ref_of_the_tag_is_accepted_too(self):
        refs = ["refs/tags/v1.3.10", "refs/tags/v1.3.16", "refs/tags/v1.3.16^{}"]
        got = forgepact_tag.plan("1.3.16", refs, tree="1.3.16", recut=True)
        self.assertEqual(got.previous, "v1.3.10")

    def test_previous_is_the_tag_below_never_the_tag_itself(self):
        got = forgepact_tag.plan("1.3.16", TAGS, tree="1.3.16", recut=True)
        self.assertEqual(got.previous, "v1.3.10")

    def test_main_already_at_the_version_needs_no_bump(self):
        # The ordinary recut: the first cut already moved main to this
        # version, so the bump step is skipped and nothing is committed.
        got = forgepact_tag.plan("1.3.16", TAGS, tree="1.3.16", recut=True)
        self.assertFalse(got.bump)

    def test_a_higher_tag_still_refuses(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.10", TAGS, tree="1.3.10", recut=True)

    def test_below_the_tree_still_refuses(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.16", TAGS, tree="1.3.20", recut=True)

    def test_a_malformed_tag_still_refuses(self):
        for raw in ("vv1.3.16", "V1.3.16", "1.3.16-rc1"):
            with self.subTest(raw=raw), self.assertRaises(SystemExit):
                forgepact_tag.plan(raw, TAGS, tree="1.3.16", recut=True)

    def test_a_version_with_no_tag_is_an_ordinary_cut(self):
        # A draft on a never-pushed tag, or a rerun after a recut that deleted
        # the old tag and then failed: nothing to reuse, same answer as without.
        self.assertEqual(
            forgepact_tag.plan("1.3.21", TAGS, tree="1.3.16", recut=True),
            forgepact_tag.plan("1.3.21", TAGS, tree="1.3.16"),
        )

    def test_recut_is_off_by_default(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.16", TAGS, tree="1.3.16")


class GoingBackwardsAgainstTagsIsRefused(unittest.TestCase):
    def test_below_the_highest_tag_numerically(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.plan("1.3.10", ["v1.3.9", "v1.3.10", "v1.3.16"], tree="1.3.9")

    def test_foreign_tags_do_not_block_or_cap_a_higher_version(self):
        got = forgepact_tag.plan("2.0.0", TAGS, tree="1.3.16")
        self.assertEqual(got.tag, "v2.0.0")


class BelowTheTreeIsRefused(unittest.TestCase):
    """`main` can hold a version above the highest tag; tagging behind it relabels code."""

    def test_above_the_highest_tag_but_below_the_tree(self):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.plan("1.3.17", ["v1.3.16"], tree="1.3.20")
        self.assertIn("1.3.20", str(caught.exception))

    def test_equal_to_the_tree_gives_no_bump(self):
        got = forgepact_tag.plan("1.3.20", ["v1.3.16"], tree="1.3.20")
        self.assertFalse(got.bump)

    def test_above_the_tree_gives_a_bump(self):
        got = forgepact_tag.plan("1.3.21", ["v1.3.16"], tree="1.3.20")
        self.assertTrue(got.bump)


class PreviousIsTheHighestReleasedTagBelowTheVersion(unittest.TestCase):
    def test_the_highest_tag_among_several(self):
        got = forgepact_tag.plan(
            "1.3.21",
            ["v1.3.9", "v1.3.10", "v1.3.16", "hub-v9.0.0", "v2.0.0-rc1"],
            tree="1.3.16",
        )
        self.assertEqual(got.previous, "v1.3.16")

    def test_numeric_not_lexical(self):
        got = forgepact_tag.plan("1.3.11", ["v1.3.9", "v1.3.10"], tree="1.3.10")
        self.assertEqual(got.previous, "v1.3.10")

    def test_no_previous_tag_at_all(self):
        got = forgepact_tag.plan("1.0.0", [], tree="1.0.0")
        self.assertEqual(got.previous, "")


class MalformedInputIsRefused(unittest.TestCase):
    def test_shapes_that_are_not_a_bare_or_v_prefixed_version(self):
        for bad in (
            "hub-v1.3.21",
            "vv1.3.21",
            "V1.3.21",
            "1.3",
            "1.3.21.0",
            "1.3.21-rc1",
            "",
            "1.3.21; rm -rf /",
            "$(whoami)",
        ):
            with self.assertRaises(SystemExit, msg=f"{bad!r} was accepted"):
                forgepact_tag.plan(bad, TAGS, tree="1.3.16")

    def test_leading_zeros_are_refused(self):
        for bad in ("01.3.21", "v1.03.21"):
            with self.assertRaises(SystemExit, msg=f"{bad!r} was accepted"):
                forgepact_tag.plan(bad, TAGS, tree="1.3.16")

    # Written as escapes so this file stays ASCII.
    UNICODE_DIGITS = (
        "1.3.21٣",  # Arabic-Indic three, trailing
        "١.3.21",  # Arabic-Indic one, leading
        "1.३.21",  # Devanagari three
        "１.3.21",  # fullwidth one
    )

    def test_unicode_digits_are_refused(self):
        for bad in self.UNICODE_DIGITS:
            with self.assertRaises(SystemExit, msg=f"{bad!r} was accepted"):
                forgepact_tag.plan(bad, TAGS, tree="1.3.16")

    def test_the_leading_zero_hint_appears_only_for_ascii_zeros(self):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.plan("01.3.21", TAGS, tree="1.3.16")
        self.assertIn("leading zero", str(caught.exception))

        for bad in self.UNICODE_DIGITS:
            with self.assertRaises(SystemExit) as caught:
                forgepact_tag.plan(bad, TAGS, tree="1.3.16")
            self.assertNotIn("leading zero", str(caught.exception))

    def test_cut_release_agrees_with_the_gate_on_leading_zeros(self):
        for bad in ("01.3.21", "1.03.21"):
            self.assertFalse(cut_release.VERSION.match(bad), bad)
        for bad in self.UNICODE_DIGITS:
            self.assertFalse(cut_release.VERSION.match(bad), bad)


class TheCliPrintsWhatAWorkflowReads(unittest.TestCase):
    def run_it(self, *args, expect=0):
        done = subprocess.run(
            [sys.executable, str(REPO / "tools/forgepact_tag.py"), *args],
            capture_output=True,
            text=True,
        )
        self.assertEqual(done.returncode, expect, done.stdout + done.stderr)
        return done

    def test_it_prints_exactly_four_lines_in_order(self):
        out = self.run_it(
            "--tag", "1.3.21", "--tree", "1.3.16", "--existing", *TAGS
        ).stdout
        lines = out.strip("\n").splitlines()
        self.assertEqual(len(lines), 4, out)
        keys = [line.split("=", 1)[0] for line in lines]
        self.assertEqual(keys, ["version", "tag", "bump", "previous"])

    def test_bump_is_false_when_the_tree_matches(self):
        out = self.run_it(
            "--tag", "1.3.16", "--tree", "1.3.16", "--existing", "v1.3.9"
        ).stdout
        pairs = dict(line.split("=", 1) for line in out.strip().splitlines())
        self.assertEqual(pairs["bump"], "false")

    def test_recut_prints_the_same_four_lines_for_an_existing_tag(self):
        out = self.run_it(
            "--tag", "1.3.16", "--recut", "--tree", "1.3.16", "--existing", *TAGS
        ).stdout
        pairs = dict(line.split("=", 1) for line in out.strip().splitlines())
        self.assertEqual(list(pairs), ["version", "tag", "bump", "previous"])
        self.assertEqual(pairs["tag"], "v1.3.16")
        self.assertEqual(pairs["bump"], "false")
        self.assertEqual(pairs["previous"], "v1.3.10")

    def test_without_recut_an_existing_tag_exits_nonzero_with_no_pairs(self):
        done = self.run_it(
            "--tag", "1.3.16", "--tree", "1.3.16", "--existing", *TAGS, expect=1
        )
        self.assertNotIn("version=", done.stdout)
        self.assertIn("recut", done.stderr)

    def test_recut_belongs_to_plan_mode_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            generated = Path(tmp) / "generated.md"
            generated.write_text("x", encoding="utf-8")
            self.run_it(
                "--compose-notes", "--recut", "--version", "1.3.16",
                "--generated", str(generated), "--out", str(Path(tmp) / "out.md"),
                "--root", tmp,
                expect=2,
            )

    def test_a_refusal_exits_nonzero_with_no_pairs_and_a_message(self):
        done = self.run_it(
            "--tag", "nonsense", "--tree", "1.3.16", expect=1
        )
        self.assertNotIn("version=", done.stdout)
        self.assertTrue(done.stderr.strip(), "a refusal must say why")

    def test_plan_mode_with_a_root_holding_no_notes_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            done = self.run_it(
                "--tag", "1.3.16", "--tree", "1.3.16", "--existing", "v1.3.9",
                "--root", str(root),
                expect=0,
            )
            self.assertIn("version=1.3.16", done.stdout)


# 2026-10-02 is a Friday; the days around it name themselves.
THURSDAY = date(2026, 10, 1)
FRIDAY = date(2026, 10, 2)
SATURDAY = date(2026, 10, 3)
NEXT_FRIDAY = date(2026, 10, 9)
DATED_FRIDAY = "# ForgePact 2.1.0\n\nRelease date: 2026-10-02\n"


class FridayReleasesMoveOneStep(unittest.TestCase):
    def refused(self, version, previous="v2.0.0"):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.schedule(version, previous, DATED_FRIDAY, FRIDAY)
        return str(caught.exception)

    def test_one_minor_or_one_major_step_is_accepted(self):
        for version in ("2.1.0", "3.0.0"):
            with self.subTest(version=version):
                forgepact_tag.schedule(version, "v2.0.0", DATED_FRIDAY, FRIDAY)

    def test_a_hotfix_keeps_major_and_minor(self):
        for version in ("2.0.1", "2.0.2", "2.0.10"):
            with self.subTest(version=version):
                forgepact_tag.schedule(version, "v2.0.0", None, THURSDAY)

    def test_skipping_a_step_is_refused_and_names_both_allowed_steps(self):
        for version in ("2.2.0", "4.0.0", "3.1.0", "3.0.1", "2.1.1"):
            with self.subTest(version=version):
                message = self.refused(version)
                self.assertIn("2.1.0", message)
                self.assertIn("3.0.0", message)

    def test_with_no_previous_tag_any_step_is_accepted(self):
        forgepact_tag.schedule("5.0.0", "", None, FRIDAY)


class FridayReleasesWaitForTheirDate(unittest.TestCase):
    def test_a_dated_friday_release_is_refused_before_its_date(self):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.schedule("2.1.0", "v2.0.0", DATED_FRIDAY, THURSDAY)
        self.assertIn("2026-10-02", str(caught.exception))
        self.assertIn("Thursday", str(caught.exception))

    def test_it_is_accepted_on_its_date_and_any_day_after(self):
        for today in (FRIDAY, SATURDAY, NEXT_FRIDAY):
            with self.subTest(today=today):
                forgepact_tag.schedule("2.1.0", "v2.0.0", DATED_FRIDAY, today)

    def test_an_undated_notes_file_is_refused_even_on_a_friday(self):
        with self.assertRaises(SystemExit) as caught:
            forgepact_tag.schedule("2.1.0", "v2.0.0", "# ForgePact 2.1.0\n", FRIDAY)
        self.assertIn("Release date: ", str(caught.exception))

    def test_a_notes_date_that_is_not_a_friday_is_refused(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.schedule(
                "2.1.0", "v2.0.0", "Release date: 2026-10-01\n", SATURDAY)

    def test_with_no_notes_file_only_a_friday_is_accepted(self):
        forgepact_tag.schedule("2.1.0", "v2.0.0", None, FRIDAY)
        for today in (THURSDAY, SATURDAY):
            with self.subTest(today=today):
                with self.assertRaises(SystemExit):
                    forgepact_tag.schedule("2.1.0", "v2.0.0", None, today)

    def test_a_hotfix_ships_before_its_own_date(self):
        forgepact_tag.schedule(
            "2.0.1", "v2.0.0", "Release date: 2026-10-05\n", THURSDAY)

    def test_an_undated_hotfix_notes_file_is_accepted_any_day(self):
        for today in (THURSDAY, FRIDAY, SATURDAY):
            with self.subTest(today=today):
                forgepact_tag.schedule("2.0.1", "v2.0.0", "# ForgePact 2.0.1\n", today)

    def test_a_hotfix_date_that_is_not_real_is_still_refused(self):
        with self.assertRaises(SystemExit):
            forgepact_tag.schedule(
                "2.0.1", "v2.0.0", "Release date: 2026-02-30\n", THURSDAY)


class TheReleaseClockIsTurkeys(unittest.TestCase):
    def test_friday_starts_at_midnight_utc_plus_three(self):
        # 21:00 UTC on Thursday is 23:00 in Poland (CEST) and already Friday
        # 00:00 in Turkey: the release may go out.
        now = datetime(2026, 10, 1, 21, 0, tzinfo=timezone.utc)
        self.assertEqual(forgepact_tag.release_today(now), FRIDAY)
        a_minute_earlier = datetime(2026, 10, 1, 20, 59, tzinfo=timezone.utc)
        self.assertEqual(forgepact_tag.release_today(a_minute_earlier), THURSDAY)


class TheCliChecksTheSchedule(unittest.TestCase):
    def run_it(self, root, today, expect):
        done = subprocess.run(
            [sys.executable, str(REPO / "tools/forgepact_tag.py"),
             "--tag", "2.1.0", "--tree", "2.0.0", "--existing", "v2.0.0",
             "--root", str(root), "--today", today],
            capture_output=True,
            text=True,
        )
        self.assertEqual(done.returncode, expect, done.stdout + done.stderr)
        return done

    def test_the_notes_file_under_root_dates_the_release(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "release-notes-v2.1.0.md").write_text(DATED_FRIDAY, encoding="utf-8")
            early = self.run_it(root, "2026-10-01", expect=1)
            self.assertNotIn("version=", early.stdout)
            self.assertIn("2026-10-02", early.stderr)
            self.assertIn("version=2.1.0", self.run_it(root, "2026-10-03", expect=0).stdout)

    def test_today_belongs_to_plan_mode_only(self):
        done = subprocess.run(
            [sys.executable, str(REPO / "tools/forgepact_tag.py"),
             "--published-notes", "--version", "2.0.0", "--today", "2026-10-02"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)


class NoteVersionsPicksOutAcceptedFilenames(unittest.TestCase):
    def test_it_matches_only_the_exact_shape(self):
        names = [
            "release-notes-v1.3.9.md",
            "release-notes-v1.3.10.md",
            "release-notes-v1.3.20.md",
            "release-notes-v1.3.21-rc1.md",
            "release-notes-v01.3.2.md",
            "release-notes-1.3.2.md",
            "README.md",
        ]
        self.assertEqual(
            set(forgepact_tag.note_versions(names)), {"1.3.9", "1.3.10", "1.3.20"}
        )


class NotesPlanOrderingAndBounds(unittest.TestCase):
    def test_ordering_and_bounds(self):
        available = [
            "1.3.9", "1.3.10", "1.3.16", "1.3.17", "1.3.18", "1.3.19", "1.3.20", "1.3.21",
        ]
        got = forgepact_tag.notes_plan(available, "1.3.20", "v1.3.16")
        self.assertTrue(got.top_from_file)
        self.assertEqual(got.skipped, ["1.3.19", "1.3.18", "1.3.17"])

    def test_numeric_order_across_a_ragged_boundary(self):
        got = forgepact_tag.notes_plan(["1.3.8", "1.3.9", "1.3.10"], "1.3.10", "v1.3.8")
        self.assertEqual(got.skipped, ["1.3.9"])

        got2 = forgepact_tag.notes_plan(["1.3.9", "1.3.10", "1.3.11"], "1.3.12", "v1.3.8")
        self.assertEqual(got2.skipped, ["1.3.11", "1.3.10", "1.3.9"])

    def test_no_previous_tag_means_no_skipped_versions(self):
        got = forgepact_tag.notes_plan(["1.3.1", "1.3.2"], "1.3.2", "")
        self.assertTrue(got.top_from_file)
        self.assertEqual(got.skipped, [])


def _fixture_notes(version: str, section: str, body: str) -> str:
    return (
        f"# ForgePact {version}\r\n\r\n## {section}\r\n- {body}\r\n\r\n"
        f"## How to update\r\nboilerplate for {version}\r\n"
    )


class ComposeBodyOrderAndHeadings(unittest.TestCase):
    def test_body_order_source_and_the_never_released_line(self):
        top = _fixture_notes("1.3.20", "New", "top thing")
        skipped = [
            ("1.3.19", _fixture_notes("1.3.19", "Fixed", "x")),
            ("1.3.18", _fixture_notes("1.3.18", "New", "y")),
            ("1.3.17", _fixture_notes("1.3.17", "Fixed", "z")),
        ]
        body, source = forgepact_tag.compose_body("1.3.20", top, "", skipped)

        self.assertEqual(source, "files")
        i20 = body.index("# ForgePact 1.3.20")
        i19 = body.index("# ForgePact 1.3.19")
        i18 = body.index("# ForgePact 1.3.18")
        i17 = body.index("# ForgePact 1.3.17")
        self.assertLess(i20, i19)
        self.assertLess(i19, i18)
        self.assertLess(i18, i17)
        self.assertIn(
            "This release also carries the notes for 1.3.19, 1.3.18 and 1.3.17, "
            "which were never released on their own.",
            body,
        )
        self.assertNotIn("Draft notes", body)
        self.assertIn("\n\n---\n\n", body)

    def test_one_how_to_update_after_the_top_and_before_the_first_skipped(self):
        top = _fixture_notes("1.3.20", "New", "top thing")
        skipped = [
            ("1.3.19", _fixture_notes("1.3.19", "Fixed", "x")),
            ("1.3.18", _fixture_notes("1.3.18", "New", "y")),
            ("1.3.17", _fixture_notes("1.3.17", "Fixed", "z")),
        ]
        body, _ = forgepact_tag.compose_body("1.3.20", top, "", skipped)

        self.assertEqual(body.count("## How to update"), 1)
        i20 = body.index("# ForgePact 1.3.20")
        i19 = body.index("# ForgePact 1.3.19")
        htu = body.index("## How to update")
        self.assertLess(i20, htu)
        self.assertLess(htu, i19)


class ComposeBodySingleFileInvariant(unittest.TestCase):
    def test_no_skipped_versions_gives_the_file_byte_identical(self):
        fixture = _fixture_notes("1.3.16", "Fixed", "a crlf fixture")
        body, source = forgepact_tag.compose_body("1.3.16", fixture, "", [])
        self.assertEqual(source, "file")
        expected = fixture.replace("\r\n", "\n").strip() + "\n"
        self.assertEqual(body, expected)


class ComposeBodyMissingTopFile(unittest.TestCase):
    def test_generated_top_plus_skipped_files_is_mixed(self):
        skipped = [
            ("1.3.19", _fixture_notes("1.3.19", "Fixed", "x")),
            ("1.3.18", _fixture_notes("1.3.18", "New", "y")),
        ]
        body, source = forgepact_tag.compose_body(
            "1.3.20", None, "* PR one by @x", skipped
        )

        self.assertEqual(source, "mixed")
        self.assertTrue(body.startswith("> **Draft notes, generated from pull request titles.**"))
        i20 = body.index("# ForgePact 1.3.20")
        igen = body.index("* PR one by @x")
        i19 = body.index("# ForgePact 1.3.19")
        self.assertLess(i20, igen)
        self.assertLess(igen, i19)
        self.assertEqual(body.count("## How to update"), 1)
        htu = body.index("## How to update")
        i18 = body.index("# ForgePact 1.3.18")
        self.assertLess(i19, htu)
        self.assertLess(htu, i18)


class ComposeBodyGeneratedOnly(unittest.TestCase):
    def test_no_skipped_versions_at_all(self):
        body, source = forgepact_tag.compose_body("1.3.20", None, "* PR one by @x", [])
        self.assertEqual(source, "generated")
        self.assertIn("Draft notes", body)
        self.assertNotIn("never released", body)


class ComposeNotesCli(unittest.TestCase):
    def run_it(self, *args, expect=0):
        done = subprocess.run(
            [sys.executable, str(REPO / "tools/forgepact_tag.py"), *args],
            capture_output=True,
            text=True,
        )
        self.assertEqual(done.returncode, expect, done.stdout + done.stderr)
        return done

    def test_it_composes_from_a_root_of_crlf_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for v in ("1.3.16", "1.3.17", "1.3.18", "1.3.19", "1.3.20", "1.3.21"):
                (root / f"release-notes-v{v}.md").write_bytes(
                    _fixture_notes(v, "New", f"thing {v}").encode("utf-8")
                )
            generated = root / "generated.md"
            generated.write_text("* generated\r\n", encoding="utf-8")
            out = root / "body.md"

            done = self.run_it(
                "--compose-notes",
                "--version", "1.3.20",
                "--previous", "v1.3.16",
                "--generated", str(generated),
                "--out", str(out),
                "--root", str(root),
            )
            lines = done.stdout.strip("\n").splitlines()
            self.assertEqual(lines, ["source=files", "versions=1.3.20 1.3.19 1.3.18 1.3.17"])
            self.assertTrue(out.exists())

    def test_a_malformed_version_exits_nonzero_with_empty_stdout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            generated = root / "generated.md"
            generated.write_text("* generated\r\n", encoding="utf-8")
            out = root / "body.md"
            done = self.run_it(
                "--compose-notes",
                "--version", "1.3.03",
                "--previous", "",
                "--generated", str(generated),
                "--out", str(out),
                expect=1,
            )
            self.assertEqual(done.stdout, "")


class PublishedNotesAreEverythingUpToTheVersion(unittest.TestCase):
    def test_it_takes_every_version_at_or_below_numerically(self):
        available = ["1.3.21", "1.3.9", "1.3.20", "1.3.10", "1.3.17", "1.3.16"]
        self.assertEqual(
            forgepact_tag.published_notes(available, "1.3.20"),
            ["1.3.9", "1.3.10", "1.3.16", "1.3.17", "1.3.20"],
        )

    def test_a_version_without_its_own_file_still_takes_the_older_ones(self):
        # A release published from generated notes still carried the skipped
        # versions' files below it.
        self.assertEqual(
            forgepact_tag.published_notes(["1.3.17", "1.3.19", "1.3.21"], "1.3.20"),
            ["1.3.17", "1.3.19"],
        )

    def test_nothing_to_delete_is_an_empty_list(self):
        self.assertEqual(forgepact_tag.published_notes(["1.3.21"], "1.3.20"), [])


class PublishedNotesCli(unittest.TestCase):
    run_it = ComposeNotesCli.run_it

    def test_it_prints_the_filenames_to_delete_and_nothing_else(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("release-notes-v1.3.19.md", "release-notes-v1.3.20.md",
                         "release-notes-v1.3.21.md", "release-notes-v1.3.20-rc1.md",
                         "README.md"):
                (root / name).write_text("x", encoding="utf-8")
            done = self.run_it("--published-notes", "--version", "v1.3.20", "--root", str(root))
            self.assertEqual(
                done.stdout.splitlines(),
                ["release-notes-v1.3.19.md", "release-notes-v1.3.20.md"],
            )

    def test_an_empty_root_prints_nothing_and_succeeds(self):
        with tempfile.TemporaryDirectory() as tmp:
            done = self.run_it("--published-notes", "--version", "1.3.20", "--root", tmp)
            self.assertEqual(done.stdout, "")

    def test_a_malformed_version_exits_nonzero_with_empty_stdout(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "release-notes-v1.3.1.md").write_text("x", encoding="utf-8")
            done = self.run_it("--published-notes", "--version", "1.3", "--root", tmp, expect=1)
            self.assertEqual(done.stdout, "")

    def test_it_cannot_be_combined_with_another_mode(self):
        done = self.run_it("--published-notes", "--compose-notes", "--version", "1.3.20", expect=2)
        self.assertEqual(done.stdout, "")
        done = self.run_it("--published-notes", "--version", "1.3.20", "--tag", "1.3.20", expect=2)
        self.assertEqual(done.stdout, "")


if __name__ == "__main__":
    unittest.main()
