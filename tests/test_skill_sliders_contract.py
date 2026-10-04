"""Skill sliders (`skillslider`, issue #160): the plugin wiring and the rules
the header must keep.

tests/test_skill_sliders_behavior.py runs the real class against a controlled
runner. This file pins what that harness cannot see:

1. **The verb ships.** `skillslider` is in the player build's
   `kPlayerCommands`, and its dispatch survives the release preprocessor.
2. **It is its own early `if`.** `RunCommand`'s `else if` chain is at MSVC's
   nesting limit (C1061), so the dispatch sits at the top level of the
   function, never chained to another branch.
3. **Off installs nothing.** The header is the only place that hooks, and
   only on the arming path: after a value of 0 has returned, and before the
   lever stores a value. Nothing in ModuleMain touches the class but the
   dispatch, so nothing is installed at load.
4. **Names, never literals or addresses.** Every script comes from a
   `HeroSiege::Scripts` constant through `SdkShortScriptName`, and every
   object from `HeroSiege::Objects` through `GetObjectName`.
5. **The slider code is the same in both builds.** The header has no research
   block other than `BP_DIAG_INCREMENT` counters, so the dev DLL the live
   session runs exercises exactly what players get.
6. **Its hooks are its own.** The hook ids are not `projprobe`'s `fp_pp_*`
   ids, and no other hook uses them.
7. **`naddrall` lists what the player build hooks.** The five scripts sit in
   `NAddrAll`'s `kNames` outside its research guard, once each.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_release_hook_contract import (  # noqa: E402
    function_body,
    load_sdk_script_constants,
    strip_comments,
    strip_research_blocks,
)

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
INCLUDE = ROOT / "plugin" / "include" / "ForgePact"
HEADER = INCLUDE / "SkillSlidersMod.hpp"

INCLUDE_LINE = "#include <ForgePact/SkillSlidersMod.hpp>"
DISPATCH = 'if (lc == "skillslider") { ForgePact::SkillSlidersMod::Instance().HandleCommand(rest); return; }'
# The five scripts the player build hooks, in the header's table order.
SCRIPTS = (
    "ReturnExtraSpellProjectiles",
    "ReturnExtraProjectilesRanged",
    "StatAOESkillSize",
    "LoadAllModifiers",
    "ReturnSpecificStat",
)
OBJECTS = ("Player_obj", "Universal_Double_Cast_obj")


def _read(path):
    return path.read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def _no_strings(code):
    """Code with every string literal's contents removed, so a brace or a
    keyword inside a message cannot move a count."""
    return re.sub(r'"(?:\\.|[^"\\\n])*"', '""', code)


def research_lines(source):
    """The lines a player build drops, preprocessor lines aside: the complement
    of `strip_research_blocks`, evaluated the same way (`#ifndef` and the
    `#else` half of `#ifdef FORGEPACT_RELEASE` alike, nesting tracked)."""
    dropped, stack = [], []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif not all(active for _, active in stack):
            dropped.append(line)
    return dropped


def non_counter_research_lines(source):
    """Research-only lines that are anything but a blank or a counter."""
    return [l for l in research_lines(source)
            if l.strip() and not re.match(r"^\s*BP_DIAG_INCREMENT\([^;]*\);\s*$", l)]


class SkillSlidersPluginWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = _read(PLUGIN)
        cls.code = strip_comments(cls.plugin)
        cls.player_build = strip_comments(strip_research_blocks(cls.plugin))

    def test_the_player_build_accepts_the_verb(self):
        allowlist = re.search(r"kPlayerCommands = \{(?P<body>.*?)\};", self.player_build, re.S)
        self.assertIsNotNone(allowlist, "kPlayerCommands not found in the player build")
        self.assertEqual(re.findall(r'"skillslider"', allowlist.group("body")), ['"skillslider"'])
        # And the dispatch is compiled into the player build.
        self.assertIn(DISPATCH, function_body(self.player_build, "static void RunCommand("))
        # Negative control: the stripper does drop research-only dispatches.
        self.assertNotIn('"projprobe"', self.player_build)
        self.assertIn('"projprobe"', self.code)

    def test_the_verb_is_its_own_early_if(self):
        self.assertEqual(self.code.count(DISPATCH), 1)
        run = _no_strings(function_body(self.code, "static void RunCommand("))
        dispatch = _no_strings(DISPATCH)
        at = run.index(dispatch)
        before = run[:at]
        # At the top level of RunCommand, not inside any block ...
        self.assertEqual(before.count("{"), before.count("}"), "the dispatch sits inside a block")
        # ... and not one more branch of a chain.
        self.assertNotRegex(before, r"\belse\s*$", "the dispatch is chained with `else`")
        # Negative control: the chain it stays out of is still there.
        self.assertRegex(run, r"\belse if \(lc == ")

    def test_the_header_is_included_once_after_what_it_calls(self):
        self.assertEqual(self.plugin.count(INCLUDE_LINE), 1)
        self.assertIn(INCLUDE_LINE, strip_research_blocks(self.plugin))
        at = self.plugin.index(INCLUDE_LINE)
        # HookOneScript's forward declaration, SdkShortScriptName and Common.hpp
        # all precede the class that uses them.
        self.assertLess(self.plugin.index("static bool HookOneScript(const char* shortName"), at)
        self.assertLess(self.plugin.index("static consteval const char* SdkShortScriptName("), at)
        self.assertLess(self.plugin.index("#include <ForgePact/Common.hpp>"), at)

    def test_nothing_reaches_the_class_but_the_dispatch(self):
        # So nothing is hooked at load: the first non-zero `skillslider` value is
        # the only way into the class.
        code = self.code.replace(INCLUDE_LINE, "").replace(DISPATCH, "")
        self.assertNotIn("SkillSlidersMod", code)

    def test_naddrall_lists_the_five_scripts_outside_its_guard(self):
        k_names = self.code.split("static void NAddrAll()", 1)[1].split("std::ofstream", 1)[0]
        guard_at = k_names.index("#ifndef FORGEPACT_RELEASE")
        player_part = k_names[:guard_at]
        for name in SCRIPTS:
            self.assertEqual(k_names.count('"' + name + '"'), 1, name + " is not listed exactly once")
            self.assertIn('"' + name + '"', player_part, name + " is inside NAddrAll's research guard")
        # The player build's own list carries them too (naddrall is a player verb).
        player_k_names = self.player_build.split("static void NAddrAll()", 1)[1].split("std::ofstream", 1)[0]
        for name in SCRIPTS:
            self.assertIn('"' + name + '"', player_k_names, name)


class SkillSlidersHeaderRulesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = _read(HEADER)
        cls.code = strip_comments(cls.header)

    def test_no_research_block_but_counters(self):
        self.assertEqual(non_counter_research_lines(self.code), [])
        # Negative controls: the helper sees a research-only statement, and
        # lets a counter through.
        probe = "a();\n#ifndef FORGEPACT_RELEASE\nOut(\"x\");\n#endif\nb();\n"
        self.assertEqual(non_counter_research_lines(probe), ['Out("x");'])
        counter = "a();\n#ifndef FORGEPACT_RELEASE\n    BP_DIAG_INCREMENT(g_X);\n#endif\n"
        self.assertEqual(non_counter_research_lines(counter), [])
        player_half = "#ifdef FORGEPACT_RELEASE\nc();\n#else\nd();\n#endif\n"
        self.assertEqual(non_counter_research_lines(player_half), ["d();"])

    def test_hooks_install_only_on_the_arming_path(self):
        self.assertEqual(self.code.count("HookOneScript("), 1)
        handle = _no_strings(function_body(self.code, "void HandleCommand(const std::string& rest)"))
        install = handle.index("HookOneScript(")
        # A value is clamped, and a value of 0 returns, before anything installs.
        self.assertLess(handle.index("value = Clamp(lever, value);"), install)
        off = handle.index("if (value == 0.0) {")
        self.assertLess(off, install)
        self.assertIn("return;", handle[off:install])
        # A script already hooked is not hooked twice.
        self.assertIn("if (hook.orig) continue;", handle[:install])
        # The lever stores its value only once every script it needs is native.
        native_check = handle.index("const int blocked = FirstNotNative(lever);")
        self.assertLess(install, native_check)
        self.assertLess(native_check, handle.index("state.value = value;"))
        self.assertIn("&native)", handle[install:native_check])
        # The script's name and id reach HookOneScript from the table, not a literal.
        self.assertIn("HookOneScript(script.name, script.hookId, script.detour, &hook.orig, &native);", self.code)
        # Off by default: no hook, no value.
        self.assertIn("PFUNC_YYGMLScript orig = nullptr;", self.code)
        self.assertRegex(self.code, r"struct LeverState \{\s*double value = 0\.0;")

    def test_script_names_come_from_sdk_constants(self):
        table = function_body(self.code, "static const ScriptInfo* Scripts()")
        names = re.findall(r"SdkShortScriptName\(HeroSiege::Scripts::gml_Script_(\w+)\)", table)
        self.assertEqual(tuple(names), SCRIPTS)
        sdk = load_sdk_script_constants()
        for name in SCRIPTS:
            self.assertEqual(sdk.get("gml_Script_" + name), "gml_Script_" + name, name + " is not an SDK constant")
        # No script name is spelled as a string anywhere in the header's code.
        for name in SCRIPTS:
            self.assertNotRegex(self.code, r'"(?:gml_Script_)?' + name + '"', name + " is a literal")

    def test_objects_come_from_sdk_constants(self):
        for name in OBJECTS:
            self.assertIn("HeroSiege::Objects::GameObject::" + name, self.code)
            self.assertNotIn('"' + name + '"', self.code)
        self.assertNotIn('"Mercenary_obj"', self.code)
        resolve = function_body(self.code, "static void Resolve(int& index, HeroSiege::Objects::GameObject object)")
        self.assertIn('"asset_get_index"', resolve)
        self.assertIn("HeroSiege::Objects::GetObjectName(object)", resolve)
        # No object index typed by hand (Player_obj, Universal_Double_Cast_obj).
        self.assertNotRegex(self.code, r"\b(3553|5318)\b")

    def test_hook_ids_are_the_mods_own(self):
        table = function_body(self.code, "static const ScriptInfo* Scripts()")
        ids = re.findall(r'SdkShortScriptName\([^)]*\), "([^"]+)"', table)
        self.assertEqual(len(ids), len(SCRIPTS))
        self.assertEqual(len(set(ids)), len(ids), "two scripts share a hook id")
        for hook_id in ids:
            self.assertFalse(hook_id.startswith("fp_pp_"), hook_id + " is projprobe's id space")
            self.assertTrue(hook_id.startswith("fp_ss_"), hook_id)
        # No other hook in the plugin uses one.
        others = [strip_comments(_read(PLUGIN))]
        others += [strip_comments(_read(p)) for p in INCLUDE.glob("*.hpp") if p.name != HEADER.name]
        for hook_id in ids:
            for text in others:
                self.assertNotIn('"' + hook_id + '"', text, hook_id + " is used elsewhere")

    def test_no_address_and_no_struct_read(self):
        calls = re.findall(r"g_Yytk->(\w+)", self.code)
        self.assertTrue(calls)
        self.assertEqual(set(calls), {"CallBuiltin"})
        for forbidden in ("reinterpret_cast", "m_Functions", "GetModuleHandle", "Rva", "MmCreateHook",
                          "HookOneScriptTable", "GetInstanceObject", "0x14"):
            self.assertNotIn(forbidden, self.code, forbidden)


if __name__ == "__main__":
    unittest.main()
