"""The panel's frontend is a built directory (panel/dist) that forgepact.py
serves and build_release.py bundles. Pins where the server looks for it, what
it will and will not hand out, and the packaging guard that refuses to build
an exe without it -- an exe missing the build starts, listens, and shows a
player nothing, which is the "reports itself fine while doing nothing" shape.

Runs against isolated settings and a temporary dist directory, never the
game's IPC (PanelSandbox from test_satanic_panel)."""
import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from http.client import HTTPConnection
from pathlib import Path
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO))

import build_release  # noqa: E402
from test_satanic_panel import PanelSandbox, forgepact  # noqa: E402


INDEX = b'<!doctype html><html><body><div id="app"></div></body></html>'
# One file per suffix the server has to name, plus a suffix it does not know.
FILES = {
    "index.html": INDEX,
    "assets/index-abc123.js": b"console.log('panel')",
    "assets/index-abc123.css": b"body{margin:0}",
    "favicon.svg": b"<svg xmlns='http://www.w3.org/2000/svg'/>",
    "assets/font.woff2": b"wOF2",
    "assets/shot.png": b"\x89PNG",
    "assets/data.json": b"{}",
    "favicon.ico": b"\x00\x00\x01\x00",
    "assets/blob.bin": b"\x01\x02",
    # A file named like an API route: /api/* must never be answered from disk.
    "api/state": b"not the state",
}
EXPECTED_TYPES = {
    "/index.html": "text/html; charset=utf-8",
    "/assets/index-abc123.js": "text/javascript",
    "/assets/index-abc123.css": "text/css",
    "/favicon.svg": "image/svg+xml",
    "/assets/font.woff2": "font/woff2",
    "/assets/shot.png": "image/png",
    "/assets/data.json": "application/json",
    "/favicon.ico": "image/x-icon",
    "/assets/blob.bin": "application/octet-stream",
}
SECRET = b"outside the panel build"


def get(port, path):
    connection = HTTPConnection("127.0.0.1", port, timeout=5)
    try:
        connection.request("GET", path)
        response = connection.getresponse()
        return response.status, dict(response.getheaders()), response.read()
    finally:
        connection.close()


class PanelDistSourcesTests(unittest.TestCase):
    """Three places the build can come from, in a fixed order of precedence."""

    def setUp(self):
        self.override_at_start = os.environ.get("FORGEPACT_PANEL_DIST")
        env = patch.dict(os.environ)
        env.start()
        self.addCleanup(env.stop)
        os.environ.pop("FORGEPACT_PANEL_DIST", None)

    def test_from_source_it_is_the_checkouts_panel_dist(self):
        self.assertEqual(forgepact._panel_dist(), REPO / "panel" / "dist")

    def test_frozen_it_is_the_bundled_panel_directory(self):
        with patch.object(sys, "frozen", True, create=True), \
             patch.object(sys, "_MEIPASS", r"C:\unpack\_MEI123", create=True):
            self.assertEqual(forgepact._panel_dist(), Path(r"C:\unpack\_MEI123") / "panel")

    def test_the_environment_overrides_both(self):
        os.environ["FORGEPACT_PANEL_DIST"] = r"C:\somewhere\else"
        self.assertEqual(forgepact._panel_dist(), Path(r"C:\somewhere\else"))
        with patch.object(sys, "frozen", True, create=True), \
             patch.object(sys, "_MEIPASS", r"C:\unpack\_MEI123", create=True):
            self.assertEqual(forgepact._panel_dist(), Path(r"C:\somewhere\else"))

    def test_an_empty_override_is_no_override(self):
        os.environ["FORGEPACT_PANEL_DIST"] = ""
        self.assertEqual(forgepact._panel_dist(), REPO / "panel" / "dist")

    def test_the_module_constant_is_resolved_the_same_way(self):
        # Resolved once at import; the handler reads the module global on every
        # request, so a test or sandbox can patch PANEL_DIST itself.
        expected = Path(self.override_at_start) if self.override_at_start else REPO / "panel" / "dist"
        self.assertEqual(forgepact.PANEL_DIST, expected)


class StaticServingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-dist-")
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.dist = base / "dist"
        for name, body in FILES.items():
            path = self.dist / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(body)
        (base / "secret.txt").write_bytes(SECRET)
        dist_patch = patch.object(forgepact, "PANEL_DIST", self.dist)
        dist_patch.start()
        self.addCleanup(dist_patch.stop)
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        self.addCleanup(self.sandbox.__exit__)

    def get(self, path):
        return get(self.sandbox.port, path)

    def test_root_serves_the_built_index_uncached(self):
        for path in ("/", "/?t=1"):
            status, headers, body = self.get(path)
            self.assertEqual(status, 200, path)
            self.assertEqual(body, INDEX, path)
            self.assertEqual(headers["Content-Type"], "text/html; charset=utf-8")
            self.assertEqual(headers["Cache-Control"], "no-cache")
            self.assertEqual(headers["Content-Length"], str(len(INDEX)))

    def test_every_suffix_gets_its_content_type(self):
        for path, content_type in EXPECTED_TYPES.items():
            status, headers, body = self.get(path)
            self.assertEqual(status, 200, path)
            self.assertEqual(headers["Content-Type"], content_type, path)
            self.assertEqual(body, FILES[path.lstrip("/")], path)

    def test_the_mime_table_is_the_documented_one(self):
        self.assertEqual(forgepact.PANEL_MIME, {
            ".html": "text/html; charset=utf-8",
            ".js": "text/javascript",
            ".css": "text/css",
            ".svg": "image/svg+xml",
            ".woff2": "font/woff2",
            ".png": "image/png",
            ".json": "application/json",
            ".ico": "image/x-icon",
        })

    def test_a_path_outside_the_build_is_refused(self):
        # Positive control first: the same file IS reachable once it is inside.
        shutil.copy(Path(self.temp.name) / "secret.txt", self.dist / "inside.txt")
        status, _, body = self.get("/inside.txt")
        self.assertEqual((status, body), (200, SECRET))
        for path in ("/../secret.txt", "/assets/../../secret.txt",
                     "/%2e%2e/secret.txt", "/%2E%2E%2Fsecret.txt",
                     "/..%5csecret.txt", "/assets/..%5c..%5csecret.txt",
                     "/" + str(Path(self.temp.name) / "secret.txt").replace("\\", "/")):
            status, headers, body = self.get(path)
            self.assertEqual(status, 404, path)
            self.assertNotIn(SECRET, body, path)
            self.assertEqual(json.loads(body), {"err": "not found"}, path)

    def test_missing_files_and_directories_are_404_json(self):
        for path in ("/nope.js", "/assets", "/assets/", "/%00"):
            status, headers, body = self.get(path)
            self.assertEqual(status, 404, path)
            self.assertTrue(headers["Content-Type"].startswith("application/json"), path)
            self.assertEqual(json.loads(body), {"err": "not found"}, path)

    def test_api_paths_are_never_answered_from_disk(self):
        status, _, body = self.get("/api/state")
        self.assertEqual(status, 200)
        self.assertIn("version", json.loads(body))
        for path in ("/api/nope", "/api"):
            status, _, body = self.get(path)
            self.assertEqual(status, 404, path)
            self.assertEqual(json.loads(body), {"err": "not found"}, path)

    def test_missing_build_answers_503_naming_the_build_command(self):
        # There is no page to fall back to any more: without a build, / says
        # what to run instead of answering 200 with nothing a player can use.
        # Assets stay 404 and the API keeps answering.
        with patch.object(forgepact, "PANEL_DIST", Path(self.temp.name) / "no-such-dist"):
            for path in ("/", "/?t=1"):
                status, headers, body = self.get(path)
                self.assertEqual(status, 503, path)
                self.assertTrue(headers["Content-Type"].startswith("application/json"), path)
                self.assertEqual(json.loads(body),
                                 {"err": "panel not built: run npm --prefix panel run build"}, path)
            status, _, _ = self.get("/assets/index-abc123.js")
            self.assertEqual(status, 404)
            status, _, body = self.get("/api/state")
            self.assertEqual(status, 200)
            self.assertIn("version", json.loads(body))


