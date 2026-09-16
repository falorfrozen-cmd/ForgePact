"""Tests for the workflow that builds the release zip and uploads it to the
tag's draft: `forgepact-release.yml`. Reuses `run_lines`/`code_lines` from
`test_forgepact_tag_workflow` -- the "what will a shell actually execute"
reader that matters for every check here, the same way it matters there.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_forgepact_tag_workflow import run_lines, code_lines  # noqa: E402


REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO / ".github/workflows/forgepact-release.yml"


def workflow_text() -> str:
    return WORKFLOW.read_text(encoding="utf-8")


class TheWorkflowExists(unittest.TestCase):
    def test_it_is_there(self):
        self.assertTrue(WORKFLOW.exists(), f"{WORKFLOW} is missing")


class TheRunBlockReaderActuallyReadsThem(unittest.TestCase):
    """Positive control: an instrument that finds nothing has measured nothing."""

    def test_a_known_run_line_is_found(self):
        lines = run_lines(workflow_text())
        self.assertGreater(len(lines), 20, "the run blocks are not being read")
        self.assertTrue(
            any("fetch_toolchain.py" in line for line in lines),
            "a command known to be in a run block was not found",
        )


class ItIsRunOnlyByHand(unittest.TestCase):
    def test_only_workflow_dispatch_triggers_it(self):
        text = workflow_text()
        self.assertIn("workflow_dispatch:", text)
        for trigger in ("push:", "pull_request:", "schedule:", "repository_dispatch:", "release:"):
            self.assertFalse(
                re.search(rf"(?m)^\s{{2}}{re.escape(trigger)}", text),
                f"{trigger} would build releases nobody asked for",
            )

    def test_tag_is_a_required_input(self):
        text = workflow_text()
        self.assertTrue(re.search(r"(?m)^\s{6}tag:\s*$", text))
        self.assertTrue(re.search(r"(?m)^\s{8}required: true\s*$", text))

    def test_dry_run_defaults_true(self):
        text = workflow_text()
        dry_run_block = text[text.find("dry_run:"):text.find("hub_ref:")]
        self.assertIn("default: true", dry_run_block)

    def test_it_refuses_to_run_off_main(self):
        self.assertTrue(
            re.search(r'\[ "\$BRANCH" != "main" \]', workflow_text()),
            "a build started off main would ship code that was never merged",
        )


class TheTagReachesTheShellSafely(unittest.TestCase):
    def test_inputs_never_appears_inside_a_run_block(self):
        for line in run_lines(workflow_text()):
            self.assertNotIn(
                "inputs.",
                line,
                f"a workflow_dispatch input is interpolated directly into a shell here: {line.strip()!r}",
            )

    def test_the_tag_is_passed_through_the_environment(self):
        self.assertIn("TAG_INPUT: ${{ inputs.tag }}", workflow_text())


class StepOrderIsTheGuardrail(unittest.TestCase):
    def test_the_pipeline_runs_in_order(self):
        text = workflow_text()
        markers = [
            ("draft_guard_1", ".draft"),
            ("checkout_tag", "ref: refs/tags/"),
            ("cut_release_check", "cut_release.py --check"),
            ("unittest_discover", "unittest discover"),
            ("fetch_toolchain", "fetch_toolchain.py"),
            ("compile_line", "compile-line"),
            ("build_bat_release", "build.bat release"),
            ("build_release_py", "build_release.py"),
            ("release_ci_package", "release_ci.py package"),
            ("gh_release_upload", "gh release upload"),
        ]
        positions = [(name, text.find(marker)) for name, marker in markers]
        for name, pos in positions:
            self.assertNotEqual(pos, -1, f"{name} is missing from the workflow")

        for (name_a, pos_a), (name_b, pos_b) in zip(positions, positions[1:]):
            self.assertLess(pos_a, pos_b, f"{name_a} must precede {name_b}")

    def test_a_second_draft_check_precedes_the_upload(self):
        text = workflow_text()
        package_at = text.find("release_ci.py package")
        upload_at = text.find("gh release upload")
        second_draft_at = text.find(".draft", package_at)
        self.assertNotEqual(second_draft_at, -1, "no second draft check after packaging")
        self.assertLess(package_at, second_draft_at)
        self.assertLess(second_draft_at, upload_at)


class UploadIsGatedAndClobbers(unittest.TestCase):
    def test_gh_release_upload_carries_clobber(self):
        # The real invocation wraps across several backslash-continued lines,
        # so this looks at the whole step's run block rather than one line.
        text = workflow_text()
        at = text.find("gh release upload")
        self.assertNotEqual(at, -1)
        block_end = text.find("\n\n", at)
        self.assertIn("--clobber", text[at:block_end if block_end != -1 else at + 400])

    def test_gh_release_upload_only_appears_in_a_not_dry_run_step(self):
        text = workflow_text()
        # The step immediately preceding "gh release upload" must be
        # conditioned on !inputs.dry_run.
        upload_at = text.find("gh release upload")
        preceding = text[:upload_at]
        step_start = preceding.rfind("- name:")
        step_text = text[step_start:upload_at]
        self.assertIn("if: ${{ !inputs.dry_run }}", step_text)

    def test_upload_artifact_only_appears_in_a_dry_run_step(self):
        text = workflow_text()
        artifact_at = text.find("upload-artifact")
        self.assertNotEqual(artifact_at, -1)
        preceding = text[:artifact_at]
        step_start = preceding.rfind("- name:")
        step_text = text[step_start:artifact_at]
        self.assertIn("if: inputs.dry_run", step_text)


class NeverPublishesOrDispatches(unittest.TestCase):
    def test_none_of_these_appear(self):
        text = workflow_text()
        for forbidden in (
            "gh release create",
            "gh release edit",
            "--draft=false",
            "--latest",
            "gh workflow run",
            "build.bat dev",
        ):
            self.assertNotIn(forbidden, text, f"{forbidden!r} is out of scope for this workflow")


class RunnerAndCheckouts(unittest.TestCase):
    def test_runner_is_pinned(self):
        self.assertIn("runs-on: windows-2025-vs2026", workflow_text())

    def test_sparse_checkout_names_hs_game_sdk(self):
        text = workflow_text()
        sparse_at = text.find("sparse-checkout:")
        self.assertNotEqual(sparse_at, -1)
        self.assertIn("hs-game-sdk", text[sparse_at:sparse_at + 120])


class Dependencies(unittest.TestCase):
    def test_requirements_build_txt_is_installed(self):
        lines = code_lines(workflow_text())
        self.assertTrue(any("requirements-build.txt" in line for line in lines))

    def test_webview_import_is_checked(self):
        lines = code_lines(workflow_text())
        self.assertTrue(any("import webview" in line for line in lines))


class BuildInfoMetadataIsActuallyRead(unittest.TestCase):
    """v1.3.20's first dry run recorded `cl_version` as cl's usage line and
    `runner_image` as "-". Both steps ran green; only the zip's
    BUILD-INFO.json showed the values were wrong."""

    def test_cl_redirects_stdout_before_stderr(self):
        cl_calls = [line for line in code_lines(workflow_text()) if line.startswith("cl ")]
        self.assertTrue(cl_calls, "positive control: the banner step's cl call is found")
        for call in cl_calls:
            # cmd applies redirections left to right, so `2>&1 > file` sends
            # stderr -- where the banner goes -- to the log, not the file.
            self.assertNotIn("2>&1 >", call)
            self.assertRegex(call, r">\s*\S.*2>&1")

    def test_the_banner_is_picked_by_content(self):
        self.assertTrue(any("Compiler Version" in line for line in code_lines(workflow_text())))

    def test_runner_image_comes_from_the_shell_not_the_env_context(self):
        # `${{ env.X }}` sees only workflow-defined variables, never the
        # runner machine's own ImageOS/ImageVersion.
        yaml_lines = [
            line for line in workflow_text().splitlines()
            if not line.strip().startswith("#")
        ]
        self.assertFalse(any("env.Image" in line for line in yaml_lines))
        self.assertTrue(any("${ImageOS" in line for line in code_lines(workflow_text())))


class Permissions(unittest.TestCase):
    def test_no_job_has_actions_write(self):
        self.assertNotIn("actions: write", workflow_text())


if __name__ == "__main__":
    unittest.main()
