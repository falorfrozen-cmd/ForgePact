"""bind_safe_server and PanelSandbox never hand headless Edge a Chromium-
restricted port. No real socket: every bind is a fake object recording its
calls in one shared event list, per docs/submodules/ForgePact/instructions.md
and ctx "### The unit tests" in forgepact-ui-sandbox-ports-context.md."""
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_satanic_panel as panel_mod


class FakeServer:
    """Stands in for test_satanic_panel._SandboxServer: server_port plus
    the three calls PanelSandbox and bind_safe_server make on it, each
    recorded on the shared events list passed in."""

    def __init__(self, port, events):
        self.server_port = port
        self._events = events
        self._events.append(("bind", port))

    def serve_forever(self):
        self._events.append(("serve_forever", self.server_port))

    def shutdown(self):
        self._events.append(("shutdown", self.server_port))

    def server_close(self):
        self._events.append(("close", self.server_port))


class BindSafeServerTests(unittest.TestCase):
    def test_returns_first_safe_port_after_one_bind(self):
        events = []
        server = panel_mod.bind_safe_server(lambda: FakeServer(50000, events))
        self.assertEqual(server.server_port, 50000)
        self.assertEqual(events, [("bind", 50000)])

    def test_rebinds_past_two_restricted_ports_and_closes_both_after(self):
        events = []
        ports = iter([1719, 6000, 50000])
        server = panel_mod.bind_safe_server(lambda: FakeServer(next(ports), events))
        self.assertEqual(server.server_port, 50000)
        self.assertEqual(events, [
            ("bind", 1719),
            ("bind", 6000),
            ("bind", 50000),
            ("close", 1719),
            ("close", 6000),
        ])

    def test_exhaustion_raises_and_closes_every_bound_server(self):
        events = []
        with self.assertRaises(RuntimeError) as caught:
            panel_mod.bind_safe_server(lambda: FakeServer(1719, events),
                                        max_attempts=3)
        self.assertIn("1719", str(caught.exception))
        closes = [event for event in events if event[0] == "close"]
        self.assertEqual(len(closes), 3)


class PanelSandboxRebindTests(unittest.TestCase):
    def test_panel_sandbox_binds_once_when_first_port_is_safe(self):
        events = []
        with patch.object(panel_mod, "_SandboxServer",
                           side_effect=lambda *a, **k: FakeServer(50000, events)):
            with panel_mod.PanelSandbox() as sandbox:
                self.assertEqual(sandbox.port, 50000)
                self.assertEqual([e for e in events if e[0] in ("bind", "close")],
                                  [("bind", 50000)])

    def test_panel_sandbox_rebinds_off_a_restricted_port(self):
        events = []
        servers = iter([1719, 50000])
        with patch.object(panel_mod, "_SandboxServer",
                           side_effect=lambda *a, **k: FakeServer(next(servers), events)):
            with panel_mod.PanelSandbox() as sandbox:
                self.assertEqual(sandbox.port, 50000)
        self.assertIn(("close", 1719), events)


class RestrictedPortConstantTests(unittest.TestCase):
    def test_holds_the_measured_restricted_ports_and_no_others(self):
        self.assertIn(1719, panel_mod.CHROMIUM_RESTRICTED_PORTS)
        self.assertIn(6000, panel_mod.CHROMIUM_RESTRICTED_PORTS)
        self.assertNotIn(8780, panel_mod.CHROMIUM_RESTRICTED_PORTS)
        self.assertNotIn(50000, panel_mod.CHROMIUM_RESTRICTED_PORTS)


if __name__ == "__main__":
    unittest.main()
