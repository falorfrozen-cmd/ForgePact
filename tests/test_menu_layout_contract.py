"""Contract tests for `menulayout`, the read-only menu listing.

`menulayout` answers where the live main-menu and character-select buttons
are - object name, on-screen text, and position in window (client)
coordinates computed from the game's own GUI and window sizes - so the hub's
`hs_select_character` clicks what the game reports instead of a fixed screen
fraction (docs/menu-layout-research.md). It ships in the player build, so
these tests pin that it stays a reader: a player command dispatched from its
own helper, nothing that hooks, performs, calls a script, creates, destroys
or writes, nothing on the per-frame path, every object an SDK name, and an
output format the hub parses byte for byte.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
REPO_ROOT = FORGEPACT_DIR.parent
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
SDK_OBJECTS_HPP = REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"
NOTES = FORGEPACT_DIR / "release-notes-v1.4.5.md"
DOC = FORGEPACT_DIR / "docs" / "menu-layout-research.md"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))
if str(SDK_PY_PATH) not in sys.path:
    sys.path.insert(0, str(SDK_PY_PATH))

from test_release_hook_contract import function_body, strip_research_blocks  # noqa: E402

# The thirteen names of the character-select static search
# (docs/menu-layout-research.md, § Static search): the three roots, five
# leaves, five unparented objects.
CHARACTER_SELECT_OBJECTS = {
    "UI_Node_Parent_obj", "UI_Parent_obj", "UI_List_Item_Parent_obj",
    "UI_Button_obj", "UI_Button_Small_obj", "UI_Character_obj",
    "UI_Create_Character_obj", "UI_Main_Menu_obj",
    "Save_Character_obj", "Save_Slot_Shop_obj", "Load_Inventory_Char_Select_obj",
    "Menu_Controller_obj", "Profile_Manager_obj",
}

# The sixteen names the stash and bag static search added
# (docs/stash-bag-layout-research.md, § Static search).
STASH_BAG_OBJECTS = {
    "New_Inventory_Data_obj", "UI_Inventory_Parent_obj", "UI_Stash_obj",
    "UI_Inventory_obj", "UI_Stash_Tab_Bar_Container_obj", "UI_Button_Stash_Tab_obj",
    "UI_Button_Inventory_Tab_obj", "UI_Button_Inventory_Tab_Small_obj",
    "UI_Button_Close_obj", "UI_Inventory_Grid_obj", "UI_Split_Stack_obj",
    "UI_Stash_Dropdown_obj", "UI_Stash_Socket_New_obj", "UI_Inventory_Drag_obj",
    "Town_Stash_obj", "Player_obj",
}

PLANNED_OBJECTS = CHARACTER_SELECT_OBJECTS | STASH_BAG_OBJECTS

# Anything that would make the listing do rather than read: hook, perform an
# event, create, destroy, write, or run a game script (by name through
# script_execute, or with a supplied self through CallBuiltinEx).
FORBIDDEN = (
    "MmCreateHook", "HookOneScript", "HookBuiltin", "CallGameScriptEx",
    "event_perform", "Rva", "GetModuleHandle", "instance_create",
    "instance_destroy", "variable_instance_set", "script_execute",
    "CallBuiltinEx",
)


def helper_signatures(source):
    """Every function this feature defines - MenuLayout* plus the dispatcher -
    mapped to its definition's signature, so function_body (which takes the
    LAST occurrence) finds the definition rather than a call site."""
    found = {}
    for match in re.finditer(r"^(static [\w:<>&* ]+?\b((?:Handle)?MenuLayout\w+)\()", source, re.M):
        found[match.group(2)] = match.group(1)
    return found


class MenuLayoutContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.player = strip_research_blocks(cls.plugin)
        cls.run_command = function_body(cls.plugin, "static void RunCommand(")
        signatures = helper_signatures(cls.plugin)
        cls.helpers = set(signatures)
        cls.bodies = {name: function_body(cls.plugin, sig) for name, sig in signatures.items()}
        cls.command = function_body(cls.plugin, "static void MenuLayoutCommand(")
        cls.row = function_body(cls.plugin, "static std::string MenuLayoutRow(")

    # --- a player command, dispatched from its own helper -------------------

    def test_literal_is_a_player_command(self):
        start = self.run_command.index("kPlayerCommands = {")
        initializer = self.run_command[start:self.run_command.index("};", start)]
        self.assertIn('"menulayout"', initializer)

    def test_literal_appears_in_the_dispatcher_and_the_allowlist(self):
        self.assertGreaterEqual(self.plugin.count('"menulayout"'), 2)

    def test_dispatched_from_a_helper_not_the_else_if_chain(self):
        self.assertIn("HandleMenuLayoutCommand", self.helpers)
        self.assertIn("if (HandleMenuLayoutCommand(lc, rest)) return;", self.run_command)
        # RunCommand names the literal only in its allowlist, never as a branch.
        self.assertEqual(self.run_command.count('"menulayout"'), 1)
        self.assertNotIn('lc == "menulayout"', self.run_command)
        dispatcher = self.bodies["HandleMenuLayoutCommand"]
        self.assertIn('lc == "menulayout"', dispatcher)
        self.assertIn("MenuLayoutCommand(rest)", dispatcher)

    def test_the_player_build_compiles_it(self):
        # Nothing of it sits inside a research-only block.
        for name in self.helpers:
            self.assertIn(name + "(", self.player, name)
        self.assertIn("if (HandleMenuLayoutCommand(lc, rest)) return;", self.player)
        self.assertIn('"menulayout"', function_body(self.player, "static void RunCommand("))

    # --- a reader -------------------------------------------------------------

    def test_nothing_it_runs_hooks_performs_or_writes(self):
        self.assertGreaterEqual(len(self.helpers), 5)
        for name, body in self.bodies.items():
            for word in FORBIDDEN:
                self.assertNotIn(word, body, f"{name} mentions {word}")

    def test_nothing_of_it_is_on_the_frame_path(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertNotIn("menulayout", frame.lower())
        for name in self.helpers:
            self.assertNotIn(name, frame, name)

    def test_instances_are_reached_by_name(self):
        for builtin in ("asset_get_index", "instance_number", "instance_find"):
            self.assertIn(f'"{builtin}"', self.command + self.bodies["MenuLayoutObjectIndex"], builtin)
        # An asset of another kind sharing the name is not an object.
        self.assertIn('"object_get_name"', self.bodies["MenuLayoutObjectIndex"])
        # Rows are deduplicated by instance id, so listing a parent and its
        # leaf never prints one button twice.
        self.assertIn("seen.insert(", self.command)

    def test_no_kind_check_decides_whether_a_row_is_listed(self):
        # This runner hands out instance handles as VALUE_REF; the listing
        # reads through whatever instance_find returned.
        self.assertNotIn("m_Kind", self.command)
        self.assertNotIn("m_Kind", self.row)

    # --- the objects ------------------------------------------------------------

    def candidate_table(self):
        start = self.plugin.index("kMenuLayoutObjects[] = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        return re.findall(r"HeroSiege::Objects::GameObject::(\w+)", block)

    def test_candidate_table_is_the_static_search_set(self):
        table = self.candidate_table()
        self.assertEqual(len(table), len(set(table)))
        self.assertEqual(set(table), PLANNED_OBJECTS)

    def test_every_candidate_is_an_sdk_game_object(self):
        from hs_game_sdk import GameObject
        header = SDK_OBJECTS_HPP.read_text(encoding="utf-8")
        for name in self.candidate_table():
            self.assertTrue(hasattr(GameObject, name), name)
            self.assertRegex(header, r"\n\s+" + re.escape(name) + r" = \d+,", name)

    def test_names_come_from_the_sdk_not_literals(self):
        for name in PLANNED_OBJECTS:
            self.assertNotIn(f'"{name}"', self.command, name)
        self.assertIn("HeroSiege::Objects::GetObjectName(obj)", self.command)

    # --- the output format (the hub's parse contract) --------------------------

    def test_header_format(self):
        pieces = ['Out("menulayout: room="', '" gui="', '"x"', '" window="',
                  '" fullscreen="', '" view="']
        at = [self.command.index(p) for p in pieces]
        self.assertEqual(at, sorted(at))

    def test_footer_format(self):
        pieces = ['Out("menulayout: listed="', '" absent="', 'std::string("none")', '" capped="']
        at = [self.command.index(p) for p in pieces]
        self.assertEqual(at, sorted(at))

    def test_row_format(self):
        pieces = ['"  obj="', '" id="', '" gui="', '" win="', '" bbox="',
                  '" visible="', '" sprite="',
                  '{ "label", "name", "slot", "index", "page", "selected",',
                  'row += " text="']
        at = [self.row.index(p) for p in pieces]
        self.assertEqual(at, sorted(at))
        # text= is present on every row, empty when the instance has none,
        # and nothing is appended after it.
        self.assertIn('row += " text=" + (hasText ? text : std::string());\n    return row;', self.row)
        self.assertIn('"none"', self.row)   # sprite_index -1

    def test_client_coordinates_read_the_sizes_by_name(self):
        for builtin in ("window_get_width", "window_get_height",
                        "display_get_gui_width", "display_get_gui_height",
                        "window_get_fullscreen", "view_get_camera"):
            self.assertIn(f'"{builtin}"', self.command, builtin)
        self.assertIn("x * sc.ww / sc.gw", self.row)
        self.assertIn("y * sc.wh / sc.gh", self.row)

    def test_a_failed_read_says_so_and_no_bare_percent_f(self):
        self.assertIn('"<read-failed>"', self.plugin)
        for name, body in self.bodies.items():
            self.assertNotIn("%f", body, name)
            self.assertNotIn("sprintf", body, name)
        self.assertIn("std::isfinite", self.bodies["MenuLayoutDecimal"])
        self.assertIn("std::isfinite", self.bodies["MenuLayoutInteger"])

    def test_listing_is_capped(self):
        self.assertIn("static constexpr int kMenuLayoutMaxRows = 200;", self.plugin)
        self.assertIn("listed >= kMenuLayoutMaxRows", self.command)

    def test_unresolved_names_are_reported_not_skipped(self):
        self.assertIn("absent +=", self.command)

    # --- documentation ------------------------------------------------------------

    def test_release_notes_name_the_command(self):
        if not NOTES.is_file():   # published notes leave main (forgepact-notes-cleanup.yml)
            self.skipTest(f"{NOTES.name} is published and no longer on main")
        self.assertIn("menulayout", NOTES.read_text(encoding="utf-8"))


DOC_HEADINGS = ("## Static search", "## Instrument", "## Live procedure",
                "## Results", "## Decision")
DECISION_KEYS = ("slotObject", "slotRule", "playObject", "playRule")


def doc_section(doc, heading):
    """From a line that is exactly `heading` to the next `## ` heading."""
    start = doc.index("\n" + heading + "\n") + 1
    following = doc.find("\n## ", start + len(heading))
    return doc[start:] if following < 0 else doc[start:following]


class MenuLayoutResearchDoc(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_headings_in_order(self):
        at = [self.doc.index("\n" + h + "\n") for h in DOC_HEADINGS]
        self.assertEqual(at, sorted(at))

    def test_decision_lines(self):
        decision = doc_section(self.doc, "## Decision")
        for key in DECISION_KEYS:
            lines = re.findall(r"(?m)^" + key + r": (.+)$", decision)
            self.assertEqual(len(lines), 1, key)
            # Phase 0 ran on 2026-09-21 (F-4): every line carries a measured
            # value, and none is a recorded conflict awaiting the owner.
            self.assertNotIn("pending", lines[0].lower(), key)
            self.assertNotIn("CONFLICT", lines[0], key)

    def test_positive_control_is_written_down(self):
        instrument = doc_section(self.doc, "## Instrument")
        self.assertIn("win=336,534", instrument)
        self.assertIn("window=1920x1080", instrument)

    def test_the_procedure_never_clicks_play(self):
        self.assertIn("Do not click `PLAY`", doc_section(self.doc, "## Live procedure"))

    def test_candidate_table_is_documented(self):
        static = doc_section(self.doc, "## Static search")
        for name in CHARACTER_SELECT_OBJECTS:
            self.assertIn(f"`{name}`", static, name)


if __name__ == "__main__":
    unittest.main()
