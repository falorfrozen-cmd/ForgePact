"""Auto-sell filtered loot (`autosell`, ForgePact #196): the plugin wiring.

tests/test_auto_sell_behavior.py runs the real AutoSellMod class against a
controlled game. This file pins what that harness cannot see, on the plugin's
comment-stripped source:

- `AutoSellOnInit` is one more consumer of the shared LootGroundInit detour,
  after the tracker; it returns at once while the switch is off and only
  reduces argument 0 and `self` to durable handles (`LaDurable`), with no
  runner call of its own;
- `AutoSellTick` runs in the frame callback after setup, after `TrackerTick`
  and before `HiddenLootTick`, returns at once while off, decides through the
  class on the room key, and then hands hidden loot this frame's sold ids;
- the hook goes in through the shared single attempt (`HiddenLootInstall`),
  never a second `HookOneScript` on LootGroundInit;
- the four game scripts (GetCounterHash, PickUpGoldCheck, GoldLogAdd,
  GetGoldAmount) are called by name through their SDK constants, with the
  argument shapes the static reading gives, self the local player accepted
  through the instance-handle rule;
- `autosell` is a player command; `quote` is research build only;
- none of the sale's parts the owner ruled out (the sell pool, the ownership
  change, the online record), no address, no ground scan, no verdict write;
- the ON, OFF and stat lines and the modstate `autoSell` object, and
  hidden loot's `sold=`.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "AutoSellMod.hpp"

ADAPTER_START = "// ---- Auto-sell filtered loot (AutoSellMod.hpp): the adapter"
ADAPTER_END = "// ---- end of the auto-sell adapter"

HOOK = "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"

#: The stat line's fields, in order, as Live procedure 1 reads them.
STAT_FIELDS = ["on", "route", "notes", "sold", "gold", "exact", "inexact", "visible", "no-filter-var", "unsellable",
               "skipped-persistent", "repeat-held", "no-player", "credit-refused", "destroy-failed", "gone",
               "unidentified", "errors", "last", "gold-now"]

#: What the sale does that autosell must never mirror, and what no player
#: binary may carry (an address, a module base).
NEVER = ("ChangeItemOwner", "InventoryUpdateExt", "sellPoolFingerprintList", "ds_list_add", "GetModuleHandle",
         "MmCreateHook", "reinterpret_cast", "variable_instance_set", "instance_find", "instance_number",
         "with_instances", "instance_deactivate")


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


class AutoSellAdapterContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = read(PLUGIN)
        cls.plugin = plugin
        cls.code = strip_comments(plugin)
        cls.player = strip_comments(strip_research_blocks(plugin))
        start = plugin.find(ADAPTER_START)
        end = plugin.find(ADAPTER_END)
        if start < 0 or end < start:
            raise AssertionError("the auto-sell adapter's start and end markers are missing from ModuleMain.cpp")
        cls.adapter = strip_comments(plugin[start:end])
        cls.player_adapter = strip_comments(strip_research_blocks(plugin[start:end]))
        cls.header = strip_comments(read(HEADER))

    def body(self, signature, source=None):
        return function_body(source if source is not None else self.code, signature)

    def lines(self, signature):
        return [l.strip() for l in self.body(signature).splitlines() if l.strip()]

    def assertBefore(self, first, second, code, where):
        for text in (first, second):
            self.assertTrue(text in code, f"{where} lacks {text!r}")
        self.assertLess(code.index(first), code.index(second), f"{where}: {first!r} must come before {second!r}")

    # ---- the trigger: the shared detour --------------------------------------

    def test_autosell_is_the_shared_detours_last_consumer(self):
        hook = self.body(HOOK)
        self.assertEqual(1, hook.count("AutoSellOnInit(S, argc, A);"))
        self.assertBefore("g_Orig_LootGroundInit(S, O, R, argc, A)", "AutoSellOnInit(S, argc, A);", hook, "HookHiddenLootInit")
        self.assertBefore("TrackerOnInit(S, argc, A);", "AutoSellOnInit(S, argc, A);", hook, "HookHiddenLootInit")
        # The hook itself still makes no runner call.
        self.assertNotIn("CallBuiltin", hook)

    def test_on_init_returns_at_once_while_off_and_only_notes(self):
        on_init = self.lines("static void AutoSellOnInit(CInstance* S, int argc, RValue** A)")
        self.assertEqual("auto& as = ForgePact::AutoSellMod::Instance();", on_init[0])
        self.assertEqual("if (!as.Enabled()) return;", on_init[1])
        text = "\n".join(on_init)
        self.assertEqual(2, text.count("LaDurable("), text)
        self.assertIn("as.Note(", text)
        self.assertNotIn("CallBuiltin", text)
        self.assertNotIn("ApCallScript", text)
        self.assertNotIn("instance_destroy", text)

    def test_the_tick_sits_after_the_tracker_and_before_hidden_loot(self):
        frame = self.body("void FrameCallback(FWFrame& FrameContext)")
        self.assertEqual(1, frame.count("AutoSellTick();"))
        self.assertIn("if (g_Setup) AutoSellTick();", frame)
        self.assertBefore("if (g_Setup) LootAnnounceTick();", "if (g_Setup) AutoSellTick();", frame, "FrameCallback")
        self.assertBefore("if (g_Setup) TrackerTick();", "if (g_Setup) AutoSellTick();", frame, "FrameCallback")
        self.assertBefore("if (g_Setup) AutoSellTick();", "if (g_Setup) HiddenLootTick();", frame, "FrameCallback")

    def test_the_tick_returns_at_once_while_off(self):
        tick = self.lines("static void AutoSellTick()")
        self.assertEqual("auto& as = ForgePact::AutoSellMod::Instance();", tick[0])
        self.assertEqual("if (!as.Enabled()) return;", tick[1])

    def test_the_tick_decides_through_the_class_then_hands_hidden_loot_the_sold_ids(self):
        tick = self.body("static void AutoSellTick()")
        self.assertIn("as.OnFrame(CurrentRoomKey());", tick)
        self.assertBefore("as.OnFrame(CurrentRoomKey());", "as.SoldThisFrame()", tick, "AutoSellTick")
        self.assertBefore("as.SoldThisFrame()", ".NoteSold(", tick, "AutoSellTick")
        self.assertIn("ForgePact::HiddenLootMod::Instance()", tick)
        # The decision is the class's: the tick itself reads nothing off an item.
        self.assertNotIn("CallBuiltin", tick)
        self.assertNotIn("instance_destroy", tick)

    def test_the_install_is_the_shared_single_attempt(self):
        tick = self.body("static void AutoSellTick()")
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", tick)
        command = self.body("static void AutoSellCommand(const std::string& rest)")
        self.assertIn("if (g_Setup && !g_HiddenLootInstallTried) HiddenLootInstall();", command)
        self.assertNotIn("HookOneScript", self.adapter)
        self.assertEqual(1, len(re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", self.code)))
        # The route is the shared detour's, said out loud when not both.
        route = self.body("static void AutoSellSyncRoute()")
        self.assertIn("ForgePact::HiddenLootMod::Instance().GetRoute()", route)
        self.assertIn("ForgePact::AutoSellMod::Route::NotInstalled", route)
        say = self.body("static void AutoSellSayRoute()")
        self.assertIn("Out(", say)
        self.assertIn("may pass it by", say)

    # ---- the sale's named pieces ------------------------------------------------

    def test_the_four_scripts_are_called_by_name_through_their_sdk_constants(self):
        for script in ("GetCounterHash", "PickUpGoldCheck", "GoldLogAdd", "GetGoldAmount"):
            self.assertIn(f"SdkShortScriptName(HeroSiege::Scripts::gml_Script_{script})", self.adapter, script)
            self.assertNotIn(f'"{script}"', self.adapter, script)
        call = self.body("static bool AsCallScript(const char* name, const RValue& player, const std::vector<RValue>& args, RValue& res)")
        # Self and other: the local player, resolved to its instance by name.
        self.assertIn("HhResolveInstance(player)", call)
        self.assertIn("ApCallScript(name, self, args, res)", call)

    def test_the_credit_reads_the_hash_just_before_the_check_and_wants_true(self):
        credit = self.body("static bool AsCredit(const RValue& player, double price)")
        self.assertBefore("gml_Script_GetCounterHash", "gml_Script_PickUpGoldCheck", credit, "AsCredit")
        self.assertIn("{ hash, RValue(price), RValue(1.0), RValue(), RValue(), RValue() }", credit)
        self.assertIn("VALUE_BOOL", credit)
        self.assertIn(".ToBoolean()", credit)

    def test_the_gold_log_and_the_balance(self):
        log = self.body("static bool AsGoldLog(const RValue& player, double price)")
        self.assertIn("gml_Script_GoldLogAdd", log)
        self.assertIn("{ RValue(18.0), RValue(price) }", log)
        read_gold = self.body("static bool AsGoldRead(const RValue& player, double& gold)")
        self.assertIn("gml_Script_GetGoldAmount", read_gold)
        self.assertIn(", {}, ", read_gold)
        self.assertIn("std::isfinite(", read_gold)

    def test_the_local_player_goes_through_the_instance_handle_rule(self):
        player = self.body("static bool AsLocalPlayer(RValue& player)")
        self.assertIn("HhResolveLocalPlayer(", player)
        self.assertIn("HeroSiege::Player::IsInstanceHandle(", player)
        # No raw kind check decides whether the sale happens.
        self.assertNotIn("m_Kind", player)
        self.assertNotIn("VALUE_OBJECT", player)

    def test_the_item_key_and_the_room_are_the_shared_ones(self):
        key = self.body("static bool AsItemKey(const RValue& itemStruct, ForgePact::AutoSellMod::ItemKey& key)")
        self.assertIn("LaItemKey(itemStruct,", key)
        room = self.body("static ForgePact::AutoSellMod::Room AsRoomInfo()")
        self.assertIn("FarSleepRoomInfo()", room)
        wire = self.body("static void AutoSellWire()")
        for setter in ("SetLocalPlayer(&AsLocalPlayer)", "SetCredit(&AsCredit)", "SetGoldLog(&AsGoldLog)",
                       "SetGoldRead(&AsGoldRead)", "SetItemKey(&AsItemKey)", "SetRoomInfo(&AsRoomInfo)"):
            self.assertIn(setter, wire)

    def test_never_the_parts_the_owner_ruled_out_nor_an_address(self):
        for text, where in ((self.adapter, "adapter"), (self.header, "AutoSellMod.hpp")):
            for name in NEVER:
                self.assertNotIn(name, text, f"{where}: {name}")
            self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", text), where)
            self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+)", text), where)
        # The class reaches the runner through CallBuiltin alone.
        self.assertEqual({"CallBuiltin"}, set(re.findall(r"g_Yytk->(\w+)", self.header)))

    # ---- the command ---------------------------------------------------------------

    def test_the_command_is_a_player_command(self):
        start = self.player.index("kPlayerCommands = {")
        self.assertIn('"autosell"', self.player[start:self.player.index("};", start)])

    def test_the_verb_is_a_standalone_early_return_in_both_builds(self):
        run = function_body(self.player, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "autosell") { AutoSellCommand(rest); return; }', run)
        self.assertNotIn('else if (lc == "autosell")', run)

    def test_quote_is_research_build_only(self):
        self.assertIn("static void AutoSellQuote(const std::string& key)", self.adapter)
        self.assertNotIn("AutoSellQuote(", self.player_adapter)
        self.assertNotIn("QuoteFields(", self.player_adapter)
        quote = self.body("static void AutoSellQuote(const std::string& key)")
        # GiveItemCommand's map-0 lookup, read only.
        self.assertIn("CmSaveInstance()", quote)
        self.assertIn("CmItemMap(save, kCmCharacterOwner, map0)", quote)
        self.assertIn("CmMapItem(map0, RValue(key), item)", quote)
        self.assertIn(".QuoteFields(item, fields, why)", quote)
        self.assertIn('"autosell quote: "', quote)
        self.assertIn('"refused - "', quote)
        for write in ("CmCall(", "variable_struct_set", "ds_map_delete", "ds_map_add", "instance_destroy"):
            self.assertNotIn(write, quote, write)
        # The player build answers a quote with a refusal, not silence.
        command = function_body(strip_comments(strip_research_blocks(self.plugin)),
                                "static void AutoSellCommand(const std::string& rest)")
        self.assertIn('"autosell: quote refused - research build only; nothing was read"', command)

    def test_the_lines_the_live_procedure_reads(self):
        command = self.body("static void AutoSellCommand(const std::string& rest)")
        self.assertIn('Out(std::string("autosell -> ON route=") + as.RouteName());', command)
        self.assertIn('Out("autosell -> OFF " + as.StatFields());', command)
        self.assertIn('Out("autosell stat: " + as.StatFields());', command)
        self.assertBefore("as.Disable();", 'Out("autosell -> OFF "', command, "AutoSellCommand")
        self.assertIn('"autosell: usage autosell 1 | 0 | stat"', command)
        # The class prints the fields, in the documented order.
        stat = self.body("std::string StatFields()", self.header)
        fields = re.findall(r'"\s?([a-z0-9-]+)="', stat)
        self.assertEqual(STAT_FIELDS, fields)

    def test_the_mod_state_has_an_auto_sell_entry(self):
        flush = self.plugin[self.plugin.index("static void FlushModState(uint32_t frame)"):]
        flush = flush[:flush.index("\n}\n")]
        start = flush.index('\\"autoSell\\":{\\"enabled\\":')
        block = flush[start:flush.index("}", flush.index('\\"errors\\":', start) + 1) + 1]
        self.assertEqual(["autoSell", "enabled", "route", "sold", "errors"], re.findall(r'\\"(\w+)\\":', block))

    def test_hidden_loot_counts_a_sold_drop(self):
        stat = self.body("static std::string HiddenLootStatFields()")
        self.assertIn('" sold=" + std::to_string(st.sold)', stat)


if __name__ == "__main__":
    unittest.main()
