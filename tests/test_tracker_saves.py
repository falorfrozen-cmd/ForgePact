"""The Tracker's read-only character save reader, ported (#185).

`src/tracker_saves.py` is HS Offline Tracker v0.1.4's `save_source.rs` in
Python. The first twelve tests are that file's own tests, one for one and
under the same names; the saves they read are encoded here, from the same
text the Rust tests use, so no test reads a real save. Every root is a
temporary directory.

The reader turns a character slot into protocol v1 `progress` dicts
(HS-Offline-Tracker's PROTOCOL.md, "Character progress") and accepts a save
only after two identical reads on two polls, so a write caught half-way is
never reported. The tests after the ported ones pin what the port adds:

  - a slot rewritten as one NUL byte is an empty slot, the way the game
    leaves a deleted character (`docs/RUNTIME_DATA_MODELS.md` section 8.2,
    "Unused and deleted character slots"), not a damaged save; a damaged
    save beside it still raises, as the negative control;
  - polling never changes a save's bytes or its modification time.
"""
import base64
import os
import sys
import tempfile
import time
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import tracker_saves  # noqa: E402
from tracker_saves import (  # noqa: E402
    HSS_XOR_KEY,
    MAX_SAVE_BYTES,
    EmptySlot,
    SaveError,
    SaveWatcher,
    decode_hss,
    progress_event,
)


