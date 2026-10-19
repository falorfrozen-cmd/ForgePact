"""Tests for the Discord release announcement: `tools/discord_release_post.py`
(how the notes become messages) and `.github/workflows/discord-release.yml`
(when it runs and how the secret and the release text reach the script).
"""

import importlib.util
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_forgepact_tag_workflow import run_lines  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO / ".github/workflows/discord-release.yml"
SCRIPT = REPO / "tools/discord_release_post.py"

spec = importlib.util.spec_from_file_location("discord_release_post", SCRIPT)
post = importlib.util.module_from_spec(spec)
spec.loader.exec_module(post)

NOTES_2_3_1_LIKE = "# ForgePact 2.3.1\n\nA quick fix.\n\n## Fixed\n\n" + "\n".join(
    f"- **Fix {i}.** " + "word " * 60 for i in range(20)
)


class Splitting(unittest.TestCase):
    def test_short_notes_are_one_message_with_ping_and_link(self):
        msgs = post.build_messages("ForgePact 9.9.9", "https://x/r", "# ForgePact 9.9.9\n\nHi.")
        self.assertEqual(len(msgs), 1)
        self.assertTrue(msgs[0]["content"].startswith("@everyone **ForgePact 9.9.9 is out!**"))
        self.assertIn("# ForgePact 9.9.9\n\nHi.", msgs[0]["content"])
        self.assertTrue(msgs[0]["content"].endswith("Download: <https://x/r>"))
        self.assertEqual(msgs[0]["allowed_mentions"], {"parse": ["everyone"]})

    def test_long_notes_split_under_the_limit_and_only_the_first_pings(self):
        msgs = post.build_messages("ForgePact 2.3.1", "https://x/r", NOTES_2_3_1_LIKE)
        self.assertGreater(len(msgs), 1)
        for m in msgs:
            self.assertLessEqual(len(m["content"]), 2000)
        self.assertEqual([m["allowed_mentions"]["parse"] for m in msgs],
                         [["everyone"]] + [[]] * (len(msgs) - 1))
        self.assertIn("@everyone", msgs[0]["content"])
        self.assertNotIn("@everyone", "".join(m["content"] for m in msgs[1:]))
        self.assertTrue(msgs[-1]["content"].endswith("Download: <https://x/r>"))

    def test_no_text_is_lost(self):
        pieces = post.split_notes(NOTES_2_3_1_LIKE)
        self.assertEqual("".join("".join(p.split()) for p in pieces), "".join(NOTES_2_3_1_LIKE.split()))

    def test_a_line_longer_than_a_message_is_cut(self):
        pieces = post.split_notes("x " * 2500)
        self.assertTrue(all(len(p) <= post.CHUNK for p in pieces))
        self.assertEqual(sum(p.count("x") for p in pieces), 2500)
        hard = post.split_notes("y" * 4000)
        self.assertEqual([len(p) for p in hard], [1800, 1800, 400])

    def test_crlf_notes(self):
        self.assertEqual(post.split_notes("a\r\nb"), ["a\nb"])

    def test_no_everyone(self):
        msgs = post.build_messages("ForgePact 1", "https://x/r", NOTES_2_3_1_LIKE, everyone=False)
        self.assertFalse(any("@everyone" in m["content"] for m in msgs))
        self.assertTrue(all(m["allowed_mentions"] == {"parse": []} for m in msgs))

    def test_a_ping_inside_the_notes_never_pings(self):
        msgs = post.build_messages("ForgePact 1", "https://x/r", "x\n" * 1500 + "@here and @everyone")
        self.assertEqual(msgs[-1]["allowed_mentions"], {"parse": []})


class Running(unittest.TestCase):
    def notes_file(self, text):
        f = tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8")
        f.write(text)
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def test_dry_run_prints_and_sends_nothing(self):
        out = io.StringIO()
        with mock.patch.object(post, "post") as sent, redirect_stdout(out), \
                mock.patch.dict(os.environ, {"DISCORD_WEBHOOK_URL": "https://discord.test/hook"}):
            code = post.main(["--tag", "v1.0.0", "--url", "https://x/r", "--notes-file",
                              self.notes_file("Notes"), "--dry-run"])
        self.assertEqual(code, 0)
        sent.assert_not_called()
        self.assertIn("@everyone **ForgePact 1.0.0 is out!**", out.getvalue())
        self.assertNotIn("discord.test", out.getvalue())

    def test_posts_every_message_in_order(self):
        with mock.patch.object(post, "post") as sent, mock.patch.object(post.time, "sleep"), \
                redirect_stdout(io.StringIO()), \
                mock.patch.dict(os.environ, {"DISCORD_WEBHOOK_URL": "https://discord.test/hook"}):
            code = post.main(["--tag", "v2.3.1", "--name", "ForgePact 2.3.1", "--url", "https://x/r",
                              "--notes-file", self.notes_file(NOTES_2_3_1_LIKE)])
        self.assertEqual(code, 0)
        expected = post.build_messages("ForgePact 2.3.1", "https://x/r", NOTES_2_3_1_LIKE)
        self.assertEqual([c.args for c in sent.call_args_list],
                         [("https://discord.test/hook", m) for m in expected])

    def test_missing_secret_fails_without_posting(self):
        with mock.patch.object(post, "post") as sent, redirect_stdout(io.StringIO()), \
                mock.patch.dict(os.environ, {"DISCORD_WEBHOOK_URL": ""}):
            code = post.main(["--tag", "v1", "--url", "https://x/r", "--notes-file", self.notes_file("n")])
        self.assertEqual(code, 1)
        sent.assert_not_called()

    def test_the_webhook_is_never_an_argument(self):
        self.assertNotIn("--webhook", SCRIPT.read_text(encoding="utf-8"))


class TheWorkflow(unittest.TestCase):
    def text(self):
        return WORKFLOW.read_text(encoding="utf-8")

    def test_triggers(self):
        text = self.text()
        on_block = text[text.index("\non:"):text.index("\npermissions:")]
        self.assertRegex(on_block, r"release:\s*\n\s*types:\s*\[published\]")
        self.assertIn("workflow_dispatch:", on_block)
        self.assertNotRegex(on_block, r"\n  (push|pull_request|schedule):")

    def test_prereleases_are_skipped(self):
        self.assertIn("github.event.release.prerelease == false", self.text())

    def test_manual_runs_are_dry_and_silent_by_default(self):
        text = self.text()
        dry = text.index("dry_run:")
        self.assertIn("default: true", text[dry:dry + 150])
        everyone = text.index("everyone:", text.index("inputs:"))
        self.assertIn("default: false", text[everyone:everyone + 150])

    def test_read_only_token(self):
        self.assertIn("permissions:\n  contents: read\n", self.text())

    def test_no_expression_inside_a_run_block(self):
        # Release names and notes are text a person typed: they reach the
        # shell only through env and files, never spliced into a script.
        lines = run_lines(self.text())
        self.assertTrue(lines, "positive control: the run blocks were found")
        self.assertFalse([l for l in lines if "${{" in l])

    def test_the_secret_only_reaches_the_post_step(self):
        text = self.text()
        self.assertEqual(text.count("secrets.DISCORD_WEBHOOK_URL"), 1)
        self.assertLess(text.index("name: Post to Discord"), text.index("secrets.DISCORD_WEBHOOK_URL"))


if __name__ == "__main__":
    unittest.main()
