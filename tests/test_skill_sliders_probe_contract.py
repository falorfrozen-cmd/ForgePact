#!/usr/bin/env python3
"""Contract tests for `projprobe` (issue #160), the projectile and AoE research instrument.

Where the game computes a player skill's projectile count, projectile speed
and area size is being measured, not yet known. `projprobe` hooks every
candidate script the static search turned up in one research build behind
one command, and carries three research-only levers (`amount`, `aoe`,
`speed`) so one live session measures baseline and boosted behaviour for all
three. A lever that works is evidence for the next workorder, not a feature.

These pins hold the properties that would otherwise rot quietly:

1. **Nothing reaches a player build.** The dispatch, every function, every
   hook and every lever sit inside one `#ifndef FORGEPACT_RELEASE` region.
2. **Every candidate is a named, listed row.** The fourteen hooked scripts
   (`LoadAllModifiers` included: the static reading puts the speed stats 74
   and 75 there, into the elements LoadProjectileSettings applies to the
   projectile's `deltaSpeed`) and the two enemy count rows are spelled
   through their hs-game-sdk constants and listed in `NAddrAll`'s `kNames`;
   the enemy rows only count.
3. **A probe never decides whether the game's code runs.** Every detour calls
   the original with its own arguments on every path, and a lever changes
   only the copy the caller gets afterwards.
4. **Off is off.** The levers start at their off values (0, 0, 1.0), each is
   clamped, and only a native install can be armed.
5. **A lever that moved nothing says so.** A multiplier on a native 0 counts
   as a no-op, not as applied, and the speed lever's stat form can add, so a
   stat a character does not carry can still be raised from 0.
6. **`ids` spends its budget on distinct ids**, one line per new (outer row,
   stat id) pair, and `show` lists every pair with its hits and last return.
7. **The marker line Live 1 reads is the one the plan spells.**

Self-contained on purpose, as test_bossprobe_object_contract.py is: the
helpers are duplicated here so the plain `unittest` and `discover -s tests`
forms both load it.
"""

import re
import unittest
from pathlib import Path

FORGEPACT_DIR = Path(__file__).resolve().parents[1]
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"

REGION_START = "\n#ifndef FORGEPACT_RELEASE\n// ---- projprobe: projectile count, projectile speed and AoE size research"
REGION_END = "\n#endif // FORGEPACT_RELEASE (projprobe)"
DISPATCH = 'if (lc == "projprobe") { ProjProbeCommand(rest); return; }'

HOOKED = (
    "StatAOESkillSize", "StatExplosionAOE", "ReturnExtraSpellProjectiles",
    "ReturnExtraProjectilesRanged", "LoadProjectileSettings", "LoadProjectile",
    "LoadAOEModifiers", "CreatePhysicalProjectile", "CA_playerProjectile",
    "TalentUseSetSpeed", "GetProjectileGravity", "AddAoeIndicatorSize",
    "CreateAoeIndicator", "LoadAllModifiers",
)
ENEMY_ROWS = ("CA_enemyProjectile", "ClientCreateEnemyProjectile")
LEVER_FLAGS = ("kPpOuter", "kPpAmount", "kPpAoe", "kPpSpeed", "kPpSpeedScope")
# The rows that may carry a lever or stack flag, and exactly which ones.
LEVER_ROWS = {
    "StatAOESkillSize": ["kPpOuter", "kPpAoe"],
    "ReturnExtraSpellProjectiles": ["kPpOuter", "kPpAmount"],
    "ReturnExtraProjectilesRanged": ["kPpOuter", "kPpAmount"],
    "LoadProjectileSettings": ["kPpOuter", "kPpSpeed", "kPpSpeedScope"],
    "LoadAllModifiers": ["kPpOuter", "kPpSpeedScope"],
}


def flag_set(flags):
    """The flag names a row's FLAGS expression carries, as whole words
    (`kPpSpeed` is not `kPpSpeedScope`)."""
    return sorted(set(re.findall(r"\bkPp\w+\b", flags)))


