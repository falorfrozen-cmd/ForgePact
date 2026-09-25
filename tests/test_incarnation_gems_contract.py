"""Contract tests for Gems of Incarnation (docs/incarnation-gems-research.md).

The decisions are pinned by test_incarnation_gems_behavior.py against the real
core. These pin the wiring around it, so it cannot drift silently:
  - the only hook is DropGems, by SDK constant, native or not at all, and it
    waits for a player (a drop hook installed at character select stalls the
    runner, the relic filter's measured rule);
  - the seed swap runs before the outermost CreateItemNew and only inside the
    drop scope; the dress runs on the finished item before Item Truth records
    it; the tables' own candidates skip everything else in the hook;
  - the player build accepts the two switches and the filter, the research
    verbs stay out;
  - the panel's two switches: defaults, launch commands, click commands, HTML;
  - the panel's mod filter: the list is the measured pool, what it saves, what
    it sends and when, and the page that draws it.
"""
import copy
import json
import re
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / 'plugin/ModuleMain.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'plugin/include/ForgePact/IncarnationGemsMod.hpp').read_text(encoding='utf-8')
sys.path.insert(0, str(ROOT / 'src'))
import forgepact  # noqa: E402


def between(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]


DOC = (ROOT / 'docs/incarnation-gems-research.md').read_text(encoding='utf-8')
PANEL = (ROOT / 'src/forgepact.py').read_text(encoding='utf-8-sig')


def doc_affixes():
    """The research doc's affix table: stat -> (label, category, best range)."""
    table = between(DOC, '| Stat | Mod | Category |', '\n\n')
    rows = {}
    for line in table.splitlines()[2:]:
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        low, high = cells[3].split('-')
        rows[int(cells[0])] = (cells[1].replace('`', ''), cells[2], (int(low), int(high)))
    return rows


class CoreStaysGameIndependentTests(unittest.TestCase):
    def test_core_names_no_runtime_interface(self):
        for name in ('RValue', 'CInstance', 'g_Yytk', 'YYTK', 'HookOneScript', 'Out('):
            self.assertNotIn(name, HEADER, name)

    def test_core_leaves_the_windows_min_max_macros_alone(self):
        # ModuleMain includes <windows.h> without NOMINMAX: std::max( breaks
        # the plugin build while the harness still compiles.
        self.assertNotRegex(HEADER, r'std::(max|min)\(')

    def test_identifier_stats_are_never_maxed(self):
        self.assertIn('stat == 462 || stat == 21', HEADER)


