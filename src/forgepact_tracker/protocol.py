"""Reading one line of HS Offline Tracker's protocol v1.

The plugin's `tracker` mod writes the same records the Tracker's producer
does (`HS-Offline-Tracker/PROTOCOL.md`): one complete JSON object and a
newline per record. This module turns one line into a record dict, or says
why it cannot, by the Tracker's own reader rules (`sniffer.rs`,
`decode_line`):

- a line longer than 1 MiB is refused;
- a record identifies version 1 by `"protocol": "hs-offline-tracker/1"`, by
  `"v": 1`, or by both, and when it carries both they must agree;
- a record whose kind protocol v1 does not define is refused.

A refused line is counted and skipped by the caller; it never stops a read.
"""

from __future__ import annotations

import json
import re

PROTOCOL_ID = "hs-offline-tracker/1"
PROTOCOL_VERSION = 1
MAX_LINE_BYTES = 1024 * 1024

# What the plugin's sensor writes.
SENSOR_KINDS = frozenset({
    "bridge_status", "session_delta", "ground_drop", "room", "satanic_zone", "vitals", "progress",
    "heartbeat",
})

# Every other kind protocol v1 defines. These are read without error and change
# nothing: they belong to the Tracker's online sources or its own producer, and
# the ForgePact sensor does not write them.
OTHER_KINDS = frozenset({
    "character", "drop_spawned", "gold", "currency", "xp_gain", "sensor_diagnostic", "account_id",
    "mail", "item_picked_up", "drop", "items_let_go", "game_started", "game_stopped",
})

KINDS = SENSOR_KINDS | OTHER_KINDS

SESSION_FILE = re.compile(r"^session-(?P<pid>\d+)-(?P<ms>\d+)\.ndjson$")


class ProtocolError(ValueError):
    """A line that is not a protocol v1 record. `reason` is a short tag for
    the counter that records it."""

    def __init__(self, reason: str, detail: str = ""):
        super().__init__("%s: %s" % (reason, detail) if detail else reason)
        self.reason = reason


def parse_line(raw: bytes) -> dict:
    """One record from one line (its newline already removed)."""
    if len(raw) > MAX_LINE_BYTES:
        raise ProtocolError("too_long", "%d bytes" % len(raw))
    try:
        record = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise ProtocolError("malformed", str(exc)) from None
    if not isinstance(record, dict):
        raise ProtocolError("malformed", "not an object")
    has_version = "v" in record
    has_protocol = "protocol" in record
    if not has_version and not has_protocol:
        raise ProtocolError("unversioned")
    if has_version and (isinstance(record["v"], bool) or record["v"] != PROTOCOL_VERSION):
        raise ProtocolError("version", repr(record["v"]))
    if has_protocol and record["protocol"] != PROTOCOL_ID:
        raise ProtocolError("version", repr(record["protocol"]))
    kind = record.get("kind")
    if not isinstance(kind, str) or kind not in KINDS:
        raise ProtocolError("unknown_kind", repr(kind))
    return record


def session_file_identity(name: str) -> tuple[int, int] | None:
    """(pid, unix ms at open) from a session file name, or None when the name
    is not one (`session-<pid>-<unix ms>.ndjson`)."""
    m = SESSION_FILE.match(name)
    if not m:
        return None
    return int(m.group("pid")), int(m.group("ms"))
