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


def jobs() -> dict:
    """{job id: its text}, for each job under `jobs:`."""
    text = workflow_text()
    body = text[text.index("\njobs:\n"):]
    heads = list(re.finditer(r"(?m)^  ([\w-]+):\s*$", body))
    return {m.group(1): body[m.start():(heads[i + 1].start() if i + 1 < len(heads) else len(body))]
            for i, m in enumerate(heads)}


COMPILE_STEP = "Compile the plugin (release)"


def step_text(job_text: str, at: int) -> str:
    """The whole step of `job_text` that contains offset `at`."""
    start = job_text.rfind("\n      - ", 0, at)
    end = job_text.find("\n      - ", at)
    return job_text[start + 1 if start != -1 else 0:end if end != -1 else len(job_text)]


def compile_step_env() -> dict:
    """{name: value} of the `env:` block on build's compile step.

    tests/test_release_pdb_symbols.py compiles its probe under exactly these
    values, so what that test proves is about what this workflow sets.
    """
    build = jobs()["build"]
    at = build.find(f"- name: {COMPILE_STEP}")
    if at == -1:
        raise AssertionError(f"{COMPILE_STEP!r} is missing from build")
    step = step_text(build, at + 2)
    env = {}
    inside = False
    for line in step.splitlines():
        if re.match(r"^\s{8}env:\s*$", line):
            inside = True
            continue
        if inside:
            m = re.match(r"^\s{10}([\w-]+):\s*(.*?)\s*$", line)
            if not m:
                break
            env[m.group(1)] = m.group(2).strip("'\"")
    return env


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
            ("setup_node", "actions/setup-node"),
            ("npm_ci", "npm ci"),
            ("npm_run_build", "npm run build"),
            ("contract_tests", "--skip-group panel-browser"),
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

    def test_the_second_guard_requires_the_tag_to_still_be_the_built_commit(self):
        # forgepact-tag.yml's recut deletes a draft and its tag and makes both
        # again on a newer commit. A build of the old commit still running
        # then finds a draft on "its" tag, so the draft flag alone would let
        # it put the old zip on the new draft.
        text = workflow_text()
        start = text.find("- name: This release is still a draft (guard 2)")
        end = text.find("- name: Upload to the draft")
        self.assertNotEqual(start, -1, "guard 2 is missing")
        self.assertLess(start, end)
        guard = text[start:end]
        self.assertIn("SOURCE_COMMIT: ${{ needs.build.outputs.source_commit }}", guard)
        code = code_lines(guard)
        self.assertTrue(
            any('commits/refs/tags/$TAG' in line for line in code),
            "guard 2 must ask where the tag points now",
        )
        at = next(
            (i for i, line in enumerate(code) if '"$tagged" != "$SOURCE_COMMIT"' in line),
            None,
        )
        self.assertIsNotNone(at, "guard 2 must compare the tag with the commit it built")
        self.assertIn("exit 1", code[at:at + 3])


class ThePanelIsBuiltBeforeItIsTested(unittest.TestCase):
    """The panel's frontend (panel/, Svelte + Vite) is not tracked as built
    files: build_release.py refuses to package without panel/dist, and the
    contract tests read it, so the tag's own tree is built with Node first."""

    def step(self, marker):
        text = workflow_text()
        at = text.find(marker)
        self.assertNotEqual(at, -1, f"{marker!r} is missing from the workflow")
        start = text.rfind("\n      - ", 0, at)
        self.assertNotEqual(start, -1)
        end = text.find("\n      - ", at)
        return text[start:end if end != -1 else len(text)]

    def test_the_node_steps_run_in_the_tagged_panel(self):
        for marker in ("run: npm ci", "run: npm run build"):
            self.assertIn("working-directory: ForgePact/panel", self.step(marker), marker)

    def test_npm_ci_installs_from_the_lockfile(self):
        lines = run_lines(workflow_text())
        self.assertTrue(any(line.strip() == "npm ci" for line in lines),
                        "npm install would resolve new versions at release time")

    def test_setup_node_caches_on_the_panel_lockfile(self):
        step = self.step("actions/setup-node")
        self.assertIn("node-version: '22'", step)
        self.assertIn("cache: npm", step)
        self.assertIn("cache-dependency-path: ForgePact/panel/package-lock.json", step)


