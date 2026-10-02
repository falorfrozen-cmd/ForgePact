"""`room` is a GameMaker built-in, never read through `variable_global_get` (#144).

`variable_global_exists("room")` answers false on this runner (the `roomprobe`
control recorded beside `CurrentRoomKey()`), so `variable_global_get("room")`
returns undefined. Converting that with `.ToDouble()` raised the runner error
`REAL argument incorrect type undefined` once per `DropKeys` call - 259 of them
in one session of kills - and the research `keychoice` log never recorded a
room. The same holds for the other room built-ins (`room_width`,
`room_height`, ...), so the whole family is pinned here.
"""

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_DIR = PROJECT_ROOT / "plugin"
MODULE_MAIN = PLUGIN_DIR / "ModuleMain.cpp"
DROP_MANAGER = PLUGIN_DIR / "include" / "ForgePact" / "DropManager.hpp"

# A room built-in fetched as if it were a user global.
GLOBAL_ROOM_READ = re.compile(
    r'variable_global_get"\s*,\s*\{\s*RValue\(\s*"room(?:_[a-z_]+)?"\s*\)'
)


def plugin_sources():
    return sorted(
        p for p in PLUGIN_DIR.rglob("*")
        if p.suffix in (".cpp", ".hpp", ".h") and "yytoolkit" not in p.as_posix().lower()
    )


def function_body(source: str, signature: str) -> str:
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"{signature} not found")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated body for {signature}")


class RoomBuiltinReadContract(unittest.TestCase):
    def test_pattern_catches_the_shipped_bug(self):
        # Negative control: the exact line removed by #144 must match, so a
        # pattern that has drifted cannot pass vacuously.
        bad = 'RValue rm = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") });'
        self.assertRegex(bad, GLOBAL_ROOM_READ)
        self.assertRegex('num("variable_global_get", { RValue("room_width") })', GLOBAL_ROOM_READ)
        self.assertNotRegex('CallBuiltin("variable_global_get", { RValue("roomCount") })', GLOBAL_ROOM_READ)

    def test_no_room_builtin_is_read_as_a_global(self):
        offenders = []
        for path in plugin_sources():
            text = path.read_text(encoding="utf-8", errors="replace")
            for number, line in enumerate(text.splitlines(), 1):
                if GLOBAL_ROOM_READ.search(line):
                    offenders.append(f"{path.relative_to(PROJECT_ROOT)}:{number}: {line.strip()}")
        self.assertEqual(offenders, [], "read `room` with GetBuiltin, not variable_global_get")

    def test_keychoice_reads_the_room_builtin_by_name(self):
        body = function_body(DROP_MANAGER.read_text(encoding="utf-8"), "static RValue& Hook_DropKeys(")
        self.assertIn('GetBuiltin("room", nullptr, NULL_INDEX,', body)
        self.assertIn('"room_get_name"', body)
        self.assertNotIn("rm.ToDouble()", body)

    def test_room_change_guards_use_the_builtin_key(self):
        source = MODULE_MAIN.read_text(encoding="utf-8")
        for signature in ("static void CiSnapTake()", "static void CiSnapDiff()", "static void ChaosTowerTick()"):
            with self.subTest(signature=signature):
                self.assertIn("CurrentRoomKey()", function_body(source, signature))
        name_body = function_body(source, "static std::string CurrentRoomName()")
        self.assertIn('GetBuiltin("room", nullptr, NULL_INDEX, v)', name_body)


if __name__ == "__main__":
    unittest.main()
