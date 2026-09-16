"""Tests for the workflow that tags a ForgePact release and drafts its notes.

`forgepact-tag.yml` is a `git tag` wrapped in guards, plus a draft release
whose body `tools/forgepact_tag.py` composes -- and the guards are the point:
once a tag is pushed, nothing here is undone from the repository's side. This
mirrors the hub's `tests/test_hub_tag_workflow.py`, minus the dispatch of a
second workflow (ForgePact leaves a draft directly; there is no build half to
dispatch into, see the module guide) and plus the notes-composition and
draft-only assertions this workflow adds.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import forgepact_tag  # noqa: E402


REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO / ".github/workflows/forgepact-tag.yml"


def workflow_text() -> str:
    return WORKFLOW.read_text(encoding="utf-8")


def run_lines(text: str) -> list[str]:
    """Every line inside a `run:` block, which is what a shell will execute."""
    lines = []
    inside = False
    indent = 0
    for line in text.splitlines():
        if re.match(r"^\s*run: \|", line):
            inside = True
            indent = len(line) - len(line.lstrip())
            continue
        if re.match(r"^\s*run: ", line):
            lines.append(line.split("run: ", 1)[1])
            continue
        if inside:
            if line.strip() and (len(line) - len(line.lstrip())) <= indent:
                inside = False
            else:
                lines.append(line)
    return lines


def code_lines(text: str) -> list[str]:
    """`run:` lines a shell will act on, with comments dropped.

    Every check about what the workflow *does* has to read these rather than
    the raw file: a comment mentioning `pipefail` contains the word, so a
    check that greps the step's text would pass whether or not the command is
    actually there.
    """
    return [
        line.strip()
        for line in run_lines(text)
        if line.strip() and not line.strip().startswith("#")
    ]


class TheWorkflowExists(unittest.TestCase):
    def test_it_is_there(self):
        self.assertTrue(WORKFLOW.exists(), f"{WORKFLOW} is missing")


class ItIsRunByHandWithATag(unittest.TestCase):
    def test_only_workflow_dispatch_triggers_it(self):
        text = workflow_text()
        self.assertTrue(re.search(r"(?m)^on:\s*$", text) or "on: workflow_dispatch" in text)
        for trigger in ("push:", "pull_request:", "schedule:", "repository_dispatch:", "release:"):
            self.assertFalse(
                re.search(rf"(?m)^\s{{2}}{re.escape(trigger)}", text),
                f"{trigger} would cut releases nobody asked for",
            )

    def test_it_takes_a_required_tag_input(self):
        text = workflow_text()
        self.assertTrue(
            re.search(r"(?m)^\s{6}tag:\s*$", text),
            "the whole point is typing the tag to release as",
        )
        self.assertTrue(
            re.search(r"(?m)^\s*required: true\s*$", text),
            "an empty tag would tag whatever the tree happens to say",
        )

    def test_it_refuses_to_run_off_main(self):
        self.assertTrue(
            re.search(r'\[ "\$BRANCH" != "main" \]', workflow_text()),
            "a release cut from a feature branch would push that branch to main",
        )


class TheTypedTagIsHandledSafely(unittest.TestCase):
    def test_the_input_is_checked_by_the_tested_tool(self):
        self.assertTrue(
            "tools/forgepact_tag.py" in workflow_text(),
            "the shape, collision, downgrade and below-tree checks belong in the tested tool",
        )

    def test_the_run_block_reader_actually_reads_them(self):
        """Positive control: an instrument that finds nothing has measured nothing."""
        lines = run_lines(workflow_text())
        self.assertGreater(len(lines), 20, "the run blocks are not being read")
        self.assertTrue(
            any("git ls-remote" in line for line in lines),
            "a command known to be in a run block was not found",
        )

    def test_the_input_never_appears_inside_a_run_block(self):
        for line in run_lines(workflow_text()):
            self.assertNotIn(
                "inputs.tag",
                line,
                f"the typed tag is interpolated into a shell here: {line.strip()!r}",
            )

    def test_the_input_is_passed_through_the_environment(self):
        self.assertTrue(
            re.search(r"TAG_INPUT: \$\{\{ inputs\.tag \}\}", workflow_text()),
            "pass the typed tag through env:, not into the command line",
        )

    def test_pipefail_precedes_the_piped_ls_remote(self):
        lines = code_lines(workflow_text())
        piped = [i for i, line in enumerate(lines) if "git ls-remote" in line and "|" in line]
        self.assertTrue(piped, "the tag query is gone, or no longer a pipeline")
        for at in piped:
            self.assertIn(
                "set -o pipefail",
                lines[:at],
                "the query's exit status is discarded by the pipe it is in",
            )
        self.assertTrue(
            any("refs/tags/v*" in line for line in lines),
            "the tag query must be scoped to this repo's v* tags",
        )


class StepOrderIsTheGuardrail(unittest.TestCase):
    def test_read_only_steps_precede_every_write(self):
        text = workflow_text()
        positions = {
            name: text.find(marker)
            for name, marker in [
                ("no_release_yet", "- name: This version has no release yet"),
                ("generate_notes", "releases/generate-notes"),
                ("compose_notes", "--compose-notes"),
                ("move_version", "- name: Move the version to match the tag"),
                ("push_main", "git push origin HEAD:main"),
                ("tag_it", "git tag -a"),
                ("release_create", "gh release create"),
            ]
        }
        for name, pos in positions.items():
            self.assertNotEqual(pos, -1, f"{name} is missing from the workflow")

        ordered = list(positions.items())
        for (name_a, pos_a), (name_b, pos_b) in zip(ordered, ordered[1:]):
            self.assertLess(pos_a, pos_b, f"{name_a} must precede {name_b}")

    def test_the_bump_only_happens_when_the_tree_disagrees(self):
        self.assertTrue(
            re.search(r"if: steps\.plan\.outputs\.bump == 'true'", workflow_text()),
            "rewriting a tree that already matches would commit nothing, noisily",
        )

    def test_the_bump_is_committed_with_every_rewritten_file(self):
        self.assertTrue(
            re.search(r"git commit --all", workflow_text()),
            "cut_release.py writes two files; committing some half-bumps main",
        )

    def test_the_tree_is_verified_with_the_notes_flag(self):
        self.assertTrue(
            re.search(
                r'cut_release\.py --check --expect "\$VERSION" --allow-missing-notes',
                workflow_text(),
            ),
            "notes are never a refusal here; only the tag workflow passes this flag",
        )


class NotesComposition(unittest.TestCase):
    def test_generate_notes_supplies_target_and_previous(self):
        text = workflow_text()
        self.assertIn("target_commitish", text)
        self.assertIn("previous_tag_name", text)

    def test_it_uses_runner_temp_not_a_tracked_file(self):
        self.assertIn("$RUNNER_TEMP", workflow_text())

    def test_the_yaml_never_names_a_release_notes_file_itself(self):
        # Selection is entirely tools/forgepact_tag.py's job; the YAML only
        # calls it, so `release-notes-v` never needs to appear here.
        self.assertNotIn("release-notes-v", workflow_text())


class TheDraftCarriesTheComposedNotes(unittest.TestCase):
    def test_gh_release_create_is_a_draft_against_the_verified_tag(self):
        text = workflow_text()
        self.assertTrue(re.search(r"gh release create[^\n]*--draft", text))
        self.assertTrue(re.search(r"gh release create[^\n]*--verify-tag", text))
        self.assertTrue(re.search(r"gh release create[^\n]*--notes-file", text))


class ThePermissionsCoverWhatItDoes(unittest.TestCase):
    def test_it_may_push_a_commit_a_tag_and_a_release(self):
        self.assertTrue(
            re.search(r"(?m)^\s*contents: write\s*$", workflow_text()),
            "without contents: write the bump, tag and draft are all a 403",
        )

    def test_it_never_needs_actions_write(self):
        # Unlike the hub's tagger, this workflow never dispatches another
        # workflow run -- there is no build half to start.
        self.assertNotIn("actions: write", workflow_text())


class NeverBuildsUploadsOrPublishes(unittest.TestCase):
    def test_none_of_these_appear(self):
        text = workflow_text()
        for forbidden in (
            "gh workflow run",
            "gh release edit",
            "gh release upload",
            "--draft=false",
            "--latest",
            "build_release.py",
            "build.bat",
            "upload-artifact",
        ):
            self.assertNotIn(forbidden, text, f"{forbidden!r} is out of scope for this workflow")


class ThePrefixAgreesWithTheTool(unittest.TestCase):
    def test_the_workflow_and_the_tool_agree_on_v(self):
        self.assertEqual(forgepact_tag.PREFIX, "v")


if __name__ == "__main__":
    unittest.main()