class LegacyPageRemovedTests(unittest.TestCase):
    """The page that used to be embedded in forgepact.py is gone, with the
    icon module only it imported; panel/ is the only frontend."""

    SOURCE = (REPO / "src" / "forgepact.py").read_text(encoding="utf-8-sig")

    def test_the_module_no_longer_carries_the_page(self):
        for name in ("HTML", "ICON_SPRITE", "ICON_MAP_JS"):
            self.assertFalse(hasattr(forgepact, name), name)
        self.assertNotIn('HTML = r"""', self.SOURCE)
        self.assertNotIn("</html>", self.SOURCE)

    def test_only_the_watched_fields_remain_of_the_poll_policy(self):
        # The policy itself lives in panel/src/poll-policy.js; Python keeps the
        # field list the parity tests compare against.
        self.assertEqual(sorted(n for n in dir(forgepact) if n.startswith("POLL_")),
                         ["POLL_WATCHED_FIELDS"])
        self.assertEqual(forgepact.POLL_WATCHED_FIELDS,
                         ["gameRunning", "ipcOk", "lastApplied", "queued"])

    def test_the_icon_module_is_deleted_and_not_imported(self):
        self.assertFalse((REPO / "src" / "panel_icons.py").exists())
        self.assertNotIn("panel_icons", self.SOURCE)
        self.assertTrue((REPO / "panel" / "src" / "icons.js").is_file())


class BuildReleaseGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-build-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)

    def run_main(self, **patches):
        out = io.StringIO()
        with contextlib.ExitStack() as stack:
            for name, value in patches.items():
                stack.enter_context(patch.object(build_release, name, value))
            # Nothing past the guards may run from a test.
            run = stack.enter_context(patch.object(build_release.subprocess, "run"))
            stack.enter_context(contextlib.redirect_stdout(out))
            code = build_release.main()
        run.assert_not_called()
        return code, out.getvalue()

    def test_packaging_is_refused_without_the_panel_build(self):
        code, out = self.run_main(PANEL_DIST=self.base / "dist")
        self.assertEqual(code, 1)
        self.assertIn("npm --prefix panel run build", out)

    def test_an_index_less_dist_is_still_refused(self):
        (self.base / "dist" / "assets").mkdir(parents=True)
        code, out = self.run_main(PANEL_DIST=self.base / "dist")
        self.assertEqual(code, 1)
        self.assertIn("npm --prefix panel run build", out)

    def test_with_the_build_present_the_next_guard_decides(self):
        # Negative control for the guard above: with index.html present it
        # passes, and an empty modfiles directory is what stops packaging.
        (self.base / "dist").mkdir()
        (self.base / "dist" / "index.html").write_bytes(INDEX)
        (self.base / "modfiles").mkdir()
        code, out = self.run_main(PANEL_DIST=self.base / "dist",
                                  MODFILES=self.base / "modfiles")
        self.assertEqual(code, 1)
        self.assertNotIn("npm --prefix panel run build", out)
        self.assertIn("modfiles_shipped is incomplete", out)

    def test_the_checkouts_dist_is_the_one_packaged(self):
        self.assertEqual(build_release.PANEL_DIST, REPO / "panel" / "dist")


