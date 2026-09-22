"""Contract tests for `angelicprobe`, the research instrument for issue #64.

Where the game's own Angelic roll picks a unique is known from a static
reading (docs/angelic-drop-research.md) but was never *measured* on the
current build: which named script the roll reaches, whether it considers one
candidate per call or walks its list, and which placement call a hit would
use. `angelicprobe` observes every plausible step in one build, and
docs/angelic-roll-hook-research.md records what came back. Nothing here is
player-visible.

These tests pin what would otherwise rot quietly.

1. **The verb never reaches a player build.** The literal disappears when the
   research blocks are stripped; the three places the player build compiles
   around it (`HookAngelicChance`, DropManager's `FP_DROP_HOOK` and
   `InstallHook`) compile to exactly what they compiled to at `137a403` - the
   player-build comparison class below, run with `FORGEPACT_TEST_BASE_DIR`
   naming a directory that holds that commit's `ModuleMain.cpp` and
   `DropManager.hpp`.
2. **Every row is attached by a route that can see compiled GML's direct
   calls, or says it cannot.** Seven of the seventeen candidate scripts are
   already held by a ForgePact hook when the research build finishes starting
   up, so a probe install of its own would be table-only and blind. Those rows
   carry the existing hook's saved original and name, the `tgprobe` shape, and
   a row with no route prints `calls=n/a`, never `0` (`AGENTS.md`, "Prove the
   Instrument Before Trusting a Negative Result").
3. **Names, never addresses, and never the one call that hangs the game.**
   Every detour target is a named table entry or a saved original a named
   install captured, checked to be code inside the game before it is patched;
   nothing calls `DropItemAngelic`, which loops forever on an empty zone list.
4. **The document cannot claim a result before there is one.** Until the live
   session's own record exists, the results and decisions read `pending`;
   once it exists they may not.

Modelled on test_menu_probe_contract.py (same helpers, same dispatch rule)
and test_toggle_skill_contract.py (the three attach routes).
"""
import importlib.util
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE",
                             ROOT / "plugin" / "ModuleMain.cpp"))
DROP_MANAGER = ROOT / "plugin" / "include" / "ForgePact" / "DropManager.hpp"
DOC = ROOT / "docs" / "angelic-roll-hook-research.md"
# The live session's own record lives in the hub checkout this module sits in
# (the hub's .claude/workorders/ directory, which git ignores). In a
# standalone clone it never exists, so there the document may stay `pending`.
LIVE_RECORD = (ROOT.parent / ".claude" / "workorders"
               / "forgepact-angelic-roll-headhunter-live-1.md")
BASE_DIR = os.environ.get("FORGEPACT_TEST_BASE_DIR")