class PluginWiringTests(unittest.TestCase):
    def test_header_included(self):
        self.assertIn('#include <ForgePact/IncarnationGemsMod.hpp>', MAIN)

    def test_only_drop_gems_is_hooked_natively_by_sdk_constant(self):
        install = between(MAIN, 'static bool GemsInstallDropHook()', '\n}\n')
        self.assertIn('SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropGems)', install)
        self.assertIn('&g_GemDropHookNative', install)
        self.assertEqual(1, MAIN.count('"fp_gem_drop"'))
        glue = between(MAIN, '// ===== Gems of Incarnation', '// ===== Headhunter mechanic')
        self.assertEqual(1, glue.count('HookOneScript('), 'the mod hooks one script')

    def test_drop_hook_waits_for_a_player(self):
        command = between(MAIN, 'static void GemsCommand(', '\n}\n')
        self.assertNotIn('GemsInstallDropHook()', command, 'the command only arms the hook')
        tick = between(MAIN, 'static void GemsTick(uint32_t frame)', '\n}\n')
        self.assertRegex(tick, r'g_GemDropPending && g_GemInPlay\)[\s\S]{0,80}GemsInstallDropHook\(\)')

    def test_swap_runs_before_the_outermost_create_and_only_in_the_drop_scope(self):
        macro = between(MAIN, '#define ITEM_CREATE_HOOK(NAME)', 'ITEM_CREATE_HOOK(CreateItemNew)')
        swap = macro.index('GemsBeforeCreate(argc, A)')
        self.assertLess(swap, macro.index('g_Orig_##NAME(S, O, R, argc, A)'))
        self.assertIn('if (_final && g_TruthDepth == 0) GemsBeforeCreate(argc, A);', macro)
        before = between(MAIN, 'static void GemsBeforeCreate(', '\n}\n')
        self.assertIn('g_GemDropDepth <= 0', before)
        self.assertIn('g_GemTableBuilding', before)
        self.assertIn('DropSeed(g_Gems, g_GemTables, true,', before)

    def test_dress_runs_on_the_finished_item_before_item_truth_records_it(self):
        macro = between(MAIN, '#define ITEM_CREATE_HOOK(NAME)', 'ITEM_CREATE_HOOK(CreateItemNew)')
        dress = macro.index('if (_final) GemsAfterCreate(_res);')
        self.assertLess(macro.index('CustomForgePostProcess(_res, argc, A, _final);'), dress)
        self.assertLess(dress, macro.index('if (_outermost) ItemTruthCapture(_res, _native);'))
        after = between(MAIN, 'static void GemsAfterCreate(', '\n}\n')
        self.assertIn('RefreshItemHash(item);', after, 'a changed item carries a fresh hash')

    def test_table_candidates_skip_every_other_post_process(self):
        macro = between(MAIN, '#define ITEM_CREATE_HOOK(NAME)', 'ITEM_CREATE_HOOK(CreateItemNew)')
        skip = macro.index('if (g_GemTableBuilding) return _res;')
        self.assertLess(skip, macro.index('CustomForgePostProcess('))
        self.assertLess(skip, macro.index('ItemTruthCapture('))

    def test_tables_build_a_little_each_frame(self):
        self.assertIn('    if (g_Setup) GemsTick(fc);', MAIN)
        tick = between(MAIN, 'static void GemsTick(uint32_t frame)', '\n}\n')
        self.assertIn('if (!g_Gems.mythic && !g_Gems.maxRoll) return;', tick)
        self.assertIn('>= budget) break;', tick)
        self.assertIn('kGemBuildBudgetMenu = 0.004, kGemBuildBudgetPlay = 0.001', MAIN)

    def test_player_build_takes_the_two_switches_only(self):
        players = between(MAIN, 'static const std::unordered_set<std::string> kPlayerCommands = {', '};')
        self.assertIn('"gemmythic", "gemmaxroll", "gemfilter"', players)
        self.assertNotIn('"gems"', players)
        self.assertIn('if (lc == "gemmythic" || lc == "gemmaxroll" || lc == "gemfilter" || lc == "gems") { GemsCommand(lc, rest); return; }', MAIN)
        command = between(MAIN, 'static void GemsCommand(', '\n}\n')
        research = command[command.index('#ifndef FORGEPACT_RELEASE'):]
        self.assertIn('drop', research)
        self.assertIn('#else', research)

    def test_convert_is_research_build_only(self):
        # `gems convert` rewrites a real drop's definition: a test tool for the
        # research build, never in the player build.
        before = between(MAIN, 'static void GemsBeforeCreate(', '\n}\n')
        convert = before[before.index('#ifndef FORGEPACT_RELEASE'):before.index('#endif')]
        self.assertIn('g_GemConvert &&', convert)
        self.assertIn('variable_struct_set", { def, RValue("b")', convert)
        self.assertEqual(1, before.count('variable_struct_set", { def, RValue("b")'))
        declared = MAIN[MAIN.index('static bool g_GemConvert = false;') - 400:MAIN.index('static bool g_GemConvert = false;')]
        self.assertIn('#ifndef FORGEPACT_RELEASE', declared)
        command = between(MAIN, 'static void GemsCommand(', '\n}\n')
        research = command[command.index('#ifndef FORGEPACT_RELEASE'):command.index('#else')]
        self.assertIn('arg == "convert 1"', research)

    def test_mod_state_reports_the_gems(self):
        self.assertIn('body += GemsModState() + "}";', MAIN)
        state = between(MAIN, 'static std::string GemsModState()', '\n}\n')
        for field in ('mythic', 'maxRoll', 'dropHook', 'mythicDrops', 'vanillaDrops', 'filter', 'filterMisses',
                      'seeds', 'bestRanges', 'ready', 'error'):
            self.assertIn('\\"' + field + '\\"', state, field)


