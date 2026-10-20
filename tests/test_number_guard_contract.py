#!/usr/bin/env python3
"""Contract tests for the number guard (`numguard`, ForgePact #230).

plugin/include/ForgePact/NumberGuard.hpp detours num_compressSteam, the game's
Steam number shortener, and caps a number it cannot show. The behaviour is
driven by tests/test_number_guard_behavior.py; these tests pin, on
comment-stripped source, what a behaviour test cannot see:

- the four constants, and that the header stays portable C++ (no <windows.h>);
- one native detour on num_compressSteam in the whole plugin, named through
  the SDK constant, tried at most once a session;
- the detour hands a substitute argument array and never writes the caller's;
- the trace line is written before the original runs;
- Arm() is called only by the three Modifiers handlers, after their own hook
  is known native and their value is off vanilla (for `stat`, only on the
  damage and crit entries), and never at load;
- the research verbs, the trace and its CalculateEndDamage callout stay out of
  the player build, while the status line and the first-cap line ship;
- `numguard` is a player command dispatched from RunCommand, and NAddrAll
  lists num_compressSteam in both builds.

Each scanner has a negative control where the check could pass vacuously.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADERS = ROOT / "plugin" / "include" / "ForgePact"
GUARD = HEADERS / "NumberGuard.hpp"
STATS = HEADERS / "StatsManager.hpp"
SLIDERS = HEADERS / "SkillSlidersMod.hpp"

HOOK_CALL = re.compile(r"\bHookOneScript(?:Table)?\s*\(")
ARM_CALL = re.compile(r"\bNumberGuard::Arm\s*\(\s*\)|(?<![\w:.>])Arm\s*\(\s*\)\s*;")
CALLER_WRITE = re.compile(r"\*\s*A\s*\[\s*0\s*\]\s*=(?!=)|\bA\s*\[\s*0\s*\]\s*(?:->\s*\w+\s*)?=(?!=)")


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def calls(source, pattern=HOOK_CALL):
    """Every call `pattern` starts, with its balanced argument list."""
    found = []
    for match in pattern.finditer(source):
        depth = 0
        for index in range(match.end() - 1, len(source)):
            if source[index] == "(":
                depth += 1
            elif source[index] == ")":
                depth -= 1
                if depth == 0:
                    found.append(source[match.start():index + 1])
                    break
    return found


class NumberGuardContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.guard_raw = read(GUARD)
        cls.guard = strip_comments(cls.guard_raw)
        cls.guard_player = strip_comments(strip_research_blocks(cls.guard_raw))
        cls.stats_raw = read(STATS)
        cls.stats = strip_comments(cls.stats_raw)
        cls.stats_player = strip_comments(strip_research_blocks(cls.stats_raw))
        cls.sliders_raw = read(SLIDERS)
        cls.sliders = strip_comments(cls.sliders_raw)
        cls.plugin_raw = read(PLUGIN)
        cls.plugin = strip_comments(cls.plugin_raw)
        cls.plugin_player = strip_comments(strip_research_blocks(cls.plugin_raw))
        cls.sources = {"ModuleMain.cpp": cls.plugin}
        for header in sorted(HEADERS.glob("*.hpp")):
            cls.sources[header.name] = strip_comments(read(header))

    # ---- the header ----------------------------------------------------------

    def test_the_four_constants(self):
        expected = {
            "kNumGuardPositiveLimit": 1e14,
            "kNumGuardNegativeLimit": -1e6,
            "kNumGuardLargestSafe": 99999000000000.0,
            "kNumGuardSmallestSafe": -999999.0,
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                match = re.search(r"inline constexpr double " + name + r"\s*=\s*([-0-9.eE+]+)\s*;", self.guard)
                self.assertIsNotNone(match, name)
                self.assertEqual(float(match.group(1)), value)
        # The safe values are inside the limits, the limits are what fails.
        self.assertLess(expected["kNumGuardLargestSafe"], expected["kNumGuardPositiveLimit"])
        self.assertGreater(expected["kNumGuardSmallestSafe"], expected["kNumGuardNegativeLimit"])
        fails = function_body(self.guard, "inline bool Fails(double v)")
        self.assertIn("v >= kNumGuardPositiveLimit", fails)
        self.assertIn("v <= kNumGuardNegativeLimit", fails)
        self.assertIn("std::isnan(v)", fails)

    def test_the_header_stays_portable(self):
        # tests/number_guard_harness.cpp compiles it with fake game types, and
        # StatsManager's and SkillSlidersMod's harnesses stand it in.
        includes = re.findall(r'#include\s*[<"]([^>"]+)[>"]', self.guard_raw)
        for banned in ("windows.h", "reward_scope.hpp", "Common.hpp"):
            self.assertFalse(any(banned in name for name in includes), (banned, includes))
        self.assertIn('#include "NumberGuard.hpp"', self.stats_raw)
        self.assertIn('#include "NumberGuard.hpp"', self.sliders_raw)

    # ---- one native detour, at most once -------------------------------------

    def test_one_hook_site_for_num_compress_steam(self):
        # The scanner's own control: it finds a call, and ignores another script.
        self.assertEqual(len([c for c in calls('HookOneScript("num_compressSteam", "x", (void*)f, &o);')
                              if "num_compressSteam" in c]), 1)
        self.assertEqual([c for c in calls('HookOneScript("num_compressGML", "x", (void*)f, &o);')
                          if "num_compressSteam" in c], [])
        sites = {}
        for name, text in self.sources.items():
            found = [c for c in calls(text) if "num_compressSteam" in c]
            if found:
                sites[name] = found
        self.assertEqual(list(sites), ["NumberGuard.hpp"], sites)
        self.assertEqual(len(sites["NumberGuard.hpp"]), 1, sites)
        site = sites["NumberGuard.hpp"][0]
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_num_compressSteam)", site)
        self.assertIn('"fp_numguard"', site)
        self.assertIn("&native", site)

    def test_install_keeps_a_route_retries_a_failure_and_only_native_arms(self):
        # A native or table-only result is kept for the session (HookOneScript
        # saves the original on its first install only); a failed one saved
        # nothing and is tried again at the next arming. The first guard's
        # sticky `installTried` kept a failed first resolve for the session.
        install = function_body(self.guard, "inline bool Install()")
        self.assertIn("if (s.route == kRouteNone || s.route == kRouteFailed)", install)
        self.assertNotIn("installTried", self.guard)
        self.assertIn("return s.route == kRouteNative;", install)
        # One line per route result, not per attempt.
        self.assertIn("const bool changed = route != s.route;", install)
        self.assertIn("numbers past the display limit are not capped", install)
        # Table-only says what is and is not capped.
        self.assertIn("only calls through the script table are capped", install)
        self.assertIn("the game's calls through its method value are not", install)
        status = function_body(self.guard, "inline std::string StatusLine()")
        self.assertIn('(s.route == kRouteNative ? "armed" : "not armed")', status)

    # ---- the detour ----------------------------------------------------------

    def test_the_detour_never_writes_the_callers_argument(self):
        # Negative control: the pattern catches each way of writing it.
        for bad in ("*A[0] = x;", "A[0] = &x;", "A[0]->m_Real = 1;"):
            self.assertRegex(bad, CALLER_WRITE)
        self.assertNotRegex("if (A[0] == nullptr) {}", CALLER_WRITE)
        hook = function_body(self.guard, "inline RValue& Hook(")
        self.assertNotRegex(hook, CALLER_WRITE)
        self.assertIn("A2.assign(A, A + argc);", hook)
        self.assertIn("A2[0] = &substitute;", hook)
        self.assertIn("RValue& result = s.original(S, O, R, argc, use);", hook)
        self.assertIn("return result;", hook)

    def test_the_trace_line_comes_before_the_original(self):
        hook = function_body(self.guard, "inline RValue& Hook(")
        self.assertLess(hook.index("TraceValue(kSiteCompress, handed);"), hook.index("s.original(S, O, R, argc, use)"))
        # And the cap is judged by value, only for the kinds a number arrives as.
        number = function_body(self.guard, "inline bool IsNumber(const RValue* v)")
        for kind in ("VALUE_REAL", "VALUE_INT32", "VALUE_INT64"):
            self.assertIn(kind, number)

    def test_the_first_cap_line_ships(self):
        self.assertIn('"numguard: capped "', self.guard_player)
        self.assertIn("++s.clamped;", function_body(self.guard_player, "inline RValue& Hook("))

    # ---- arming --------------------------------------------------------------

    def test_arm_is_called_only_by_the_three_modifiers_handlers(self):
        # Negative control: the pattern sees both spellings.
        self.assertEqual(len(ARM_CALL.findall("ForgePact::NumberGuard::Arm(); Arm();")), 2)
        self.assertEqual(ARM_CALL.findall("CombatText::Install(); s.Arm2();"), [])
        # Other mods have an Arm() of their own; outside NumberGuard.hpp only
        # the qualified call is this guard's.
        found = {name: (ARM_CALL.findall(text) if name == "NumberGuard.hpp" else re.findall(r"\bNumberGuard::Arm\s*\(", text))
                 for name, text in self.sources.items()}
        found = {name: hits for name, hits in found.items() if hits}
        self.assertEqual(sorted(found), ["NumberGuard.hpp", "SkillSlidersMod.hpp", "StatsManager.hpp"], found)
        # Never at load, from a default config or from the dispatcher.
        self.assertNotIn("ModuleMain.cpp", found)
        # Inside NumberGuard.hpp only the research build's `trace on` arms it.
        self.assertEqual(ARM_CALL.findall(self.guard_player), [])
        self.assertEqual(len(ARM_CALL.findall(self.guard)), 1)

    def test_stat_arms_after_a_damage_multiplier_off_one_is_set(self):
        body = function_body(self.stats, "void HandleStatCommand(const std::string& rest)")
        self.assertEqual(body.count("ForgePact::NumberGuard::Arm();"), 1)
        arm = body.index("if (c != 1.0 && ArmsNumberGuard(*hedef)) ForgePact::NumberGuard::Arm();")
        self.assertLess(body.index("*hedef->mult = c;"), arm)
        # The x1-with-no-hook early return and the failed-install return come first.
        self.assertLess(body.index("c == 1.0 && !*hedef->orig"), arm)
        self.assertLess(body.index("kancasi kurulamadi"), arm)

    def test_only_the_damage_and_crit_stat_entries_arm(self):
        # Owner, 2026-10-20: "Irrelevant mods shouldn't arm it". The named
        # predicate decides from the entry's alias, against one list.
        predicate = function_body(self.stats, "static bool ArmsNumberGuard(const Entry& e)")
        self.assertIn("kGuardArmAliases", predicate)
        listed = re.search(r"kGuardArmAliases\[kGuardArmCount\]\s*=\s*\{(?P<body>.*?)\};", self.stats, re.DOTALL)
        self.assertIsNotNone(listed)
        aliases = re.findall(r'"(\w+)"', listed.group("body"))
        self.assertEqual(aliases, ["damage", "critdamage", "critchance", "spellcritdamage", "spellcritchance"])
        # Negative control: each listed alias is a real `stat` entry, and the
        # entries left out (Experience, gold, speed and the rest) are too.
        table = function_body(self.stats, "const Entry* Table()")
        entries = re.findall(r'"(\w+)" \},', table)
        self.assertEqual(len(entries), 14, entries)
        self.assertTrue(set(aliases) < set(entries), entries)
        for left_out in ("exp", "expgain", "magicfind", "extragold", "movespeed", "attackspeed"):
            with self.subTest(alias=left_out):
                self.assertIn(left_out, entries)
                self.assertNotIn(left_out, aliases)

    def test_statadd_arms_only_after_the_native_check(self):
        body = function_body(self.stats, "void HandleStatAddCommand(const std::string& rest)")
        self.assertEqual(body.count("ForgePact::NumberGuard::Arm();"), 1)
        arm = body.index("ForgePact::NumberGuard::Arm();")
        self.assertLess(body.index("if (ek == 0.0)"), arm)
        self.assertLess(body.index("if (*e->route != kRouteNative)"), arm)
        self.assertLess(body.index("*e->bonus = ek;"), arm)

    def test_skillslider_arms_only_after_the_native_check(self):
        body = function_body(self.sliders, "void HandleCommand(const std::string& rest)")
        self.assertEqual(body.count("ForgePact::NumberGuard::Arm();"), 1)
        arm = body.index("ForgePact::NumberGuard::Arm();")
        self.assertLess(body.index("if (value == 0.0)"), arm)
        self.assertLess(body.index("const int blocked = FirstNotNative(lever);"), arm)
        self.assertLess(body.index("state.value = value;"), arm)

    # ---- the player build ------------------------------------------------------

    def test_research_surfaces_stay_out_of_the_player_build(self):
        for word in ("forcedOff", "traceOn", "TraceValue", "TraceResult", "installEndDamageTrace", "largest"):
            with self.subTest(word=word):
                self.assertNotIn(word, self.guard_player)
                self.assertIn(word, self.guard)
        self.assertIn("is available in the research build only", self.guard_player)
        # Both builds count calls, so a player's report tells "the detour
        # never ran" from "nothing needed capping": one `calls=`, after `first=`.
        self.assertIn("++s.calls;", function_body(self.guard_player, "inline RValue& Hook("))
        status = function_body(self.guard_player, "inline std::string StatusLine()")
        self.assertEqual(status.count('" calls="'), 1, status)
        self.assertLess(status.index('" first="'), status.index('" calls="'))
        self.assertEqual(function_body(self.guard, "inline std::string StatusLine()").count("calls="), 1)
        # StatsManager's trace callout and install are research-only too.
        for word in ("TraceResult", "InstallEndDamageTrace"):
            with self.subTest(word=word):
                self.assertNotIn(word, self.stats_player)
                self.assertIn(word, self.stats)
        self.assertIn("#define FP_STAT_TRACE(NAME, SITE, VALUE) ((void)0)", self.stats_player)
        self.assertNotIn("installEndDamageTrace", self.plugin_player)
        self.assertNotIn("m_EndDamageRoute", self.stats_player)

    def test_the_trace_reply_names_end_damage_route(self):
        # The hand-in returns the route HookOneScript took, and the reply
        # names it rather than "hooked", which a table-only hook also was.
        self.assertIn("inline int (*installEndDamageTrace)() = nullptr;", self.guard)
        handle = function_body(self.guard, "inline void HandleCommand(const std::string& rest)")
        self.assertIn('", CalculateEndDamage " + RouteName(damage)', handle)
        self.assertNotIn('"hooked"', handle)
        install = function_body(self.stats, "int InstallEndDamageTrace()")
        self.assertIn("&native", install)
        self.assertIn("m_EndDamageRoute", install)
        # `stat damage` installed first: its route is kept too.
        stat = function_body(self.stats, "void HandleStatCommand(const std::string& rest)")
        self.assertIn("HookOneScript(hedef->name, hedef->hookId, hedef->hook, hedef->orig, &native);", stat)
        self.assertIn("m_EndDamageRoute = native ?", stat)

    # ---- ModuleMain --------------------------------------------------------------

    def test_numguard_is_a_player_command_dispatched_from_run_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertIn('"numguard"', allowlist.group("body"))
        run = function_body(self.plugin_player, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "numguard") { NumGuardCmd(rest); return; }', run)
        cmd = function_body(self.plugin_player, "static void NumGuardCmd(const std::string& rest)")
        self.assertIn("ForgePact::NumberGuard::HandleCommand(rest);", cmd)

    def test_naddrall_lists_num_compress_steam_in_both_builds(self):
        names = function_body(self.plugin_player, "static void NAddrAll()")
        self.assertIn('"num_compressSteam"', names)


if __name__ == "__main__":
    unittest.main()
