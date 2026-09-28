"""The panel sandbox's listening socket holds a browser's burst of connections.

On Windows, a connection that finds a listener's accept queue full is refused
at once: Node reports `connect ECONNREFUSED`, Edge `net::ERR_CONNECTION_REFUSED`,
although the server is up. socketserver's queue is 5 deep, and headless Edge
opens up to six connections to the panel at a time (the fonts and pictures
right after boot). While the sandbox's accept loop is starved of CPU, as on
the release job's four-core runner beside three other browser suites, that
burst filled the queue, and the first Node-side `sandbox.state()` after boot
was refused: 2.0.0's release run 36372423744, attempt 1, `panel_ui` and
`enabled_mods` in `npm run e2e`. `test_satanic_panel._SandboxServer` gives
the sandbox a queue of 128.

Real loopback sockets, but nothing here is served: the server under test is
never started, so every connection stays in the queue, the state a starved
accept loop leaves it in.
"""
import socket
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_satanic_panel as panel_mod  # noqa: E402

# More connections than headless Edge opens to one origin at once (six), with
# a suite's own Node-side requests on top.
BURST = 16


def connect_burst(port, count):
    """Connect `count` times to 127.0.0.1:port; return the connected sockets
    and the first error, None when every connect succeeded. Stops at the
    first error."""
    clients = []
    for _ in range(count):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(10)
        try:
            client.connect(("127.0.0.1", port))
        except OSError as error:
            client.close()
            return clients, error
        clients.append(client)
    return clients, None


class SandboxAcceptQueueTests(unittest.TestCase):
    def burst(self, server):
        self.addCleanup(server.server_close)
        clients, error = connect_burst(server.server_port, BURST)
        for client in clients:
            self.addCleanup(client.close)
        return clients, error

    def test_the_sandbox_server_queues_a_burst_nobody_has_accepted(self):
        self.assertGreaterEqual(panel_mod._SandboxServer.request_queue_size, BURST)
        server = panel_mod._SandboxServer(("127.0.0.1", 0), panel_mod.forgepact.H)
        clients, error = self.burst(server)
        self.assertIsNone(error)
        self.assertEqual(len(clients), BURST)

    @unittest.skipUnless(sys.platform == "win32",
                         "Windows refuses a connection to a full queue; Linux leaves it waiting")
    def test_the_stock_server_refuses_the_same_burst(self):
        # The instrument: forgepact's own server class, with socketserver's
        # queue, refuses part of the burst the sandbox's server holds.
        server = panel_mod.forgepact.ThreadingHTTPServer(("127.0.0.1", 0), panel_mod.forgepact.H)
        clients, error = self.burst(server)
        self.assertIsInstance(error, ConnectionRefusedError)
        self.assertLess(len(clients), BURST)

    def test_panel_sandbox_serves_through_the_sandbox_server(self):
        with panel_mod.PanelSandbox() as sandbox:
            self.assertIsInstance(sandbox.server, panel_mod._SandboxServer)
            code, state = sandbox.request()
        self.assertEqual(code, 200)
        self.assertEqual(state["cfg"]["density"], 3)


if __name__ == "__main__":
    unittest.main()
