"""Source and text contract for Headhunter and Tyrant's Crown from the game's own Angelic roll (#74).

#63 put both items into ForgePact's own Angelic/Unholy pool, so they dropped on the
slider's die whatever their World switches said. #74 (owner-directed, 2026-10-02) takes
them out of that pool again and moves them onto the game's own roll: a
`CreateDefaultParams` call while `DropItemAngelicChance` runs is a hit (the roll returns
undefined either way), and on each hit, while a panel switch is on, the switched-on items
roll one pool entry's share and drop beside the game's own item. The owner's decision of
2026-10-02 ("Panel switch only") makes the gate the panel switch (`force`), not the
mechanic's enabled state, which a forged item's auto-arm also sets.

`test_angelic_hit_behavior.py` runs the hook natively but skips without a C++ toolchain;
this file pins the same shape on the source text, so it is checked everywhere the suite
runs: the pool no longer carries the items, the detection is installed by name only from
the switches' `force` paths and the research levers (never at startup, never by the
auto-arm), the gate reads the panel switch, the roll-in-progress state is a scope guard, the status
line carries the counters the live procedure reads, the research levers stay out of the
player build, and the player-facing texts say what the items now do.
"""
import importlib.util
import os
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE", ROOT / "plugin" / "ModuleMain.cpp")).read_text(encoding="utf-8").replace("\r\n", "\n")
PANEL_SRC = pathlib.Path(os.environ.get("FORGEPACT_TEST_PANEL_SRC", str(ROOT / "panel" / "src")))