class PanelTests(unittest.TestCase):
    def test_defaults_on_until_the_owner_decides(self):
        self.assertIs(True, forgepact.DEFAULTS['mod_gem_mythic'])
        self.assertIs(True, forgepact.DEFAULTS['mod_gem_maxroll'])

    def test_launch_sends_both_switches_when_on(self):
        commands = forgepact.build_cmds(copy.deepcopy(forgepact.DEFAULTS))
        self.assertIn('gemmythic 1', commands)
        self.assertIn('gemmaxroll 1', commands)

    def test_launch_sends_nothing_when_off(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg['mod_gem_mythic'] = cfg['mod_gem_maxroll'] = False
        commands = forgepact.build_cmds(cfg)
        self.assertFalse(any(c.startswith(('gemmythic', 'gemmaxroll')) for c in commands))

    def test_a_click_sends_its_command(self):
        source = (ROOT / 'src/forgepact.py').read_text(encoding='utf-8-sig')
        self.assertIn('''cmds = [f"gemmythic {1 if cfg['mod_gem_mythic'] else 0}"]''', source)
        self.assertIn('''send_cmds([f"gemmaxroll {1 if cfg['mod_gem_maxroll'] else 0}"], cfg)''', source)
        self.assertRegex(source, r'"mod_craft_mats", "mod_gem_mythic", "mod_gem_maxroll"\):')

    def test_switches_on_the_mods_tab(self):
        source = (ROOT / 'src/forgepact.py').read_text(encoding='utf-8-sig')
        for element in ('id="mod_gem_mythic"', 'id="mod_gem_maxroll"', 'id="mgmval"', 'id="mgrval"',
                        "mod_gem_mythic:'mod_gem_mythic'", "mgmval:'mod_gem_mythic'", "mgrval:'mod_gem_maxroll'"):
            self.assertEqual(1, source.count(element), element)
        self.assertIn('Mythic Gems of Incarnation', source)
        self.assertIn('Max-roll Gems of Incarnation', source)



class PanelSandbox:
    """The real panel server on a temporary settings file; game IPC is a mock."""

    def __init__(self, running):
        self.temp = tempfile.TemporaryDirectory(prefix='forgepact-gems-')
        self.config = Path(self.temp.name) / 'forgepact.json'
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg['game_exe'] = str(Path(self.temp.name) / 'Hero_Siege.exe')
        cfg['auto_apply'] = False
        self.config.write_text(json.dumps(cfg), encoding='utf-8')
        self.patches = [patch.object(forgepact, 'CONFIG', self.config),
                        patch.object(forgepact, 'game_running', return_value=running),
                        patch.object(forgepact, 'mod_chain', return_value={}),
                        patch.object(forgepact, 'send_cmds')]
        self.mocks = [p.start() for p in self.patches]
        self.sent = self.mocks[3]
        self.server = forgepact.ThreadingHTTPServer(('127.0.0.1', 0), forgepact.H)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        for p in reversed(self.patches):
            p.stop()
        self.temp.cleanup()

    def request(self, body=None):
        connection = HTTPConnection('127.0.0.1', self.server.server_port, timeout=5)
        try:
            if body is None:
                connection.request('GET', '/api/state')
            else:
                connection.request('POST', '/api/set', json.dumps(body), {'Content-Type': 'application/json'})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def saved(self):
        return json.loads(self.config.read_text(encoding='utf-8'))


class GemFilterPanelTests(unittest.TestCase):
    def sandbox(self, running=True):
        box = PanelSandbox(running)
        self.addCleanup(box.close)
        return box

    def test_the_list_is_the_measured_pool(self):
        # Every mod the game rolled on 14,521 gems, each once, worded and
        # grouped as the research doc's table has it; a skill grant is one row.
        rows = doc_affixes()
        self.assertEqual(36, len(forgepact.GEM_AFFIXES))
        self.assertEqual(len(forgepact.GEM_AFFIXES), len(forgepact.GEM_AFFIX_IDS))
        self.assertEqual(set(rows), set(forgepact.GEM_AFFIX_IDS))
        self.assertNotIn(463, forgepact.GEM_AFFIX_IDS)
        for stat, category, label in forgepact.GEM_AFFIXES:
            self.assertEqual((label, category), rows[stat][:2], stat)
            self.assertIn(category, forgepact.GEM_CATEGORIES, stat)
        self.assertEqual(set(forgepact.GEM_CATEGORIES), {c for _s, c, _l in forgepact.GEM_AFFIXES})
        self.assertIn('if (out.size() > 64) return std::nullopt;', HEADER)
        self.assertLessEqual(len(forgepact.GEM_AFFIX_IDS), 64)

    def test_a_filter_is_all_or_known_mods(self):
        value = forgepact.gem_filter_value
        self.assertEqual('all', value('all'))
        self.assertEqual([68, 284], value([284, 68, 68]))
        self.assertEqual('all', value(sorted(forgepact.GEM_AFFIX_IDS)))
        for junk in ([], [999], [463], [True], [68.5], '68', None, {'68': True}):
            self.assertIsNone(value(junk), junk)

    def test_launch_sends_a_narrowed_filter_right_after_mythic(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        self.assertEqual('all', cfg['gem_filter'])
        self.assertFalse(any(c.startswith('gemfilter') for c in forgepact.build_cmds(cfg)))
        cfg['gem_filter'] = [68, 284]
        commands = forgepact.build_cmds(cfg)
        self.assertEqual('gemfilter 68,284', commands[commands.index('gemmythic 1') + 1])
        cfg['mod_gem_mythic'] = False
        self.assertFalse(any(c.startswith('gemfilter') for c in forgepact.build_cmds(cfg)))

    def test_saving_a_filter_tells_the_running_game(self):
        box = self.sandbox()
        code, result = box.request({'key': 'gem_filter', 'value': [284, 68]})
        self.assertEqual(200, code, result)
        self.assertEqual([68, 284], box.saved()['gem_filter'])
        box.sent.assert_called_once()
        self.assertEqual(['gemfilter 68,284'], box.sent.call_args.args[0])
        box.sent.reset_mock()
        code, result = box.request({'key': 'gem_filter', 'value': 'all'})
        self.assertEqual(200, code, result)
        self.assertEqual(['gemfilter all'], box.sent.call_args.args[0])

    def test_nothing_ticked_is_refused_and_keeps_the_filter(self):
        box = self.sandbox()
        box.request({'key': 'gem_filter', 'value': [68]})
        box.sent.reset_mock()
        for junk in ([], [999], 'nonsense'):
            code, result = box.request({'key': 'gem_filter', 'value': junk})
            self.assertEqual(400, code, junk)
            self.assertIn('err', result)
        self.assertEqual([68], box.saved()['gem_filter'])
        box.sent.assert_not_called()

    def test_turning_mythic_on_restates_the_filter(self):
        box = self.sandbox()
        box.request({'key': 'gem_filter', 'value': [201, 462]})
        box.sent.reset_mock()
        box.request({'key': 'mod_gem_mythic', 'value': True})
        self.assertEqual(['gemmythic 1', 'gemfilter 201,462'], box.sent.call_args.args[0])
        box.request({'key': 'mod_gem_mythic', 'value': False})
        self.assertEqual(['gemmythic 0'], box.sent.call_args.args[0])

    def test_state_gives_the_page_its_list(self):
        box = self.sandbox(running=False)
        code, state = box.request()
        self.assertEqual(200, code)
        self.assertEqual([[s, c, l] for s, c, l in forgepact.GEM_AFFIXES], state['gemAffixes'])
        self.assertEqual(list(forgepact.GEM_CATEGORIES), state['gemCategories'])
        self.assertEqual('all', state['cfg']['gem_filter'])

    def test_filter_row_under_the_switches(self):
        for element in ('id="gemfilter_row"', 'id="gemfilter_toggle"', 'id="gemfilter_summary"',
                        'id="gemfilter_panel"', 'Filter&hellip;', "key:'gem_filter'"):
            self.assertEqual(1, PANEL.count(element), element)
        # The row sits right under the Max-roll switch, and the Mods tab keeps
        # the switches, the row and its list together as one card (its columns
        # move rows, never a bare list).
        self.assertLess(PANEL.index('id="mod_gem_maxroll"'), PANEL.index('id="gemfilter_row"'))
        self.assertLess(PANEL.index('id="gemfilter_row"'), PANEL.index('id="gemfilter_panel"'))
        self.assertEqual(1, PANEL.count('id="mod_gem_maxroll_row"'))
        self.assertIn("gemGroup.append(gemParent,document.getElementById('mod_gem_maxroll_row'),"
                      "document.getElementById('gemfilter_row'),document.getElementById('gemfilter_panel'));", PANEL)
        self.assertIn(".feature-with-child>#gemfilter_row{", PANEL)
        # The page draws from /api/state, never a list of its own.
        self.assertIn('ST.gemAffixes', PANEL)
        self.assertIn('ST.gemCategories', PANEL)


if __name__ == '__main__':
    unittest.main()