class UploadIsGatedAndClobbers(unittest.TestCase):
    def test_gh_release_upload_carries_clobber(self):
        # The real invocation wraps across several backslash-continued lines,
        # so this looks at the whole step's run block rather than one line.
        text = workflow_text()
        at = text.find("gh release upload")
        self.assertNotEqual(at, -1)
        block_end = text.find("\n\n", at)
        self.assertIn("--clobber", text[at:block_end if block_end != -1 else at + 400])

    def test_gh_release_upload_only_appears_in_the_not_dry_run_upload_job(self):
        found = {name for name, text in jobs().items() if "gh release upload" in text}
        self.assertEqual(found, {"upload"})
        head = jobs()["upload"].split("steps:", 1)[0]
        self.assertIn("if: ${{ !inputs.dry_run }}", head)

    def test_the_upload_waits_for_both_test_jobs(self):
        head = jobs()["upload"].split("steps:", 1)[0]
        self.assertIn("needs: [build, panel-browser-tests]", head)

    def test_the_browser_job_cannot_write(self):
        # build keeps contents: write only because guard 1 must see a
        # draft, which the Releases API lists only to push access.
        writers = {name for name, text in jobs().items() if "contents: write" in text}
        self.assertEqual(writers, {"build", "upload"})
        self.assertIn("contents: read", jobs()["panel-browser-tests"])

    def test_gh_release_upload_appears_only_in_upload(self):
        self.assertNotIn("gh release upload", jobs()["build"])

    def test_the_zip_is_kept_on_every_run_and_checked_before_upload(self):
        build, upload = jobs()["build"], jobs()["upload"]
        step = build[build.rfind("- name:", 0, build.find("upload-artifact")):]
        self.assertNotIn("if:", step[:step.find("upload-artifact")],
                         "the upload job has nothing to upload unless every run keeps the zip")
        self.assertIn("zip_hash: ${{ steps.package.outputs.zip_hash }}", build)
        self.assertIn("download-artifact", upload)
        check = upload.find("ZIP_HASH: ${{ needs.build.outputs.zip_hash }}")
        self.assertNotEqual(check, -1, "the upload job must compare the artifact with build's hash")
        self.assertLess(check, upload.find("gh release upload"))
        self.assertTrue(any('"$got" != "$ZIP_HASH"' in line for line in code_lines(upload)))


class TheSuiteIsSplitAcrossTwoJobs(unittest.TestCase):
    """build runs everything but the panel's browser suites, and
    panel-browser-tests runs just those, through main's parallel runner;
    --skip-group/--only-group of one group cover the suite once between them."""

    def runner_lines(self, job):
        """The run block that calls the runner, from that call to the step's end."""
        text = jobs()[job]
        lines = [line for line in code_lines(text) if "run_tests_parallel.py" in line]
        self.assertEqual(len(lines), 1, f"{job} runs the suite once, through the runner")
        start = text.rfind("python ", 0, text.find("run_tests_parallel.py"))
        end = text.find("\n      - ", start)
        return text[start:end if end != -1 else len(text)]

    def test_the_two_jobs_select_complementary_halves_of_one_group(self):
        self.assertIn("--skip-group panel-browser", self.runner_lines("build"))
        self.assertIn("--only-group panel-browser", self.runner_lines("panel-browser-tests"))

    def test_both_use_mains_runner_on_the_tagged_tests(self):
        for job in ("build", "panel-browser-tests"):
            call = self.runner_lines(job)
            self.assertIn("../forgepact-ci/tools/run_tests_parallel.py -s tests", call, job)

    def test_nothing_runs_unittest_discover_serially(self):
        self.assertFalse(any("unittest discover" in line for line in code_lines(workflow_text())))

    def test_only_the_perf_suite_is_left_out_and_only_when_the_tag_has_it(self):
        text = workflow_text()
        self.assertEqual(re.findall(r"--exclude-module ([\w.*-]+)", text), ["test_panel_perf"])
        self.assertIn("if [ -f tests/test_panel_perf.py ]", text)

    def test_a_skipped_browser_suite_fails_its_job(self):
        call = self.runner_lines("panel-browser-tests")
        self.assertIn("set -o pipefail", jobs()["panel-browser-tests"])
        self.assertIn("skipped=", call)
        self.assertIn("exit 1", call[call.find("skipped="):])

    def test_a_hung_browser_suite_cannot_hold_the_runner_for_hours(self):
        # The job takes about ten minutes; a tag's harness that hangs instead
        # of failing would otherwise sit out GitHub's six-hour default.
        head = jobs()["panel-browser-tests"].split("steps:", 1)[0]
        limit = re.search(r"(?m)^    timeout-minutes: (\d+)\s*$", head)
        self.assertIsNotNone(limit, "panel-browser-tests has no timeout-minutes")
        self.assertTrue(20 <= int(limit.group(1)) <= 60, limit.group(0))


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

    def test_both_test_jobs_get_hs_game_sdk_as_a_sibling(self):
        # The panel's sandbox imports hs-game-sdk from beside the ForgePact
        # checkout. Without it the Satanic Zone pool is empty and the browser
        # suites fail on the environment: 2.0.0's first recut did.
        for job in ("build", "panel-browser-tests"):
            text = jobs()[job]
            self.assertIn("repository: falorfrozen-cmd/hero-siege-offline-toolkit", text, job)
            placed = text.find("mv hub/hs-game-sdk hs-game-sdk")
            self.assertNotEqual(placed, -1, f"{job} never places hs-game-sdk beside ForgePact")
            self.assertLess(placed, text.find("run_tests_parallel.py"), f"{job} places the SDK after its tests")


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


