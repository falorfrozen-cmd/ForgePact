"""Tests for `forgepact-notes-cleanup.yml`, which deletes the release-notes
files a published release carried. Reuses `run_lines`/`code_lines` from
`test_forgepact_tag_workflow`, so every check reads what a shell will
actually execute rather than a comment that mentions it.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_forgepact_tag_workflow import run_lines, code_lines  # noqa: E402


REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO / ".github/workflows/forgepact-notes-cleanup.yml"


def workflow_text() -> str:
    return WORKFLOW.read_text(encoding="utf-8")


def first_code_line(needle: str) -> int:
    for i, line in enumerate(code_lines(workflow_text())):
        if needle in line:
            return i
    return -1


class TheWorkflowExists(unittest.TestCase):
    def test_it_is_there(self):
        self.assertTrue(WORKFLOW.exists())

    def test_the_run_block_reader_finds_a_known_line(self):
        # Positive control: every negative check below means nothing if the
        # reader cannot see this workflow's run blocks at all.
        self.assertNotEqual(first_code_line("--published-notes"), -1)


class ItRunsOnPublishOrByHand(unittest.TestCase):
    def test_triggers(self):
        text = workflow_text()
        on_block = text[text.index("\non:"):text.index("\npermissions:")]
        self.assertRegex(on_block, r"release:\s*\n\s*types:\s*\[published\]")
        self.assertIn("workflow_dispatch:", on_block)
        self.assertNotRegex(on_block, r"\n  (push|pull_request|schedule):")

    def test_the_tag_input_is_required(self):
        text = workflow_text()
        at = text.index("tag:", text.index("workflow_dispatch:"))
        self.assertIn("required: true", text[at:at + 200])

    def test_prereleases_are_skipped(self):
        self.assertIn("github.event.release.prerelease == false", workflow_text())


class UntrustedTextReachesTheShellOnlyThroughEnv(unittest.TestCase):
    def test_no_expression_context_inside_a_run_block(self):
        for line in run_lines(workflow_text()):
            self.assertNotIn("inputs.", line)
            self.assertNotIn("github.event", line)
            self.assertNotIn("${{", line)


class StepOrderIsTheGuardrail(unittest.TestCase):
    def test_it_checks_publication_then_lists_then_deletes_then_pushes(self):
        order = [
            first_code_line("release_ci.py tag"),
            first_code_line(".draft"),
            first_code_line("--published-notes"),
            first_code_line("git rm"),
            first_code_line("git commit"),
            first_code_line("git push origin HEAD:main"),
        ]
        self.assertNotIn(-1, order, order)
        self.assertEqual(order, sorted(order), order)

    def test_a_draft_is_refused(self):
        guard = [line for line in code_lines(workflow_text()) if ".draft" in line]
        self.assertTrue(guard)
        self.assertTrue(any("::error::" in line and "draft" in line.lower()
                            for line in code_lines(workflow_text())))

    def test_git_rm_is_fed_only_by_the_tool_output(self):
        rm_lines = [line for line in code_lines(workflow_text()) if "git rm" in line]
        self.assertEqual(len(rm_lines), 1, rm_lines)
        self.assertIn("$files", rm_lines[0])
        self.assertIn("--", rm_lines[0])


class ThePushIsOrdinaryAndRetried(unittest.TestCase):
    def test_a_rejected_push_rebases_and_retries(self):
        lines = code_lines(workflow_text())
        self.assertTrue(any("git pull --rebase origin main" in line for line in lines))

    def test_nothing_forceful_or_broad(self):
        lines = code_lines(workflow_text())
        for forbidden in ("--force", "push -f", "git add -A", "git add .",
                          "commit --all", "commit -a", "git tag",
                          "gh release create", "gh release edit",
                          "gh release delete", "gh release upload",
                          "gh workflow run"):
            self.assertFalse(any(forbidden in line for line in lines), forbidden)


class Permissions(unittest.TestCase):
    def test_top_level_is_read_only(self):
        text = workflow_text()
        top = text[text.index("\npermissions:"):text.index("\njobs:")]
        self.assertIn("contents: read", top)

    def test_the_job_may_only_write_contents(self):
        text = workflow_text()
        self.assertIn("contents: write", text[text.index("\njobs:"):])
        self.assertNotIn("actions: write", text)
        self.assertNotRegex(text, re.compile(r"^\s*(pull-requests|packages|id-token): write", re.M))


if __name__ == "__main__":
    unittest.main()
