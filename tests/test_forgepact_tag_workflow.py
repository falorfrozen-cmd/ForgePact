"""Tests for the workflow that tags a ForgePact release and drafts its notes.

`forgepact-tag.yml` is a `git tag` wrapped in guards, plus a draft release
whose body `tools/forgepact_tag.py` composes, plus a dispatch of the build
half (`forgepact-release.yml`) right after the draft is left -- the same
shape as the hub's `tests/test_hub_tag_workflow.py`, except the dispatch here
targets `main` rather than the tag ref (see `forgepact-release.yml`'s own
header comment for why: the tag tree it would otherwise run against does not
carry the build workflow itself). The guards are still the point: once a tag
is pushed, nothing here is undone from the repository's side. This file also
carries the notes-composition and draft-only assertions this workflow adds
that the hub's tagger has no equivalent of.
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

    def test_it_may_start_the_build(self):
        # Starting forgepact-release.yml needs actions: write, the documented
        # exception to "events made with GITHUB_TOKEN start no runs" -- the
        # same permission the hub's tagger carries for the same reason.
        self.assertTrue(
            re.search(r"(?m)^\s*actions: write\s*$", workflow_text()),
            "gh workflow run forgepact-release.yml is a 403 without this",
        )


class NeverBuildsUploadsOrPublishes(unittest.TestCase):
    def test_none_of_these_appear(self):
        text = workflow_text()
        for forbidden in (
            "gh release edit",
            "gh release upload",
            "--draft=false",
            "--latest",
            "build_release.py",
            "build.bat",
            "upload-artifact",
        ):
            self.assertNotIn(forbidden, text, f"{forbidden!r} is out of scope for this workflow")


class TheBuildIsDispatchedAfterTheDraft(unittest.TestCase):
    def test_the_dispatch_exists(self):
        lines = code_lines(workflow_text())
        self.assertTrue(
            any("gh workflow run forgepact-release.yml" in line for line in lines),
            "the draft is left with nothing building it",
        )

    def test_it_comes_after_the_draft_is_created(self):
        text = workflow_text()
        create_at = text.find("gh release create")
        dispatch_at = text.find("gh workflow run forgepact-release.yml")
        self.assertNotEqual(create_at, -1)
        self.assertNotEqual(dispatch_at, -1)
        self.assertLess(
            create_at, dispatch_at,
            "starting the build before the draft exists races the tag",
        )

    def test_it_targets_main_with_the_tag_and_a_real_run(self):
        lines = code_lines(workflow_text())
        dispatch = next(
            (line for line in lines if "gh workflow run forgepact-release.yml" in line),
            None,
        )
        self.assertIsNotNone(dispatch)
        self.assertIn("--ref main", dispatch)
        self.assertIn('-f tag="$TAG"', dispatch)
        self.assertIn("-f dry_run=false", dispatch)


def step_block(text: str, name: str) -> str:
    """The text of the step whose `- name:` starts with `name`, up to the next step."""
    start = text.find(f"- name: {name}")
    if start == -1:
        raise AssertionError(f"no step named {name!r}")
    end = text.find("\n      - ", start)
    return text[start:end if end != -1 else len(text)]


def steps(text: str) -> list[str]:
    """Every step in the job, as text, in order."""
    jobs_at = text.find("\n    steps:\n")
    parts = re.split(r"\n(?=      - )", text[jobs_at:])
    return [part for part in parts[1:] if part.lstrip().startswith("- ")]


GUARD_1 = "This version has no release yet"
GUARD_2 = "The draft is still a draft (guard 2)"
DELETE = "Delete the old draft and its tag"
REPLACE_ONLY = "if: steps.release.outputs.replace == 'true'"


class RecutIsAnExplicitInput(unittest.TestCase):
    """Replacing a draft deletes a release and a tag, so it never happens
    because a version was typed that happened to be taken."""

    def recut_block(self) -> str:
        match = re.search(r"(?m)^ {6}recut:\s*\n((?: {8}.*\n)+)", workflow_text())
        self.assertIsNotNone(match, "there is no recut input")
        return match.group(1)

    def test_it_is_a_boolean_that_defaults_to_false(self):
        block = self.recut_block()
        self.assertRegex(block, r"(?m)^ {8}type: boolean\s*$")
        self.assertRegex(block, r"(?m)^ {8}default: false\s*$")
        self.assertNotRegex(block, r"required: true")

    def test_it_reaches_the_shell_only_through_the_environment(self):
        text = workflow_text()
        self.assertIn("RECUT: ${{ inputs.recut }}", text)
        for line in run_lines(text):
            self.assertNotIn("inputs.recut", line, f"interpolated into a shell: {line.strip()!r}")

    def test_the_tool_is_told_only_when_the_input_is_true(self):
        code = code_lines(step_block(workflow_text(), "Is this a tag we can release"))
        self.assertIn('if [ "$RECUT" = "true" ]; then', code)
        self.assertIn("recut=(--recut)", code)
        plan = next((line for line in code if "tools/forgepact_tag.py" in line), "")
        self.assertIn('"${recut[@]}"', plan)


class RecutReplacesOnlyADraft(unittest.TestCase):
    def guard_1(self) -> list[str]:
        return code_lines(step_block(workflow_text(), GUARD_1))

    def error_after(self, code: list[str], condition: str) -> str:
        at = next((i for i, line in enumerate(code) if condition in line), None)
        self.assertIsNotNone(at, f"no check {condition!r}")
        following = code[at + 1:at + 3]
        self.assertTrue(any(line == "exit 1" for line in following), f"{condition!r} does not stop")
        return next(line for line in following if "::error::" in line)

    def test_the_run_block_reader_finds_the_guard(self):
        """Positive control for the step reader these tests use."""
        self.assertTrue(any("releases?per_page=100" in line for line in self.guard_1()))

    def test_with_recut_off_a_release_still_refuses_and_names_the_input(self):
        message = self.error_after(self.guard_1(), '"$RECUT" != "true"')
        self.assertIn("recut", message)

    def test_recut_wants_exactly_one_release(self):
        self.error_after(self.guard_1(), '"$existing" != "1"')

    def test_a_published_release_is_refused(self):
        message = self.error_after(self.guard_1(), '"$draft" != "true"')
        self.assertIn("published", message)
        self.assertIn("never deleted or moved", message)

    def test_a_tag_with_no_release_is_refused(self):
        code = self.guard_1()
        at = next((i for i, line in enumerate(code) if '*" refs/tags/$TAG "*)' in line), None)
        self.assertIsNotNone(at, "a tag with no release must be told apart from no tag")
        self.assertIn("exit 1", code[at:at + 4])

    def test_replace_is_only_set_after_every_check(self):
        code = self.guard_1()
        replace_at = code.index('echo "replace=true" >> "$GITHUB_OUTPUT"')
        for condition in ('"$RECUT" != "true"', '"$existing" != "1"', '"$draft" != "true"'):
            check_at = next(i for i, line in enumerate(code) if condition in line)
            self.assertLess(check_at, replace_at, f"{condition} is checked after replace is set")

    def test_the_count_draft_and_id_come_from_one_query(self):
        queries = [line for line in self.guard_1() if "gh api" in line]
        self.assertEqual(len(queries), 1, "two queries can describe two different moments")


class ASecondGuardRunsRightBeforeTheDelete(unittest.TestCase):
    def test_guard_2_rechecks_the_same_draft(self):
        block = step_block(workflow_text(), GUARD_2)
        self.assertIn(REPLACE_ONLY, block)
        code = code_lines(block)
        self.assertTrue(any("releases?per_page=100" in line for line in code), "guard 2 must ask again")
        condition = next((line for line in code if '"$draft" != "true"' in line), "")
        self.assertIn('"$existing" != "1"', condition)
        self.assertIn('"$id" != "$ID"', condition)
        at = code.index(condition)
        self.assertIn("exit 1", code[at:at + 3])

    def test_guard_2_is_the_step_right_before_the_delete(self):
        names = [re.match(r"\s*- (?:name: )?(.*)", part).group(1) for part in steps(workflow_text())]
        guard_at = next(i for i, name in enumerate(names) if name.startswith(GUARD_2))
        self.assertTrue(names[guard_at + 1].startswith(DELETE), names[guard_at + 1])
        self.assertTrue(names[guard_at - 1].startswith(GUARD_1), names[guard_at - 1])


class DeletionOnlyFollowsTheDraftChecks(unittest.TestCase):
    DELETIONS = ("-X DELETE", "--delete", "git tag -d", "release delete")

    def test_every_deletion_is_in_the_replace_only_delete_step(self):
        for part in steps(workflow_text()):
            code = code_lines(part)
            if any(word in line for line in code for word in self.DELETIONS):
                self.assertIn(f"- name: {DELETE}", part)
                self.assertIn(REPLACE_ONLY, part)

    def test_the_delete_step_deletes_the_checked_release_then_its_tag(self):
        code = code_lines(step_block(workflow_text(), DELETE))
        asked = next(i for i, line in enumerate(code) if "git ls-remote --exit-code" in line)
        release = code.index('gh api -X DELETE "repos/$REPO/releases/$ID"')
        remote_tag = code.index('git push origin --delete "refs/tags/$TAG"')
        local_tag = code.index('git tag -d "$TAG"')
        self.assertLess(asked, release, "a failed tag query must come before anything is deleted")
        self.assertLess(release, remote_tag, "deleting the tag first leaves the draft behind")
        self.assertLess(remote_tag, local_tag)

    def test_the_delete_follows_both_guards_and_precedes_the_notes_and_the_tag(self):
        text = workflow_text()
        order = [
            text.find(f"- name: {GUARD_1}"),
            text.find(f"- name: {GUARD_2}"),
            text.find("-X DELETE"),
            # After the old tag is gone, so generate-notes honours
            # target_commitish instead of measuring up to the old tag.
            text.find('gh api -X POST "repos/$REPO/releases/generate-notes"'),
            text.find("git tag -a"),
            text.find("gh release create"),
        ]
        self.assertNotIn(-1, order)
        self.assertEqual(order, sorted(order))

    def test_the_summary_says_when_a_draft_was_replaced(self):
        block = step_block(workflow_text(), "Say what still needs a human")
        self.assertIn("REPLACED: ${{ steps.release.outputs.replace }}", block)


class ThePrefixAgreesWithTheTool(unittest.TestCase):
    def test_the_workflow_and_the_tool_agree_on_v(self):
        self.assertEqual(forgepact_tag.PREFIX, "v")


if __name__ == "__main__":
    unittest.main()