class ThePluginSymbolsAreKeptBesideTheZip(unittest.TestCase):
    """Issue #76: an incident report records a crash as a module and an
    offset, and only that tag's PDB maps an offset inside BloodPactPlugin.dll
    to one of our functions. build.bat's `cl ` line is the compile contract
    (test_build_bat_contract.py, and release_ci.py compile-line refuses a tag
    whose line differs from main's), so the symbols come from the compile
    step's environment instead: cl prepends CL's options to its command line
    and link appends _LINK_'s. test_release_pdb_symbols.py proves, on a probe
    compiled under these exact values, that they change no code."""

    def test_the_compile_step_asks_cl_and_link_for_symbols(self):
        env = compile_step_env()
        self.assertIn("/Zi", env.get("CL", "").split(), "cl writes no debug information without /Zi")
        link = env.get("_LINK_", "").split()
        # /DEBUG turns /OPT:REF and /OPT:ICF off unless they are named, which
        # would change the shipped code; both stay explicit.
        for option in ("/DEBUG:FULL", "/OPT:REF", "/OPT:ICF"):
            self.assertIn(option, link)
        alt = [option for option in link if option.startswith("/PDBALTPATH:")]
        self.assertEqual(len(alt), 1, "the DLL must name its PDB without the runner's build path")
        name = alt[0].split(":", 1)[1]
        self.assertTrue(name.endswith(".pdb") and "\\" not in name and "/" not in name, alt[0])

    def test_the_compile_line_itself_is_unchanged(self):
        build = jobs()["build"]
        step = step_text(build, build.find(f"- name: {COMPILE_STEP}") + 2)
        self.assertEqual([line for line in code_lines(step)], ["plugin_build\\build.bat release"])

    def test_only_the_compile_step_gets_the_symbol_options(self):
        # The contract tests compile native harnesses of their own; they keep
        # the compiler's defaults.
        text = workflow_text()
        self.assertEqual(len(re.findall(r"(?m)^\s+CL:", text)), 1)
        self.assertEqual(len(re.findall(r"(?m)^\s+_LINK_:", text)), 1)

    def artifact_steps(self):
        build = jobs()["build"]
        found = [m.start() for m in re.finditer(r"uses: actions/upload-artifact@", build)]
        return build, [step_text(build, at) for at in found], found

    def test_the_pdb_is_a_second_artifact_after_the_zip(self):
        build, steps, at = self.artifact_steps()
        self.assertEqual(len(steps), 2, "build keeps the zip and the PDB, nothing else")
        zip_step, pdb_step = steps
        self.assertIn("name: forgepact-release-zip", zip_step)
        self.assertNotIn(".pdb", zip_step)
        self.assertIn("plugin_build/BloodPactPlugin_ship.pdb", pdb_step)
        self.assertIn("name: forgepact-plugin-pdb-${{ steps.tag.outputs.tag }}", pdb_step)
        self.assertRegex(pdb_step, r"(?m)^\s+retention-days: 90\s*$")
        self.assertRegex(pdb_step, r"(?m)^\s+if-no-files-found: error\s*$")
        self.assertLess(build.find(f"- name: {COMPILE_STEP}"), at[1])
        self.assertLess(at[0], at[1])

    def test_the_pdb_is_kept_on_every_run(self):
        # A dry run is how a maintainer gets the symbols for a build that was
        # never uploaded; the step carries no condition, as the zip's does not.
        _, steps, _ = self.artifact_steps()
        head = steps[1][:steps[1].find("uses:")]
        self.assertNotIn("if:", head)

    def test_the_draft_gets_only_the_zip_and_its_hash(self):
        upload = jobs()["upload"]
        at = upload.find("gh release upload")
        self.assertNotEqual(at, -1)
        call = upload[at:upload.find("--clobber", at)]
        self.assertEqual(
            re.findall(r'"([^"]+)"', call),
            ["$TAG", "$RUNNER_TEMP/out/ForgePact-$VERSION.zip", "$RUNNER_TEMP/out/ForgePact-$VERSION.zip.sha256"],
        )
        self.assertNotIn(".pdb", upload)
        self.assertNotIn("forgepact-plugin-pdb", upload)


if __name__ == "__main__":
    unittest.main()