def strip_research_blocks(source):
    """What the player build compiles (FORGEPACT_RELEASE defined)."""
    kept, stack = [], []
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
        elif all(active for _, active in stack):
            kept.append(line)
    return "\n".join(kept)


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def function_body(source, signature):
    """The text between a function's outer braces (brace-matched), skipping
    forward declarations (a `;` before the opening brace)."""
    start = source.index(signature)
    open_at = source.index("{", start)
    while ";" in source[start:open_at]:
        start = source.index(signature, start + 1)
        open_at = source.index("{", start)
    depth = 0
    for i in range(open_at, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[open_at + 1:i]
    raise AssertionError("unbalanced braces after " + signature)


def statements(body):
    return [s.strip() for s in body.split(";") if s.strip()]


class ProjProbeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8-sig")
        start = cls.plugin.index(REGION_START)
        end = cls.plugin.index(REGION_END, start)
        cls.region_span = (start, end)
        cls.region = strip_comments(cls.plugin[start:end])
        cls.code = strip_comments(cls.plugin)
        cls.player_build = strip_comments(strip_research_blocks(cls.plugin))
        rows = re.search(r"#define PROJPROBE_SCRIPTS\(X\)(.*?)\n\s*\n", cls.region, re.S)
        cls.rows = re.findall(r"X\((\w+), HeroSiege::Scripts::gml_Script_(\w+), ([^)]*)\)", rows.group(1))

    def test_projprobe_is_research_only(self):
        # Exactly one region, and it is a research block of its own.
        self.assertEqual(self.plugin.count(REGION_START), 1)
        self.assertEqual(self.plugin.count(REGION_END), 1)
        opening, body = self.region.split("\n", 2)[1:]
        self.assertEqual(opening, "#ifndef FORGEPACT_RELEASE")
        self.assertNotRegex(body, r"(?m)^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b",
                            "the region holds no nested preprocessor block that could end it early")

        # Every ProjProbe function and every PpNat_ detour is defined inside it.
        start, end = self.region_span
        defined = list(re.finditer(r"\nstatic [^;{}()]*?\b((?:ProjProbe|PpNat_)\w*)\(", self.plugin))
        self.assertGreaterEqual(len(defined), 25, "the definitions were not found")
        for m in defined:
            self.assertTrue(start < m.start() < end, m.group(1) + " is defined outside the projprobe region")
        # And nothing the region declares is used outside it except through the
        # one dispatch line. (`g_Pp`/`kPp` alone is not ours: prospectprobe
        # shares the prefix, so the check is by name, not by prefix.)
        outside = self.code.replace(self.region, "").replace(DISPATCH, "")
        ours = set(re.findall(r"\b(ProjProbe\w+|PpNat_\w*|g_Pp\w+|kPp\w+|PROJPROBE_SCRIPTS)\b", self.region))
        self.assertGreaterEqual(len(ours), 60, "the region's names were not found")
        for name in sorted(ours):
            self.assertNotRegex(outside, r"\b" + re.escape(name) + r"\b", name + " is used outside the projprobe region")

        # The dispatch sits in RunCommand, once, inside a research block.
        self.assertEqual(self.code.count(DISPATCH), 1)
        self.assertIn(DISPATCH, function_body(self.code, "static void RunCommand("))
        self.assertNotRegex(self.player_build, r"(?i)projprobe|\bg_Pp|\bkPp")
        allowlist = re.search(r"kPlayerCommands = \{(?P<body>.*?)\};", self.code, re.S)
        self.assertNotIn('"projprobe"', allowlist.group("body"))

        # Negative control: the stripper keeps the player build itself.
        self.assertIn("static void RunCommand(", self.player_build)
        self.assertIn('"statadd"', self.player_build)

    def test_every_candidate_is_an_sdk_row_and_listed(self):
        names = [name for _, name, _ in self.rows]
        self.assertEqual(sorted(names), sorted(HOOKED + ENEMY_ROWS))
        for safe, name, flags in self.rows:
            # The row's identifier and its SDK constant name the same script.
            self.assertEqual(safe, name)
            if name in ENEMY_ROWS:
                self.assertEqual(flags.strip(), "kPpCount", name + " is the enemy side: count only, never a lever")
            else:
                self.assertIn("kPpLog", flags, name)
        flags = {name: f for _, name, f in self.rows}
        for name, f in flags.items():
            lever = sorted(x for x in flag_set(f) if x in LEVER_FLAGS)
            self.assertEqual(lever, sorted(LEVER_ROWS.get(name, [])), name + "'s lever and stack flags")
        # The flags are distinct bits, so `kPpSpeed` and `kPpSpeedScope` never alias.
        bits = dict(re.findall(r"\b(kPp(?:Count|Log|Outer|Amount|Aoe|Speed|SpeedScope))\s*=\s*(\d+),", self.region))
        self.assertEqual(sorted(bits), sorted(("kPpCount", "kPpLog") + LEVER_FLAGS))
        values = [int(v) for k, v in bits.items() if k != "kPpCount"]
        self.assertEqual(len(set(values)), len(values))
        for v in values:
            self.assertEqual(v & (v - 1), 0, "flag %d is one bit" % v)
        # The short name handed to HookOneScript comes from the SDK constant.
        self.assertIn("{ SdkShortScriptName(NAME), FLAGS,", self.region)

        k_names = self.code.split("static void NAddrAll()", 1)[1].split("std::ofstream", 1)[0]
        for name in HOOKED + ENEMY_ROWS + ("ReturnSpecificStat",):
            self.assertIn('"' + name + '"', k_names, name + " missing from NAddrAll's kNames")
        # statadd's pin on the same list still holds.
        self.assertIn('"StatSpellHaste", "StatAllSkills",', k_names)

    def test_levers_start_off_and_are_clamped(self):
        self.assertRegex(self.region, r"static int\s+g_PpAmount = 0;")
        self.assertRegex(self.region, r"static double\s+g_PpAoe = 0\.0;")
        self.assertRegex(self.region, r"static double\s+g_PpSpeedMult = 1\.0;")
        self.assertRegex(self.region, r"static int\s+g_PpSpeedStatId = -1;")
        self.assertIn("static std::atomic<bool> g_PpIds{ false };", self.region)

        self.assertIn("static int ProjProbeClampAmount(double k) { return (int)std::lround(std::clamp(k, 0.0, 10.0)); }",
                      self.region)
        self.assertIn("static double ProjProbeClampAoe(double bonus) { return std::clamp(bonus, 0.0, 300.0); }", self.region)
        self.assertIn("static double ProjProbeClampSpeed(double mult) { return std::clamp(mult, 1.0, 4.0); }", self.region)
        for command, clamp, store in (
                ("static void ProjProbeAmountCommand(", "ProjProbeClampAmount(asked)", "g_PpAmount = k;"),
                ("static void ProjProbeAoeCommand(", "ProjProbeClampAoe(asked)", "g_PpAoe = bonus;"),
                ("static void ProjProbeSpeedCommand(", "ProjProbeClampSpeed(asked)", "g_PpSpeedMult = mult;")):
            body = function_body(self.region, command)
            self.assertIn(clamp, body, command)
            # Clamped before it is stored, and stored only after the arm check.
            self.assertLess(body.index(clamp), body.index(store), command)
            self.assertLess(body.index("ProjProbeArm("), body.index(store), command)
        # NaN and infinity never reach a clamp.
        self.assertIn("!std::isfinite(v)", function_body(self.region, "static bool ProjProbeParseNumber("))

        # The off value is a pass-through: each lever acts only away from it.
        levers = function_body(self.region, "static void ProjProbeApplyLevers(")
        self.assertIn("if (t.mode != kPpNative) return;", levers)
        self.assertIn("g_PpAmount != 0", levers)
        self.assertIn("g_PpAoe != 0.0", levers)
        self.assertIn("g_PpSpeedMult != 1.0", levers)
        self.assertIn("g_PpSpeedMult != 1.0", function_body(self.region, "static void ProjProbeAfterStat("))
        # Off restores the pass-through without arming anything.
        self.assertIn("if (k == 0) { g_PpAmount = 0;", function_body(self.region, "static void ProjProbeAmountCommand("))
        self.assertIn("if (bonus == 0.0) { g_PpAoe = 0.0;", function_body(self.region, "static void ProjProbeAoeCommand("))
        off = function_body(self.region, "static void ProjProbeSpeedOff(")
        self.assertIn("g_PpSpeedMult = 1.0;", off)
        self.assertIn("g_PpSpeedStatId = -1;", off)

        # Only a native install is armed; TABLE-ONLY is refused by name.
        arm = function_body(self.region, "static bool ProjProbeArm(")
        self.assertIn("if (g_PpRows[idx].mode != kPpNative) {", arm)
        self.assertIn("not armed", arm)
        attach = function_body(self.region, "static void ProjProbeAttach(")
        self.assertIn("HookOneScript(t.name, t.hookId, t.detour, &t.orig, &native)", attach)
        self.assertIn("TABLE-ONLY", attach)
        stat = function_body(self.region, "static bool ProjProbeAttachStat(")
        self.assertRegex(stat, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_ReturnSpecificStat\),")
        self.assertIn("&native)", stat)
        # Nothing is installed at load: the ReturnSpecificStat hook goes in only
        # from `ids on` and the speed lever's stat form.
        self.assertEqual(self.code.count("ProjProbeAttachStat()"), 3)
        ids = function_body(self.region, "static void ProjProbeIdsCommand(")
        self.assertIn("ProjProbeAttachStat()", ids)
        speed_command = function_body(self.region, "static void ProjProbeSpeedCommand(")
        self.assertIn("ProjProbeAttachStat()", speed_command)
        # `ids` attributes stat ids to every outer row, LoadAllModifiers included,
        # and the outer rows are exactly the ones flagged so.
        outer = sorted(n for n, f in LEVER_ROWS.items() if "kPpOuter" in f)
        self.assertEqual(sorted(re.findall(r"kPp_(\w+)", ids)), outer)
        # The speed lever's stat form arms both scopes it applies under; the
        # instance form arms LoadProjectileSettings alone.
        self.assertIn('ProjProbeArm({ kPp_LoadAllModifiers, kPp_LoadProjectileSettings }, "speed stat")', speed_command)
        self.assertIn('ProjProbeArm({ kPp_LoadProjectileSettings }, "speed")', speed_command)
        self.assertIn("static constexpr long kProjProbeLogBudget = 40;", self.region)
        self.assertIn("static constexpr long kPpIdsBudget = 200;", self.region)
        # Names, never addresses.
        for token in ("HookOneScriptTable", "MmCreateHook", "Rva", "GetModuleHandle"):
            self.assertNotIn(token, self.region)

    def test_every_hook_calls_the_original_whatever_the_probe(self):
        # Every row's detour is the shared body, nothing more.
        self.assertIn("{ return ProjProbeDetourBody(kPp_##SAFE, S, O, R, argc, A); }", self.region)

        call_original = function_body(self.region, "static RValue& ProjProbeCallOriginal(")
        self.assertEqual(statements(call_original),
                         ["const ProjProbeOuterScope scope(t)", "return t.orig ? t.orig(S, O, R, argc, A) : R"])

        for signature, call in (
                ("static RValue& ProjProbeDetourBody(", "RValue& r = ProjProbeCallOriginal(t, S, O, R, argc, A);"),
                ("static RValue& ProjProbeHookReturnSpecificStat(",
                 "RValue& r = g_PpOrigReturnSpecificStat ? g_PpOrigReturnSpecificStat(S, O, R, argc, A) : R;")):
            body = function_body(self.region, signature)
            code = re.sub(r'"(?:\\.|[^"\\])*"', '""', body)   # string literals out
            self.assertEqual(body.count(call), 1, signature)
            at = body.index(call)
            # Unconditional: outside every block, and nothing returns before it.
            self.assertEqual(body[:at].count("{"), body[:at].count("}"), signature + ": the original call sits inside a block")
            self.assertNotIn("return", body[:at], signature)
            self.assertEqual(body.count("return"), 1, signature)
            self.assertEqual(statements(body)[-1], "return r", signature)
            for arg in ("S", "O", "R", "argc", "A"):
                self.assertNotRegex(code, r"(?<![\w.>])" + arg + r"\s*=(?!=)", signature + " reassigns " + arg)

        # What reads the arguments only reads them.
        for signature in ("static std::string ProjProbeCallLine(", "static bool ProjProbeStatIdArg(",
                          "static RValue& ProjProbeHookReturnSpecificStat(", "static RValue& ProjProbeDetourBody("):
            fn = re.sub(r'"(?:\\.|[^"\\])*"', '""', function_body(self.region, signature))
            self.assertNotRegex(fn, r"\*\s*A\s*\[[^\]]*\]\s*=(?!=)", signature)
            self.assertNotRegex(fn, r"(?<![\w.>])A\s*\[[^\]]*\]\s*(?:->[^=;]*)?=(?!=)", signature)

        # A lever changes the result on a copy, and reads a number only after
        # checking its kind (ToDouble on another kind raises the runner's error).
        adjust = function_body(self.region, "static bool ProjProbeAdjust(")
        self.assertLess(adjust.index("N1Numeric(r)"), adjust.index("r.ToDouble()"))
        self.assertLess(adjust.index("N1Numeric(e0)"), adjust.index("e0.ToDouble()"))
        self.assertIn('"array_create"', adjust)
        self.assertIn("r = copy;", adjust)
        # `speed <mult>` scales self's `deltaSpeed` first and `speed` as a second
        # write, through the instance-variable builtins; a variable that is not
        # a number is left alone, never created.
        scale_var = function_body(self.region, "static bool ProjProbeScaleVar(")
        get = scale_var.index('"variable_instance_get", { inst, RValue(var) }')
        check = scale_var.index("if (!N1Numeric(v))")
        self.assertLess(get, check)
        self.assertLess(check, scale_var.index("v.ToDouble()"))
        self.assertLess(check, scale_var.index('"variable_instance_set", { inst, RValue(var), RValue(native * mult) }'))
        speed = function_body(self.region, "static void ProjProbeScaleSpeed(")
        delta_at = speed.index('ProjProbeScaleVar(inst, "deltaSpeed", mult,')
        self.assertLess(delta_at, speed.index('ProjProbeScaleVar(inst, "speed", mult,'))
        # Both values are logged before and after, within a budget reset clears.
        self.assertIn("g_PpSpeedLogged < kProjProbeLogBudget", speed)
        self.assertIn("InterlockedExchange(&g_PpSpeedLogged, 0);", function_body(self.region, "static void ProjProbeReset("))
        name = function_body(self.region, "static std::string ProjProbeSelfName(")
        self.assertLess(name.index("IsNumericInstanceRead(oi)"), name.index("object_get_name"))

        # Each lever prints its first boosted call once per arming.
        first = function_body(self.region, "static void ProjProbeFirstBoost(")
        self.assertIn("InterlockedCompareExchange(&first, kPpFirstShown, kPpFirstPending) != kPpFirstPending", first)
        self.assertIn('"projprobe %s: first boosted call %g -> %g"', first)

        # `ids` reads the stack depth of this thread only.
        for name in ("g_PpOuterDepth", "g_PpSpeedScopeDepth", "g_PpOuterName"):
            self.assertRegex(self.region, r"static thread_local [\w\s\*]+\b" + name + r" = ")
        self.assertIn("const bool watched = g_PpOuterDepth > 0;",
                      function_body(self.region, "static RValue& ProjProbeHookReturnSpecificStat("))
        self.assertIn("g_PpSpeedScopeDepth > 0", function_body(self.region, "static void ProjProbeAfterStat("))
        # The speed stat form's scope is the rows flagged kPpSpeedScope
        # (LoadAllModifiers and LoadProjectileSettings), counted on entry.
        scope = self.region[self.region.index("struct ProjProbeOuterScope"):]
        scope = scope[:scope.index("};")]
        self.assertIn("(t.flags & kPpSpeedScope) != 0", scope)
        self.assertIn("++g_PpSpeedScopeDepth", scope)
        self.assertIn("--g_PpSpeedScopeDepth", scope)
        self.assertIn('"projprobe ids: "', self.region)

    def test_a_lever_that_moved_nothing_says_so(self):
        # A multiplier on a native 0 (a character with no projectile-speed gear)
        # leaves 0. That is "ran and did nothing", never an applied boost.
        after_stat = function_body(self.region, "static void ProjProbeAfterStat(")
        noop_at = after_stat.index("InterlockedIncrement(&g_PpStatNoop);")
        self.assertLess(after_stat.index("if (boosted == native) {"), noop_at)
        self.assertLess(noop_at, after_stat.index("InterlockedIncrement(&g_PpStatApplied);"))
        self.assertIn("native 0: a multiplier cannot move it", self.region)
        speed = function_body(self.region, "static void ProjProbeScaleSpeed(")
        self.assertIn("InterlockedIncrement(&g_PpSpeedNoop);", speed)
        self.assertIn("deltaMoved || speedMoved", speed)
        scale_var = function_body(self.region, "static bool ProjProbeScaleVar(")
        self.assertLess(scale_var.index("if (native == 0.0)"), scale_var.index('"variable_instance_set"'))
        show = function_body(self.region, "static void ProjProbeShow(")
        for counter in ("statNoop=", "speedNoop="):
            self.assertIn(counter, show)
        reset = function_body(self.region, "static void ProjProbeReset(")
        for counter in ("g_PpStatNoop", "g_PpSpeedNoop"):
            self.assertIn("InterlockedExchange(&" + counter + ", 0);", reset)

        # So the stat form can also add, which raises a stat from 0: off at 0,
        # clamped, stored only after the arm check, and cleared by off.
        self.assertRegex(self.region, r"static double\s+g_PpSpeedAdd = 0\.0;")
        self.assertIn("static double ProjProbeClampSpeedAdd(double bonus) { return std::clamp(bonus, 0.0, 100.0); }",
                      self.region)
        speed_command = function_body(self.region, "static void ProjProbeSpeedCommand(")
        self.assertIn('== "add"', speed_command)
        self.assertLess(speed_command.index("ProjProbeClampSpeedAdd(asked)"), speed_command.index("g_PpSpeedAdd = bonus;"))
        self.assertLess(speed_command.index("ProjProbeArm("), speed_command.index("g_PpSpeedAdd = bonus;"))
        self.assertIn("g_PpSpeedAdd = 0.0;", function_body(self.region, "static void ProjProbeSpeedOff("))
        self.assertIn("ProjProbeAdjust(r, g_PpSpeedAdd, 1.0, native, boosted)", after_stat)
        self.assertIn("g_PpSpeedAdd != 0.0", after_stat)

    def test_ids_spends_its_budget_on_distinct_ids(self):
        # One line per new (outer row, stat id) pair, not per call:
        # LoadProjectileSettings alone makes about 30 dispatcher calls a
        # projectile, which would spend the line budget before 74/75 appear.
        after_stat = function_body(self.region, "static void ProjProbeAfterStat(")
        record_at = after_stat.index("ProjProbeIdsRecord(")
        self.assertLess(record_at, after_stat.index('"projprobe ids: "'))
        self.assertLess(record_at, after_stat.index("InterlockedIncrement(&g_PpIdsLogged)"))
        record = function_body(self.region, "static bool ProjProbeIdsRecord(")
        self.assertIn("std::lock_guard<std::mutex> lock(g_PpIdsLock);", record)
        self.assertIn("++e.hits;", record)
        self.assertIn("e.lastRet = ret;", record)
        self.assertIn("g_PpIdsSeen.size() >= kPpIdsTableMax", record)
        self.assertIn("InterlockedIncrement(&g_PpIdsDropped);", record)
        # `show` lists every pair with no budget, and names a saturated instrument.
        show = function_body(self.region, "static void ProjProbeShow(")
        self.assertIn("for (const ProjProbeIdSeen& e : seen)", show)
        self.assertIn(" hits=", show)
        self.assertIn(" lastRet=", show)
        self.assertGreaterEqual(show.count("SATURATED"), 2)
        reset = function_body(self.region, "static void ProjProbeReset(")
        self.assertIn("g_PpIdsSeen.clear();", reset)
        self.assertIn("InterlockedExchange(&g_PpIdsDropped, 0);", reset)
        # `ids on` names an outer row it cannot watch instead of reporting on.
        ids = function_body(self.region, "static void ProjProbeIdsCommand(")
        self.assertIn("if (g_PpRows[idx].mode != kPpNative)", ids)
        self.assertIn("NOT watched", ids)

    def test_status_line_is_the_marker_live_one_reads(self):
        status = function_body(self.region, "static void ProjProbeStatus(")
        fmt = "projprobe: hooks=%d/%d amount=+%d aoe=+%g speed=x%.2f ids=%s"
        self.assertIn('"' + fmt + '"', status)
        self.assertRegex(status, r"ProjProbeNativeCount\(\), \(int\)kPpRowCount, g_PpAmount, g_PpAoe, g_PpSpeedMult,\s*"
                                 r"g_PpIds\.load\(\) \? \"on\" : \"off\"")
        self.assertIn("if (sub.empty()) { ProjProbeStatus(); return; }",
                      function_body(self.region, "static void ProjProbeCommand("))
        # Rendered at the all-off state with the row total, it is Live 1's marker.
        self.assertEqual(len(self.rows), 16)
        self.assertEqual(fmt % (0, len(self.rows), 0, 0.0, 1.0, "off"),
                         "projprobe: hooks=0/16 amount=+0 aoe=+0 speed=x1.00 ids=off")
        # And the hook summary is the plan's line.
        self.assertIn('Out("projprobe hook: " + std::to_string(native) + " native, " + std::to_string(via) + " via hook, "',
                      function_body(self.region, "static void ProjProbeHook("))


if __name__ == "__main__":
    unittest.main()
