"""The panel's ports stay clear of the other toolkit tools' ports (1.4.6).

The Item Editor keeps 8765-8774 for itself, and the Toolkit Hub checks
ForgePact on its first candidate port. While 8766 led the list, an open editor
held it: the panel landed on 8780 anyway, and the hub's check of 8766 reached
the editor. The ports below are each tool's own, as the hub's
catalog/sources.toml and the tools' sources recorded them on 2026-09-25.

When every candidate is busy the OS picks the port, and it can pick one
WebView2 refuses (net::ERR_UNSAFE_PORT): a blank panel window. The bind tests
below inject the bind, so they never race the OS for a real port.
"""
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact

OTHER_TOOLS = {
    # hs_item_editor_gui.py binds PORT..PORT + 9 and keeps all ten.
    "Item Editor": set(range(8765, 8775)),
    # HS-AFK-Expedition tools/panel.py, the --port default.
    "AFK FARM panel": {8787},
    # catalog/sources.toml, [tool.launch] ports.
    "HS Offline Launcher": {8861, 8862, 8863, 8961},
    "HSCraftSim": set(range(17870, 17880)),
}


class PanelPortTests(unittest.TestCase):
    def test_the_panel_starts_on_its_first_candidate(self):
        self.assertEqual(forgepact.PORT, forgepact.PORT_CANDIDATES[0])

    def test_the_first_candidate_is_the_port_the_hub_checks(self):
        # The hub's catalog/sources.toml checks http://127.0.0.1:8780/
        # ([tool.launch.health]); move the two together.
        self.assertEqual(forgepact.PORT_CANDIDATES[0], 8780)

    def test_no_candidate_belongs_to_another_toolkit_tool(self):
        for tool, ports in OTHER_TOOLS.items():
            with self.subTest(tool=tool):
                self.assertFalse(set(forgepact.PORT_CANDIDATES) & ports)

    def test_no_candidate_is_a_port_the_panel_window_refuses(self):
        self.assertFalse(set(forgepact.PORT_CANDIDATES)
                         & forgepact.CHROMIUM_RESTRICTED_PORTS)


class FakeServer:
    """Stands in for ThreadingHTTPServer: a port and a close, no socket."""

    def __init__(self, port, log):
        self.server_port = port
        self.closed = False
        self.log = log

    def server_close(self):
        self.closed = True
        self.log.append(("close", self.server_port))

    def serve_forever(self):
        raise AssertionError("threading.Thread is patched; nothing serves")


def fake_binds(ports):
    """A bind() that hands out `ports` in order, logging every bind and close,
    so a test sees the order without racing the OS for a real port."""
    log, made = [], []

    def bind():
        server = FakeServer(ports[len(made)], log)
        made.append(server)
        log.append(("bind", server.server_port))
        return server
    return bind, log, made


class BindSafeServerTests(unittest.TestCase):
    """The port-0 fallback never leaves the panel on a port WebView2 refuses
    (net::ERR_UNSAFE_PORT), which is a blank window for the player."""

    def test_a_safe_first_port_is_kept(self):
        bind, log, made = fake_binds([50123])
        self.assertIs(forgepact.bind_safe_server(bind), made[0])
        self.assertEqual(log, [("bind", 50123)])

    def test_a_restricted_port_is_rebound_and_held_until_a_safe_one_is_bound(self):
        bind, log, made = fake_binds([6665, 6666, 50123])
        server = forgepact.bind_safe_server(bind)
        self.assertEqual(server.server_port, 50123)
        self.assertFalse(server.closed)
        # Both rejects stay open through every later bind, so the OS cannot
        # hand the same port back; they are closed only once 50123 is held.
        self.assertEqual(log, [("bind", 6665), ("bind", 6666), ("bind", 50123),
                               ("close", 6665), ("close", 6666)])

    def test_exhausting_the_attempts_refuses_and_closes_everything(self):
        bind, log, made = fake_binds([6000, 10080, 1719])
        with self.assertRaises(RuntimeError) as caught:
            forgepact.bind_safe_server(bind, max_attempts=3)
        self.assertIn("[6000, 10080, 1719]", str(caught.exception))
        self.assertTrue(all(s.closed for s in made))
        self.assertEqual(len(made), 3)

    def test_the_attempts_are_bounded_by_default(self):
        ports = [6000] * (forgepact.SAFE_BIND_ATTEMPTS + 1)
        bind, log, made = fake_binds(ports)
        with self.assertRaises(RuntimeError):
            forgepact.bind_safe_server(bind)
        self.assertEqual(len(made), forgepact.SAFE_BIND_ATTEMPTS)


class MainPortFallbackTests(unittest.TestCase):
    """main() with every candidate busy: the OS's port is checked before any
    window opens."""

    def run_main(self, port0_ports):
        bind, log, made = fake_binds(port0_ports)

        def server(addr, handler):
            if addr[1] != 0:
                raise OSError("reserved")  # every candidate is refused
            return bind()
        webview = mock.MagicMock()
        with mock.patch.object(forgepact, "PORT", forgepact.PORT), \
                mock.patch.object(forgepact.socket, "create_connection",
                                  side_effect=OSError), \
                mock.patch.object(forgepact, "ThreadingHTTPServer", side_effect=server), \
                mock.patch.object(forgepact, "refuse_to_start") as refuse, \
                mock.patch.object(forgepact.threading, "Thread") as thread, \
                mock.patch.dict(sys.modules, {"webview": webview}):
            forgepact.main()
            return forgepact.PORT, made, refuse, thread, webview

    def test_a_restricted_port_from_the_os_is_skipped(self):
        port, made, refuse, thread, webview = self.run_main([6000, 50123])
        self.assertEqual(port, 50123)
        self.assertTrue(made[0].closed)
        refuse.assert_not_called()
        self.assertIn("http://127.0.0.1:50123",
                      webview.create_window.call_args.args)

    def test_no_safe_port_refuses_instead_of_opening_a_blank_window(self):
        port, made, refuse, thread, webview = self.run_main(
            [6000] * forgepact.SAFE_BIND_ATTEMPTS)
        refuse.assert_called_once()
        message = refuse.call_args.args[0]
        for candidate in forgepact.PORT_CANDIDATES:
            self.assertIn(str(candidate), message)
        webview.create_window.assert_not_called()
        thread.assert_not_called()
        self.assertTrue(all(s.closed for s in made))


if __name__ == "__main__":
    unittest.main()
