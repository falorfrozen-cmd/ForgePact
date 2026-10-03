"""A client that goes away mid-answer is a closed client, not a server error.

The page closing while an /api/state poll is in flight aborts the socket under
the panel's answer, and the write raised ConnectionAbortedError (WinError
10053) out of the handler. socketserver printed a full traceback per request,
and in the forgepact-74 final gate those tracebacks filled the 4000-character
tail test_panel_perf.py quotes, so the perf suite's own verdict lines were cut
from its failure message. The handler now ends such a request quietly; any
other write error still raises.
"""
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact


class GoneWriter(io.RawIOBase):
    """A response stream whose peer has gone: every write raises `error`."""

    def __init__(self, error):
        self.error = error

    def writable(self):
        return True

    def write(self, b):
        raise self.error


def handler(wfile):
    h = forgepact.H.__new__(forgepact.H)
    h.wfile = wfile
    h.request_version = "HTTP/1.1"
    h.requestline = "GET /api/state HTTP/1.1"
    h.command = "GET"
    h.client_address = ("127.0.0.1", 0)
    h.close_connection = False
    return h


class ClientGoneTests(unittest.TestCase):
    GONE = (ConnectionAbortedError(10053, "aborted"), ConnectionResetError(10054, "reset"),
            BrokenPipeError(32, "broken pipe"))

    def test_a_json_answer_to_a_gone_client_ends_the_request_quietly(self):
        for error in self.GONE:
            with self.subTest(type(error).__name__):
                h = handler(GoneWriter(error))
                h._json({"ok": True})
                self.assertTrue(h.close_connection)

    def test_a_file_answer_to_a_gone_client_ends_the_request_quietly(self):
        with tempfile.TemporaryDirectory() as d:
            page = Path(d) / "index.html"
            page.write_text("<!doctype html>", encoding="utf-8")
            for error in self.GONE:
                with self.subTest(type(error).__name__):
                    h = handler(GoneWriter(error))
                    h._file(page)
                    self.assertTrue(h.close_connection)

    def test_any_other_write_error_still_raises(self):
        # The negative control: only a client that went away is swallowed.
        for error in (PermissionError(13, "denied"), OSError(5, "io")):
            with self.subTest(type(error).__name__):
                with self.assertRaises(type(error)):
                    handler(GoneWriter(error))._json({"ok": True})

    def test_a_present_client_gets_the_whole_answer(self):
        out = io.BytesIO()
        h = handler(out)
        h._json({"ok": True}, 201)
        head, _, body = out.getvalue().partition(b"\r\n\r\n")
        self.assertTrue(head.startswith(f"{forgepact.H.protocol_version} 201".encode()), head)
        self.assertIn(b"Content-Length: 12", head)
        self.assertEqual(body, b'{"ok": true}')
        self.assertFalse(h.close_connection)


if __name__ == "__main__":
    unittest.main()