# One definition of "what the player build compiles", shared with the release
# contract rather than copied, so the two can never disagree about it.
_spec = importlib.util.spec_from_file_location(
    "_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
function_body = _release.function_body
strip_comments = _release.strip_comments
call_is_inside_addr_check_guard = _release.call_is_inside_addr_check_guard

#: The seventeen candidate rows: id -> the SDK script it observes.
CANDIDATES = {
    "drop-item": "DropItem",
    "angelic-chance": "DropItemAngelicChance",
    "angelic-forced": "DropItemAngelic",
    "drop-boss": "DropItemBoss",
    "drop-heroic": "DropItemHeroic",
    "drop-debug": "DropItemDebug",
    "drop-unique": "DropUniqueItems",
    "unique-random-id": "GetUniqueRandomItemID",
    "unique-repo": "GetUniqueRepoStruct",
    "unique-charm": "GetUniqueCharm",
    "angelic-charm": "DropAngelicCharm",
    "angelic-key": "DropAngelicKey",
    "default-params": "CreateDefaultParams",
    "loot-create": "LootGroundCreate",
    "loot-create-item": "LootGroundCreateFromItem",
    "loot-drop": "LootGroundDrop",
    "rare-announce": "GetRareDropAnnouncement",
}

#: Held at startup by DropManager::InstallHooks (a native first install), so
#: the probe counts them from a note in that hook's body - route 3.
DROP_MANAGER_ROWS = ("drop-item", "drop-boss", "angelic-forced",
                     "angelic-key", "angelic-charm")

#: Held at startup by InstallItemInspectHooks' table-only hooks, so the
#: saved original is still the game's own code - route 2.
TABLE_ONLY_ROWS = ("loot-create", "loot-create-item")

#: Names the static search found and deliberately did not hook; the document
#: has to say so, or the next session re-derives the list.
REJECTED_NAMES = (
    "DefineItemUnique", "LoadAngelicAugment", "DialogOpenAngelicRealm",
    "UiAAngelicRealm", "StashUniqueAddItemOnline", "UiAStashTabUniqueBuy",
    "LootGroundInit", "LootGroundDraw", "LootGroundDeActiveStep",
    "LootGroundRelicStep", "Loot_Manager_obj", "cpr_irandom",
)

DOC_HEADINGS = (
    "## Static search",
    "## Candidates and controls",
    "## Instrument",
    "## Live procedure",
    "## Results",
    "## Negative results, sourced",
    "## Decision",
)

VERDICTS = ("works", "not observed", "unmeasured")

BLOCK_START = "// Research instrument for docs/angelic-roll-hook-research.md"
BLOCK_END = "#endif // FORGEPACT_RELEASE (angelicprobe)"


def normalised(text):
    """Code with comments and every run of whitespace collapsed."""
    return re.sub(r"\s+", " ", strip_comments(text.replace("\r\n", "\n"))).strip()


def macro_definition(source, name):
    """The full text of `#define name(...)`, continuation lines included."""
    source = source.replace("\r\n", "\n")
    start = source.index("#define " + name + "(")
    end = start
    while True:
        line_end = source.index("\n", end)
        if not source[end:line_end].rstrip().endswith("\\"):
            return source[start:line_end]
        end = line_end + 1


def section(doc, heading):
    """From a line that is exactly `heading` to the next `## ` heading."""
    doc = doc.replace("\r\n", "\n")
    start = doc.index("\n" + heading + "\n") + 1
    following = doc.find("\n## ", start + len(heading))
    return doc[start:] if following < 0 else doc[start:following]


class AngelicProbeSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.shipped = strip_research_blocks(cls.plugin)
        cls.drop_manager = DROP_MANAGER.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = cls.plugin.find(BLOCK_START)
        end = cls.plugin.find(BLOCK_END)
        cls.block = cls.plugin[start:end] if 0 <= start < end else ""
        cls.code = strip_comments(cls.block)

    def require_block(self):
        self.assertTrue(self.block, "the angelicprobe research block is missing")

    def row_line(self, row_id):
        rows = self.table()
        lines = [line for line in rows.split("\n") if '{ "%s",' % row_id in line]
        self.assertEqual(len(lines), 1, f"exactly one table row for {row_id}")
        return lines[0]

    def table(self):
        self.require_block()
        start = self.block.index("g_ApRollRows[] = {")
        return self.block[start:self.block.index("};", start)]

    # ---- the verb never reaches a player ------------------------------------

    def test_the_verb_is_research_build_only(self):
        self.require_block()
        self.assertIn("angelicprobe", self.plugin)
        self.assertNotIn("angelicprobe", self.shipped,
                         "the literal (comments included) must disappear "
                         "from the player build")
        self.assertEqual(re.findall(r"\bApRoll\w*", strip_comments(self.shipped)), [])

    def test_the_verb_is_not_a_player_command(self):
        start = self.plugin.index("kPlayerCommands = {")
        self.assertNotIn("angelicprobe", self.plugin[start:self.plugin.index("};", start)])

    def test_the_literal_occurs_once_inside_its_own_handler(self):
        self.assertEqual(self.plugin.count('"angelicprobe"'), 1)
        self.assertEqual(self.plugin.count("static bool HandleAngelicProbeCommand("), 1)
        handler = function_body(self.plugin, "static bool HandleAngelicProbeCommand(")
        self.assertIn('lc == "angelicprobe"', handler)
        shipped = function_body(self.shipped, "static bool HandleAngelicProbeCommand(")
        self.assertIn("return false;", shipped)
        self.assertNotIn("return true", shipped)

    def test_dispatched_on_the_line_after_the_menu_probe(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn(
            "    if (HandleMenuProbeCommand(lc, rest)) return;\n"
            "    if (HandleAngelicProbeCommand(lc, rest)) return;\n",
            run)
        self.assertEqual(run.count("HandleAngelicProbeCommand("), 1)
        self.assertNotIn('"angelicprobe"', run)

    def test_no_new_top_level_else_if_in_run_command(self):
        # C1061: the chain is at MSVC's nesting limit; see test_menu_probe_contract.
        run = strip_comments(function_body(
            self.plugin, "static void RunCommand(const std::string& line)"))
        self.assertEqual(len(re.findall(r"(?m)^\s{4}\}\s*else if \(", run)), 121)

    def test_nothing_reaches_the_frame_callback(self):
        frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertNotIn("angelicprobe", frame)
        self.assertEqual(re.findall(r"\bAp[A-Z]\w*", frame), [],
                         "a probe that runs every frame is a mod, not a probe")

    # ---- every row, by name, on a route that can see direct calls -----------

    def test_the_table_has_exactly_the_candidate_rows(self):
        ids = re.findall(r'\{ "([a-z-]+)",', self.table())
        self.assertEqual(sorted(ids), sorted(CANDIDATES))

    def test_every_row_names_its_script_through_the_sdk(self):
        for row_id, script in CANDIDATES.items():
            with self.subTest(row=row_id):
                self.assertIn(
                    "SdkShortScriptName(HeroSiege::Scripts::gml_Script_%s)" % script,
                    self.row_line(row_id))

    def test_rows_already_held_at_startup_carry_the_existing_hook(self):
        for row_id in DROP_MANAGER_ROWS:
            with self.subTest(row=row_id):
                line = self.row_line(row_id)
                self.assertIn("kApRollHeldByDropManager", line)
                self.assertIn('"Hook_%s"' % CANDIDATES[row_id], line)
        for row_id in TABLE_ONLY_ROWS:
            with self.subTest(row=row_id):
                line = self.row_line(row_id)
                self.assertIn("&g_Orig_%s" % CANDIDATES[row_id], line)
                self.assertIn('"Hook_%s"' % CANDIDATES[row_id], line)
        for row_id in set(CANDIDATES) - set(DROP_MANAGER_ROWS) - set(TABLE_ONLY_ROWS):
            with self.subTest(row=row_id):
                line = self.row_line(row_id)
                self.assertNotIn("Hook_", line)
                self.assertNotIn("kApRollHeldByDropManager", line)

    def test_drop_manager_exposes_the_five_held_originals_to_research_only(self):
        accessor = function_body(self.drop_manager, "ResearchHeldOriginal(")
        for row_id in DROP_MANAGER_ROWS:
            name = CANDIDATES[row_id]
            with self.subTest(name=name):
                self.assertIn('"%s"' % name, accessor)
                self.assertIn("&m_Orig_%s;" % name, accessor)
        shipped = strip_research_blocks(self.drop_manager)
        self.assertNotIn("ResearchHeldOriginal", shipped)
        self.assertEqual(re.findall(r"\bAp[A-Z]\w*", shipped), [])
        self.assertIn("ResearchHeldOriginal(", self.code)

    def test_the_angelic_chance_row_reuses_the_existing_hook(self):
        # The `raredrop angelic` and `angelicwatch` installs stay the only two
        # spelled with the literal pair; the probe reaches the same hook, the
        # same saved original and the same hook id through the SDK name, and
        # asks HookOneScript whether its inline detour went in.
        self.assertEqual(self.plugin.count('"DropItemAngelicChance", "fp_angch"'), 2)
        self.assertRegex(
            self.code,
            r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_DropItemAngelicChance\),"
            r"\s*\"fp_angch\",\s*\(PVOID\)HookAngelicChance,\s*&g_OrigAngChance,\s*&\w+\)")
        self.assertEqual(self.code.count("HookOneScript("), 1,
                         "the angelic-chance row is the only HookOneScript install")
        self.assertIn("InstallHeadhunterHook()", self.code,
                      "the kill control has to be installed for kills= to count")

    def test_no_address_is_resolved_by_hand(self):
        self.require_block()
        for token in ("Rva", "HookOneScriptTable", "GetModuleHandleW"):
            self.assertNotIn(token, self.code)
        self.assertIsNone(re.search(
            r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+|k\w*Rva\w*)", self.code))
        self.assertEqual(self.code.count("MmCreateHook("), 1,
                         "the hooking library is reached from exactly one place")
        self.assertTrue(
            call_is_inside_addr_check_guard(self.code, "MmCreateHook("),
            "every MmCreateHook( must sit inside the braced then-block of "
            "if (AddrIsExecutableInModule(...)) { ... }")

    def test_nothing_calls_drop_item_angelic_or_any_candidate(self):
        self.assertIsNone(re.search(
            r'CallGameScript[A-Za-z]*\(.*"gml_Script_DropItemAngelic"', self.plugin))
        self.require_block()
        self.assertNotIn("CallGameScript", self.code,
                         "the probe counts candidates; it never invokes one")

    def test_a_row_without_a_route_reports_no_count(self):
        show = function_body(self.plugin, "static void ApRollShow(")
        for field in ("calls=n/a", "calls=", "insideDropItem=",
                      "insideAngelicChance=", "kills=", "lastChance=",
                      "lastReturn=", "routeText"):
            self.assertIn(field, show)
        for route in ("TABLE-ONLY (", "detoured (under table-only ", "detoured",
                      " (native)", "blocked: ", "not found ("):
            self.assertIn(route, self.code)

    def test_arguments_are_logged_with_a_bounded_format(self):
        # Known Limitations item 10: %f on a game double can abort the process.
        self.require_block()
        self.assertIsNone(re.search(r"%[-+ #0-9.]*l?[fF]", self.code))
        self.assertIn("%g", self.code)
        self.assertIn("TyInstName", self.code)
        self.assertIn(".substr(0, 60)", self.code)

    def test_the_unique_list_is_read_by_name(self):
        # The listing and the one helper it formats each entry with.
        listing = (function_body(self.plugin, "static void ApRollList(")
                   + function_body(self.plugin, "static std::string ApRollShape("))
        for step in ("asset_get_index", "instance_find", "variable_instance_get",
                     "lootListUnique", "GameObject::Loot_Manager_obj",
                     "array_length", "variable_struct_get_names"):
            self.assertIn(step, listing)
        self.assertTrue("HhResolveInstance" in listing or "HhUsableInstance" in listing)
        self.assertIn("variable_instance_exists", listing,
                      "a missing variable prints one refusal line")

    # ---- attribution: the two depths, compiled away from the player ---------

    def test_drop_hook_invokes_the_probe_scope_once_before_the_original(self):
        body = macro_definition(self.drop_manager, "FP_DROP_HOOK")
        self.assertEqual(body.count("BP_ANGELIC_PROBE_SCOPE("), 1)
        self.assertEqual(self.drop_manager.count("BP_ANGELIC_PROBE_SCOPE"), 1)
        self.assertLess(body.index("BP_ANGELIC_PROBE_SCOPE("),
                        body.index("mgr.m_Orig_##NAME("))

    def test_the_probe_scope_compiles_to_nothing_in_the_player_build(self):
        defs = re.findall(r"(?m)^\s*#\s*define\s+BP_ANGELIC_PROBE_SCOPE\([^)]*\)(.*)$",
                          self.shipped)
        self.assertEqual(len(defs), 1)
        self.assertIn(defs[0].strip(), ("", "((void)0)"))
        research = re.findall(r"(?m)^\s*#\s*define\s+BP_ANGELIC_PROBE_SCOPE\([^)]*\)(.*)$",
                              self.plugin)
        self.assertEqual(len(research), 2)
        self.assertTrue(any("ApRollDropScope" in d for d in research))

    def test_the_angelic_chance_depth_is_research_only(self):
        full = function_body(self.plugin, "static RValue& HookAngelicChance(")
        shipped = function_body(self.shipped, "static RValue& HookAngelicChance(")
        for token in ("++g_ApRollChanceDepth", "--g_ApRollChanceDepth"):
            self.assertIn(token, full)
            self.assertNotIn(token, shipped)
        self.assertNotIn("ApRoll", shipped)
        code = strip_comments(full)
        first_call = code.index("g_OrigAngChance(S")
        last_call = code.rindex("g_OrigAngChance(S")
        self.assertLess(code.index("++g_ApRollChanceDepth"), first_call)
        self.assertGreater(code.index("--g_ApRollChanceDepth"), last_call,
                           "the extra-roll loop stays inside the depth")

    def test_the_gate_is_found_before_drop_manager_hides_drop_item(self):
        full = strip_comments(function_body(self.plugin, "static void InstallHook()"))
        shipped = strip_comments(function_body(self.shipped, "static void InstallHook()"))
        self.assertIn("FindAngelicGate()", full)
        self.assertLess(full.index("FindAngelicGate()"), full.index("InstallDropMultHooks();"))
        self.assertNotIn("FindAngelicGate", shipped)


@unittest.skipUnless(BASE_DIR, "set FORGEPACT_TEST_BASE_DIR to a directory holding "
                               "137a403's ModuleMain.cpp and DropManager.hpp")
class PlayerBuildUnchangedTests(unittest.TestCase):
    """The player build compiles to what it compiled to at 137a403."""

    @classmethod
    def setUpClass(cls):
        base = Path(BASE_DIR)
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.base_plugin = (base / "ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dm = DROP_MANAGER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.base_dm = (base / "DropManager.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_drop_manager_player_build_is_unchanged(self):
        def player(text):
            kept = [line for line in strip_comments(strip_research_blocks(text)).split("\n")
                    if "BP_ANGELIC_PROBE_SCOPE" not in line]
            return normalised("\n".join(kept))
        self.assertEqual(player(self.dm), player(self.base_dm))

    def test_player_build_bodies_are_unchanged(self):
        for signature in ("static RValue& HookAngelicChance(",
                          "static unsigned char* FindAngelicGate()",
                          "static bool OpenAngelicGate()",
                          "static void CloseAngelicGate()",
                          "static void InstallHook()",
                          "static RValue& Hook_EnemyDestroyKillProc("):
            with self.subTest(function=signature):
                now = function_body(strip_research_blocks(self.plugin), signature)
                then = function_body(strip_research_blocks(self.base_plugin), signature)
                self.assertEqual(normalised(now), normalised(then))

    def test_the_probe_moves_no_install(self):
        now = function_body(self.plugin, "static void InstallItemInspectHooks()")
        then = function_body(self.base_plugin, "static void InstallItemInspectHooks()")
        self.assertEqual(normalised(now), normalised(then))
        self.assertEqual(normalised(function_body(self.dm, "void InstallHooks()")),
                         normalised(function_body(self.base_dm, "void InstallHooks()")))


class AngelicRollDocTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = DOC.read_bytes() if DOC.exists() else b""
        cls.doc = cls.raw.decode("utf-8").replace("\r\n", "\n")
        cls.live = LIVE_RECORD.exists()

    def require_doc(self):
        self.assertTrue(self.doc, "docs/angelic-roll-hook-research.md is missing")

    def test_the_document_carries_every_heading_in_order(self):
        self.require_doc()
        positions = [self.doc.find("\n" + h + "\n") for h in DOC_HEADINGS]
        self.assertTrue(all(p >= 0 for p in positions), positions)
        self.assertEqual(positions, sorted(positions))

    def test_the_document_is_crlf(self):
        self.require_doc()
        self.assertGreater(self.raw.count(b"\r\n"), 0)
        self.assertEqual(self.raw.count(b"\n") - self.raw.count(b"\r\n"), 0)

    def test_the_static_search_names_every_candidate_and_every_rejection(self):
        self.require_doc()
        search = section(self.doc, "## Static search")
        missing = [n for n in list(CANDIDATES.values()) + ["EnemyDestroyKillProc"]
                   if n not in search]
        self.assertEqual(missing, [])
        missing = [n for n in REJECTED_NAMES if n not in search]
        self.assertEqual(missing, [], "a name the search found and set aside "
                                      "has to say why, or it is found again")

    def test_every_candidate_has_a_row(self):
        self.require_doc()
        candidates = section(self.doc, "## Candidates and controls")
        for row_id in CANDIDATES:
            self.assertIn("| `%s` |" % row_id, candidates)
        self.assertIn("positive control", candidates)

    def test_every_live_step_has_a_results_row(self):
        self.require_doc()
        results = section(self.doc, "## Results")
        for step in range(1, 14):
            rows = [line for line in results.split("\n")
                    if line.startswith("| L-1.%d |" % step)]
            self.assertEqual(len(rows), 1, "L-1.%d" % step)
            if self.live:
                self.assertNotIn("pending", rows[0],
                                 "the session has run; L-1.%d needs its reply" % step)

    def _label(self, row_id):
        decision = section(self.doc, "## Decision")
        match = re.search(r"(?ms)^\* `%s` - \*\*(.+?)\*\*" % re.escape(row_id), decision)
        self.assertIsNotNone(match, f"{row_id} needs a line ``* `{row_id}` - **<verdict>**``")
        return " ".join(match.group(1).split()).rstrip(".")

    def test_every_candidate_carries_one_labelled_decision_line(self):
        self.require_doc()
        decision = section(self.doc, "## Decision")
        bullets = [line for line in decision.split("\n") if line.startswith("* `")]
        self.assertEqual(len(bullets), len(CANDIDATES))
        for row_id in CANDIDATES:
            label = self._label(row_id)
            allowed = VERDICTS if self.live else VERDICTS + ("pending",)
            self.assertTrue(any(label.startswith(v) for v in allowed),
                            f"{row_id} is labelled {label!r}")

    def test_the_finding_names_only_candidates_that_worked(self):
        self.require_doc()
        findings = re.findall(r"(?m)^finding: (.+)$", self.doc)
        self.assertEqual(len(findings), 1)
        finding = findings[0].strip()
        if finding == "pending":
            self.assertFalse(self.live, "the session has run; `pending` is no "
                                        "longer an answer")
            return
        if finding == "none":
            return
        for token in (t.strip() for t in finding.split(",")):
            self.assertIn(token, CANDIDATES)
            self.assertTrue(self._label(token).startswith("works"),
                            f"{token} is named in finding: but not labelled works")

    def test_pending_only_before_the_session(self):
        self.require_doc()
        if self.live:
            self.assertNotIn("pending", self.doc)

    def test_the_follow_up_needs_are_listed(self):
        self.require_doc()
        self.assertIn("### What the follow-up needs", self.doc)


if __name__ == "__main__":
    unittest.main()