# One definition of "what the player build compiles", shared with the release contract.
_spec = importlib.util.spec_from_file_location("_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
strip_comments = _release.strip_comments


def body(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def read(path):
    return pathlib.Path(path).read_text(encoding="utf-8").replace("\r\n", "\n")


class AngelicHitSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code = strip_comments(SOURCE)
        cls.shipped = strip_research_blocks(SOURCE)
        cls.shipped_code = strip_comments(cls.shipped)

    # ---- the pool no longer carries the two items --------------------------------------

    def test_the_pool_append_is_gone(self):
        self.assertNotIn("AppendSignatureCandidates", self.code)

    def test_a_pool_candidate_has_no_signature_member(self):
        struct = re.search(r"struct AngelicCandidate \{([^}]*)\}", self.code)
        self.assertIsNotNone(struct, "struct AngelicCandidate")
        self.assertNotIn("signature", struct.group(1))

    def test_the_slider_never_spawns_a_signature_item(self):
        kill = body(self.code, "static void AngelicDropOnKill(CInstance* S)")
        self.assertNotIn("SpawnSignatureItem(", kill)
        self.assertIn("SpawnAngelicItem(", kill)

    # ---- detection: CreateDefaultParams, by name, from the switches only ---------------

    def test_create_default_params_is_hooked_once_inside_the_installer(self):
        self.assertEqual(self.code.count('HookOneScript("CreateDefaultParams"'), 1)
        install = body(self.code, "static void InstallSignatureAngelicHooks(")
        self.assertIn('HookOneScript("CreateDefaultParams"', install)
        self.assertIn("&g_Orig_CreateDefaultParams", install)
        self.assertIn("(PVOID)HookAngelicChance, &g_OrigAngChance", install)
        self.assertRegex(install, r"if \(!g_OrigAngChance\)")
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertIn("g_Orig_CreateDefaultParams(S, O, R, argc, A)", hook)

    def test_the_installer_is_in_the_player_build(self):
        self.assertIn("static void InstallSignatureAngelicHooks(", self.shipped_code)
        self.assertIn('HookOneScript("CreateDefaultParams"', self.shipped_code)

    def test_installed_by_the_switches_never_at_startup(self):
        for signature in ("static void InstallHook()", "EXPORTED AurieStatus ModuleInitialize"):
            with self.subTest(function=signature):
                self.assertNotIn("InstallSignatureAngelicHooks", body(self.code, signature))
        frame = body(self.code, "void FrameCallback(FWFrame& FrameContext)")
        for name in ("InstallSignatureAngelicHooks", "SignatureDropOnAngelicHit", "g_SigGame"):
            self.assertNotIn(name, frame, "nothing new runs every frame")
        # Owner, 2026-10-02 ("Panel switch only"): a forged item's auto-arm turns its mechanic
        # on at every launch but never the drop, so neither it nor the enable path it shares
        # with `headhunter on` installs the detection.
        for signature in ("static void EnableHeadhunter()", "static void HeadhunterAutoArm()", "static void TyrantAutoArm()"):
            with self.subTest(function=signature):
                self.assertNotIn("InstallSignatureAngelicHooks", body(self.code, signature))
        # Only the panel switch's own command, `headhunter force` / `tyrant force`, installs it:
        # every line of the command that does names the `force` value and no other.
        command = body(self.code, "static bool HandleHeadhunterCommand(")
        for verb in ("headhunter", "tyrant"):
            with self.subTest(command=verb):
                start = command.index('lc == "%s"' % verb)
                branch = command[start:command.index("} else if (lc ==", start)]
                lines = [line for line in branch.splitlines() if "InstallSignatureAngelicHooks" in line]
                self.assertTrue(lines, "the %s command's force path installs the detection" % verb)
                for line in lines:
                    self.assertIn('== "force"', line)
                    for other in ('"on"', '"1"', '"off"', '"0"'):
                        self.assertNotIn(other, line, "only `force` installs the detection, not on/1 or off")
        # The research levers install it too, so gameHits= counts with both switches off.
        self.assertIn("InstallSignatureAngelicHooks();", body(self.code, "static void AngelicHitCommand("))

    def test_the_gate_reads_the_panel_switch(self):
        # The flags `tyrant force` / `headhunter force` set and `off` clears, which the panel
        # sends; the auto-arm from a forged item sets only g_TyEnabled / g_HhEnabled.
        gate = body(self.code, "static bool SignatureSwitchOn(")
        self.assertIn("g_TyForced", gate)
        self.assertIn("g_HhForced", gate)
        self.assertNotIn("Enabled", gate)
        # The auto-arm log lines say what the gate does, not what the mechanic does.
        self.assertIn("SignatureSwitchOn(0)", body(self.code, "static void TyrantAutoArm()"))
        self.assertIn("SignatureSwitchOn(1)", body(self.code, "static void HeadhunterAutoArm()"))

    # ---- the roll-in-progress state is a scope guard ------------------------------------

    def test_the_roll_state_is_a_scope_guard_in_the_player_build(self):
        hook = strip_comments(body(self.shipped, "static RValue& HookAngelicChance("))
        self.assertNotIn("ApRoll", hook)
        for token in ("++g_SigRollDepth", "--g_SigRollDepth", "g_SigRollDepth++", "g_SigRollDepth--"):
            self.assertNotIn(token, hook, "a bare ++/-- leaves the state raised when the game's roll throws")
        guard = re.search(r"\b([A-Za-z_]\w*Scope)\s+\w+\s*;", hook)
        self.assertIsNotNone(guard, "HookAngelicChance declares the roll-in-progress guard")
        self.assertFalse(guard.group(1).startswith("ApRoll"))
        self.assertLess(guard.start(), hook.index("g_OrigAngChance(S"),
                        "the guard is raised before the first original call and, living to the "
                        "end of the function, covers the extra-roll loop")
        struct = re.search(r"struct %s \{(.*?)\n\};" % guard.group(1), self.shipped_code, re.S)
        self.assertIsNotNone(struct)
        self.assertRegex(struct.group(1), r"%s\(\)\s*\{\s*\+\+g_SigRollDepth;" % guard.group(1))
        self.assertRegex(struct.group(1), r"~%s\(\)\s*\{\s*--g_SigRollDepth;" % guard.group(1))

    def test_the_research_depth_guard_is_untouched(self):
        full = strip_comments(body(SOURCE, "static RValue& HookAngelicChance("))
        self.assertRegex(full, r"\bApRollChanceDepthScope\s+\w+\s*;")

    # ---- counters and status -----------------------------------------------------------

    def test_sigdrop_status_carries_the_live_procedure_tokens(self):
        status = body(self.code, "static void SigDropStatus()")
        for token in ("gameRolls=", "gameHits=", "shareRolls=", "sigFromGame=", "crown=", "belt=",
                      "gate=tyrant:", ",headhunter:", "force ", " | rolls=", " drops=", " fails=",
                      '" cdpCalls="', '" detect="'):
            self.assertIn(token, status)

    def test_both_status_lines_carry_the_detection_route(self):
        # The live procedure reads detect= and cdpCalls= before it trusts gameHits=0: an
        # unreachable hook must not read as a roll that never hit.
        for signature in ("static void SigDropStatus()", "static void AngelicHitStatus()"):
            with self.subTest(status=signature):
                status = body(self.code, signature)
                self.assertRegex(status, r'" cdpCalls=" \+ std::to_string\(g_SigCdpCalls\) \+ " detect=" \+ g_SigDetectRoute\);\s*\}$')
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertLess(hook.index("InterlockedIncrement(&g_SigCdpCalls)"), hook.index("g_SigRollDepth"),
                        "cdpCalls counts every call that reaches the hook, before the roll check")
        self.assertIn('static const char* g_SigDetectRoute = "off";', self.shipped_code)
        self.assertIn("g_SigDetectRoute = cdpRoute;", body(self.code, "static void InstallSignatureAngelicHooks("))

    def test_one_angelic_hit_line_per_hit(self):
        hit = body(self.code, "static void SignatureDropOnAngelicHit(")
        self.assertIn('"angelic hit:', hit)
        self.assertIn("SpawnSignatureItem(", hit)
        self.assertIn("SignatureShare(", hit)
        self.assertIn("BuildAngelicPool(false)", hit)
        self.assertIn("g_AngelicPool.size()", hit)

    # ---- the research levers never reach a player -----------------------------------

    def test_the_hit_levers_are_research_build_only(self):
        self.assertIn('"angelicprobe hit', SOURCE)
        self.assertNotIn("angelicprobe hit", self.shipped)
        for lever in ("chance", "rate", "share", "off", "status"):
            self.assertIn('"%s"' % lever, body(self.code, "static void AngelicHitCommand("))
        self.assertNotIn("AngelicHitCommand", self.shipped)

    def test_no_new_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", SOURCE, re.S)
        self.assertIsNotNone(allowlist)
        self.assertNotIn("angelichit", allowlist.group("body"))
        self.assertNotIn("angelicprobe", allowlist.group("body"))


class AngelicHitPlayerTextTests(unittest.TestCase):
    """What the `player-text` item wrote, as assertions (the plan's three checks)."""

    def test_panel_text(self):
        loot = read(PANEL_SRC / "tabs" / "Loot.svelte")
        card = re.search(r'id="angelicCard".*?id="angelicnote"', loot, re.S)
        self.assertIsNotNone(card)
        self.assertNotIn("Headhunter", card.group(0), "the slider no longer drops Headhunter")
        self.assertNotIn("Tyrant", card.group(0), "the slider no longer drops Tyrant's Crown")
        mods = read(PANEL_SRC / "tabs" / "Mods.svelte")
        headhunter = mods.index('id="headhunter"')
        tyrant = mods.index('id="tyrant"')
        self.assertIn("Angelic", mods[headhunter - 1500:headhunter])
        self.assertIn("Angelic", mods[tyrant - 1500:tyrant])

    def test_readme(self):
        text = read(ROOT / "README.md")
        start = text.index("\n### Signature drops\n")
        end = text.find("\n### ", start + 1)
        section = text[start:] if end < 0 else text[start:end]
        for token in ("Angelic roll", "Headhunter", "Tyrant", "sigdrop"):
            self.assertIn(token, section)
        self.assertIn("switch", section.lower())
        row = [line for line in text.splitlines() if line.startswith("| **Angelic / Unholy Drops")][0]
        self.assertNotIn("Headhunter", row)
        self.assertNotIn("Tyrant", row)

    def test_release_notes(self):
        notes = read(ROOT / "release-notes-v2.2.0.md")
        self.assertTrue(notes.startswith("# ForgePact 2.2.0"))
        for token in ("Release date:", "Headhunter", "Tyrant's Crown", "Angelic", "## How to update"):
            self.assertIn(token, notes)


if __name__ == "__main__":
    unittest.main()