class PyInstallerBundlesThePanel(unittest.TestCase):
    def command(self):
        build = Path(tempfile.gettempdir()) / "forgepact-build"
        return build_release.pyinstaller_command(build, build / "version_info.txt")

    def add_data(self):
        cmd = self.command()
        pairs = [cmd[i + 1] for i, arg in enumerate(cmd) if arg == "--add-data"]
        self.assertEqual(len(pairs), 1, cmd)
        source, sep, dest = pairs[0].rpartition(";")
        self.assertEqual(sep, ";", "PyInstaller's Windows separator is ';'")
        return Path(source), dest

    def test_the_dist_directory_is_added_as_data(self):
        source, _ = self.add_data()
        self.assertEqual(source, build_release.PANEL_DIST)
        self.assertTrue(source.is_absolute(), "--specpath build/ would re-root a relative path")

    def test_the_destination_is_where_the_frozen_panel_looks(self):
        _, dest = self.add_data()
        with patch.dict(os.environ), \
             patch.object(sys, "frozen", True, create=True), \
             patch.object(sys, "_MEIPASS", r"C:\unpack\_MEI123", create=True):
            os.environ.pop("FORGEPACT_PANEL_DIST", None)
            self.assertEqual(forgepact._panel_dist(), Path(r"C:\unpack\_MEI123") / dest)

    def test_the_existing_command_is_kept(self):
        cmd = self.command()
        for arg in ("--onefile", "--windowed", "--paths", "--version-file", "--clean"):
            self.assertIn(arg, cmd)
        self.assertEqual(cmd[cmd.index("--paths") + 1], str(build_release.SDK_PY))
        self.assertEqual(cmd[-1], str(build_release.SRC))
        excluded = {cmd[i + 1] for i, arg in enumerate(cmd) if arg == "--exclude-module"}
        self.assertTrue({"tkinter", "PIL", "numpy", "pytest"} <= excluded)


class PackagedPanelToolsTests(unittest.TestCase):
    """The pure parts of tools/panel_smoke.py and tools/package_size.py; the
    exe itself is exercised by running them (instructions.md, Command Reference)."""

    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, str(REPO / "tools"))
        import package_size
        import panel_smoke
        cls.smoke, cls.size = panel_smoke, package_size

    def test_the_smoke_polls_the_panels_own_ports(self):
        self.assertEqual(self.smoke.port_candidates(), forgepact.PORT_CANDIDATES)

    def test_the_first_script_asset_is_found_in_a_vite_index(self):
        for src in ("/assets/index-D1x.js", "./assets/index-D1x.js"):
            html = f'<link rel="stylesheet" href="/assets/index-a.css"><script type="module" crossorigin src="{src}"></script>'
            self.assertEqual(self.smoke.first_asset(html.encode()), "/assets/index-D1x.js")
        self.assertIsNone(self.smoke.first_asset(b'<link href="/assets/index-a.css">'))

    def test_each_check_needs_the_real_answer(self):
        self.assertTrue(self.smoke.index_ok((200, "text/html", INDEX)))
        self.assertFalse(self.smoke.index_ok((200, "text/html", b"<!DOCTYPE html><div id=\"legacy\">")))
        self.assertFalse(self.smoke.index_ok(None))
        self.assertTrue(self.smoke.asset_ok((200, "text/javascript", b"")))
        self.assertFalse(self.smoke.asset_ok((200, "application/octet-stream", b"")))
        self.assertFalse(self.smoke.asset_ok((404, "text/javascript", b"")))
        self.assertTrue(self.smoke.api_ok((200, "application/json", b'{"version": "1.4.5"}')))
        self.assertFalse(self.smoke.api_ok((200, "application/json", b'{"err": "x"}')))
        self.assertFalse(self.smoke.api_ok((200, "text/html", b"<html>")))

    def test_the_verdict_line_shape(self):
        self.assertEqual(self.smoke.verdict_line(True, "http://127.0.0.1:8780", True, True, True),
                         "window=found url=http://127.0.0.1:8780 index=ok assets=ok api=ok")
        self.assertEqual(self.smoke.verdict_line(False, None, False, False, False),
                         "window=missing url=none index=bad assets=bad api=bad")

    def test_a_tree_is_the_forgepact_checkout_or_the_toolkit_holding_it(self):
        self.assertEqual(self.size.resolve_tree(REPO), REPO)
        if (REPO.parent / "ForgePact" / "build_release.py").is_file():
            self.assertEqual(self.size.resolve_tree(REPO.parent), REPO.parent / "ForgePact")
        with tempfile.TemporaryDirectory() as empty, self.assertRaises(SystemExit):
            self.size.resolve_tree(Path(empty))


if __name__ == "__main__":
    unittest.main()