def encode_hss(text):
    """The save's three layers applied in the game's order, for fixtures."""
    payload = text.encode("utf-8")
    utf16_like = bytearray(len(payload) * 2)
    utf16_like[0::2] = payload
    key = (HSS_XOR_KEY * (len(utf16_like) // len(HSS_XOR_KEY) + 1))[: len(utf16_like)]
    obfuscated = (
        int.from_bytes(utf16_like, "little") ^ int.from_bytes(key, "little")
    ).to_bytes(len(utf16_like), "little")
    return base64.b64encode(zlib.compress(obfuscated, 9)) + b"\x00"


def save(name, xp, kills):
    return encode_hss(
        f'[0]\nname="{name}"\nclass="13.000000"\nlevel="100.000000"\n'
        f'herolevel="9.000000"\nexperience="{xp}.000000"\ndifficulty="3.000000"\n'
        f'hardcore="0.000000"\nact_previous_0="1.000000"\nact_previous_1="9.000000"\n'
        f'[stat]\nstatisticTotalMonsterKills="{kills}.000000"\n'
        f'statisticUberPhantomLeviathanKills="4.000000"\n'
        f'statisticCrystalChestOpened="23.000000"\n'
    )


def account_values(event):
    return (event["name"], event["experience"], event["kills"])


class _Clock:
    """Strictly increasing modification times for the fixtures' writes.

    The reader tells a new write from an old one by size and modification
    time, as the Tracker does. Two writes inside one timestamp tick would
    look like no write at all, so every fixture write gets its own time,
    a millisecond after the last, close to now.
    """

    def __init__(self):
        self.next_ns = time.time_ns() - 10_000_000_000

    def write(self, path, data):
        Path(path).write_bytes(data)
        self.next_ns += 1_000_000
        os.utime(path, ns=(self.next_ns, self.next_ns))


class SaveSourceTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix="fp-tracker-saves-")
        self.root = Path(self._tmp.name) / "Hero_Siege"
        self.clock = _Clock()

    def tearDown(self):
        self._tmp.cleanup()

    def nested(self):
        nested = self.root / "hs2saves"
        nested.mkdir(parents=True, exist_ok=True)
        return nested

    # ------------------------------------------------------------------
    # Ported from save_source.rs, one for one.

    def test_synthetic_s10_save_becomes_only_an_account_snapshot(self):
        text = decode_hss(save("Test Hero", 728_844_020, 91_420))
        event = progress_event(text)
        self.assertEqual(event["protocol"], "hs-offline-tracker/1")
        self.assertEqual(event["v"], 1)
        self.assertEqual(event["kind"], "progress")
        self.assertEqual(event["name"], "Test Hero")
        # A local save carries no season or Blood Pact field, so the record
        # asserts no mode at all rather than a made-up one.
        self.assertNotIn("season", event, "local saves must not assert a season/currency mode")
        self.assertNotIn("blood_pact", event)
        self.assertEqual(
            (
                event["experience"],
                event["level"],
                event["hero_level"],
                event["difficulty"],
                event["act"],
                event["kills"],
            ),
            (728_844_020, 100, 9, 3, 9, 91_420),
        )
        self.assertEqual(event["tallies"]["statisticuberphantomleviathankills"], 4)
        self.assertEqual(event["tallies"]["statisticcrystalchestopened"], 23)
        self.assertIs(event["armed_baseline"], False)

    def test_raw_and_decompressed_limits_are_both_enforced(self):
        with self.assertRaises(SaveError) as raw:
            decode_hss(b"A" * (MAX_SAVE_BYTES + 1))
        self.assertIn("raw save", str(raw.exception))

        encoded = encode_hss("x" * (MAX_SAVE_BYTES // 2 + 1))
        self.assertLess(
            len(encoded), MAX_SAVE_BYTES, "fixture must exercise only the decompressed limit"
        )
        with self.assertRaises(SaveError) as inflated:
            decode_hss(encoded)
        self.assertIn("decompressed save", str(inflated.exception))

    def test_truncated_and_corrupted_layers_fail_closed(self):
        encoded = save("Test Hero", 10, 2)
        for cut in (1, 2, 7, len(encoded) // 2, len(encoded) - 2):
            with self.subTest(cut=cut), self.assertRaises(SaveError):
                decode_hss(encoded[:cut])
        internal_nul = bytearray(encoded)
        internal_nul[8] = 0
        with self.assertRaises(SaveError) as caught:
            decode_hss(bytes(internal_nul))
        self.assertIn("NUL", str(caught.exception))

    def test_nested_saves_take_priority_and_require_two_identical_reads(self):
        nested = self.nested()
        legacy_path = self.root / "herosiege1.hss"
        self.clock.write(legacy_path, save("Legacy", 10, 1))
        nested_path = nested / "herosiege2.hss"
        self.clock.write(nested_path, save("Nested", 20, 2))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.assertEqual(watcher.poll(), [], "baseline must not guess an active slot")

        self.clock.write(legacy_path, save("Legacy", 30, 3))
        self.assertEqual(watcher.poll(), [], "legacy root is fallback only")

        self.clock.write(nested_path, save("Nested", 40, 4))
        self.assertEqual(watcher.poll(), [], "first stable observation must remain pending")
        events = watcher.poll()
        self.assertEqual(len(events), 2, "the armed baseline precedes the new save")
        self.assertEqual((events[1]["experience"], events[1]["kills"]), (40, 4))
        self.assertEqual(watcher.poll(), [], "unchanged saves must be deduplicated")

        watcher.game_stopped()
        self.assertEqual(watcher.poll(), [])

    def test_legacy_root_is_used_only_when_nested_has_no_character_slots(self):
        self.root.mkdir(parents=True)
        legacy_path = self.root / "herosiege3.hss"
        self.clock.write(legacy_path, save("Legacy", 10, 1))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.clock.write(legacy_path, save("Legacy", 300, 3))
        self.assertEqual(watcher.poll(), [])
        events = watcher.poll()
        self.assertEqual(len(events), 2)
        self.assertEqual((events[1]["experience"], events[1]["kills"]), (300, 3))

    def test_first_delayed_write_returns_the_armed_baseline_then_current_progress(self):
        path = self.nested() / "herosiege1.hss"
        self.clock.write(path, save("Sgham", 270_653, 44_023))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.clock.write(path, save("Sgham", 0, 44_138))
        self.assertEqual(watcher.poll(), [])
        events = watcher.poll()

        self.assertEqual(len(events), 2)
        self.assertEqual(account_values(events[0]), ("Sgham", 270_653, 44_023))
        self.assertEqual(account_values(events[1]), ("Sgham", 0, 44_138))
        self.assertEqual([e["armed_baseline"] for e in events], [True, False])

    def test_changing_characters_does_not_replay_another_armed_slot_baseline(self):
        nested = self.nested()
        main = nested / "herosiege1.hss"
        alt = nested / "herosiege2.hss"
        self.clock.write(main, save("Main", 1_000, 100))
        self.clock.write(alt, save("Alt", 2_000, 200))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.clock.write(main, save("Main", 1_050, 105))
        self.assertEqual(watcher.poll(), [])
        self.assertEqual(len(watcher.poll()), 2)

        self.clock.write(alt, save("Alt", 2_040, 204))
        self.assertEqual(watcher.poll(), [])
        events = watcher.poll()
        self.assertEqual(len(events), 1, "the alt's pre-session save is not replayed")
        self.assertEqual(account_values(events[0]), ("Alt", 2_040, 204))

    def test_equally_new_slot_writes_are_rejected_as_ambiguous(self):
        nested = self.nested()
        one = nested / "herosiege1.hss"
        two = nested / "herosiege2.hss"
        self.clock.write(one, save("One", 10, 1))
        self.clock.write(two, save("Two", 20, 2))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.clock.write(one, save("One", 30, 3))
        self.clock.write(two, save("Two", 40, 4))
        tied = time.time_ns() + 30_000_000_000
        for path in (one, two):
            os.utime(path, ns=(tied, tied))

        self.assertEqual(watcher.poll(), [])
        self.assertEqual(
            watcher.poll(), [], "a filename tie-break would guess which character is active"
        )

    def test_truncated_write_is_never_emitted_and_a_later_stable_save_recovers(self):
        path = self.nested() / "herosiege1.hss"
        self.clock.write(path, save("Safe", 100, 10))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        self.clock.write(path, b"truncated base64 payload")
        self.assertEqual(watcher.poll(), [])

        self.clock.write(path, save("Safe", 150, 15))
        self.assertEqual(
            watcher.poll(), [], "the changed second read only replaces the pending half"
        )
        events = watcher.poll()
        self.assertEqual(len(events), 2)
        self.assertEqual(account_values(events[0]), ("Safe", 100, 10))
        self.assertEqual(account_values(events[1]), ("Safe", 150, 15))

    def test_a_save_written_since_the_game_started_is_bootstrapped_safely(self):
        self.clock.write(self.nested() / "herosiege7.hss", save("Already Running", 777, 12))

        watcher = SaveWatcher(self.root)
        watcher.game_started_since(0.0)
        events = watcher.poll()
        self.assertEqual(len(events), 1)
        self.assertEqual((events[0]["experience"], events[0]["kills"]), (777, 12))

    def test_a_save_older_than_the_running_game_is_not_guessed_as_active(self):
        self.clock.write(self.nested() / "herosiege8.hss", save("Old Slot", 88, 8))

        watcher = SaveWatcher(self.root)
        watcher.game_started_since(time.time() + 60)
        self.assertEqual(watcher.poll(), [])

    def test_placeholder_inventory_and_unpersisted_runtime_values_are_not_events(self):
        placeholder = encode_hss(
            '[0]\nname="New Char"\nclass="1.000000"\nlevel="1.000000"\n'
            'experience="0.000000"\ninventory="not-telemetry"\nmf="999999.000000"\n'
            'room="Act_09_09"\n'
        )
        text = decode_hss(placeholder)
        with self.assertRaises(SaveError):
            progress_event(text)

    # ------------------------------------------------------------------
    # What the port adds.

    def test_a_nul_only_slot_reads_as_empty_not_damaged(self):
        with self.assertRaises(EmptySlot):
            decode_hss(b"\x00")
        self.assertTrue(issubclass(EmptySlot, SaveError))

        nested = self.nested()
        live = nested / "herosiege1.hss"
        deleted = nested / "herosiege2.hss"
        self.clock.write(live, save("Live", 500, 50))
        self.clock.write(deleted, save("Gone", 900, 90))

        watcher = SaveWatcher(self.root)
        watcher.game_started()
        # Deleting a character at character select leaves its slot as one
        # NUL byte: the reader passes over it without an error.
        self.clock.write(deleted, b"\x00")
        self.assertEqual(watcher.poll(), [])
        self.assertEqual(watcher.poll(), [])
        self.assertEqual(watcher.poll(), [])

        # The empty slot did not stand in for the first real write: the live
        # slot's first write still carries its armed baseline.
        self.clock.write(live, save("Live", 520, 52))
        self.assertEqual(watcher.poll(), [])
        events = watcher.poll()
        self.assertEqual([account_values(e) for e in events], [("Live", 500, 50), ("Live", 520, 52)])
        self.assertEqual([e["armed_baseline"] for e in events], [True, False])

        # Negative control: a damaged save in a slot still raises.
        self.clock.write(deleted, b"!!!! not a save")
        self.assertEqual(watcher.poll(), [])
        with self.assertRaises(SaveError) as caught:
            watcher.poll()
        self.assertNotIsInstance(caught.exception, EmptySlot)

    def test_a_slot_with_no_slot_number_is_not_a_character_save(self):
        nested = self.nested()
        self.clock.write(nested / "herosiege.hss", save("Nameless", 1, 1))
        self.clock.write(nested / "ether1.hss", save("Ether", 1, 1))
        self.clock.write(nested / "herosiege1.hss.bak", save("Backup", 1, 1))
        watcher = SaveWatcher(self.root)
        self.assertEqual(watcher.candidate_files(), [])
        self.clock.write(nested / "HeroSiege12.HSS", save("Upper", 1, 1))
        self.assertEqual([p.name for p in watcher.candidate_files()], ["HeroSiege12.HSS"])

    def test_the_reader_never_writes_a_save(self):
        nested = self.nested()
        paths = [nested / "herosiege1.hss", nested / "herosiege2.hss"]
        self.clock.write(paths[0], save("One", 10, 1))
        self.clock.write(paths[1], b"\x00")

        def state():
            return [(p.read_bytes(), p.stat().st_mtime_ns, p.stat().st_size) for p in paths]

        before = state()
        watcher = SaveWatcher(self.root)
        watcher.game_started_since(0.0)
        for _ in range(4):
            watcher.poll()
        watcher.game_stopped()
        self.assertEqual(state(), before)
        self.assertEqual(sorted(p.name for p in nested.iterdir()), ["herosiege1.hss", "herosiege2.hss"])

    def test_the_module_imports_only_the_standard_library(self):
        source = Path(tracker_saves.__file__).read_text(encoding="utf-8")
        imported = set()
        for line in source.splitlines():
            words = line.split()
            if len(words) >= 2 and words[0] in ("import", "from") and not line.startswith(" "):
                imported.add(words[1].split(".")[0])
        allowed = set(sys.stdlib_module_names) | {"__future__"}
        self.assertEqual(sorted(imported - allowed), [])


if __name__ == "__main__":
    unittest.main()
