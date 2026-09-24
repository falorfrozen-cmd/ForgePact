#!/usr/bin/env python3
"""Contract tests for the Mods tab's card split (ForgePact issue #12).

Reads `forgepact.py` and `panel_icons.py` as text from
`FORGEPACT_TEST_PANEL_DIR` (default: `src/`), never by import, so the same
tests run against an older copy of those two files with no `hs_game_sdk` on
the path - the "prove the instrument" precedent set by
`FORGEPACT_TEST_PLUGIN_SOURCE` (`test_signature_drop_contract.py`,
`test_headhunter_dispatch.py`). `forgepact.py` is UTF-8 with a BOM, so both
files are read with `utf-8-sig`.

`ModsCategoryBaselineTests` pins what must survive the split: the five-tab
sidebar, every control's card membership, the Items/Quality-of-Life
classification rule (an Items row's description names a forged Mechanic; no
other Mods-tab row's does), the parent/child groupings, and the three silent
failure traps in the JS `preparePanelUI()` (the conversion loop's id list,
the grouping condition's id, and the `SECTION_ICONS` key). It names no card
id except `itemsCard`, because the other card's id changes - so the same
assertions pass on the pre-change panel and on the result.

`ModsCategorySplitTests` pins the result: two Mods-tab cards named
`qolCard`/`itemsCard`, in that order, `qolCard` titled "Quality of Life" and
holding exactly the ten Quality of Life controls in the assignment table's
order, and no remaining "gameplay" wording or `gameplayCard` id anywhere in
either source file.

`ModsSubtabMarkupTests` and `ModsSubtabBehaviourTests` (round 1, the same
issue's follow-up) pin the Quality of Life | Items sub-tab strip added on top
of the split above: the strip's and buttons' markup, each card's `role`/
`aria-labelledby`, the `.subtabbar`/`.subtabbtn` CSS, and that `boot`/
`preparePanelUI` wire the sub-tabs up. `ModsSubtabBehaviourTests` runs the
page's real `openTab`, `openModsSubtab`, `bindModsSubtabs`, `PAGE_INFO` and
state declaration through `node` against a small stub DOM built in this file
(never by importing `forgepact`, which would pull `hs_game_sdk` onto the path
and defeat running against an older fixture copy) - the same "prove the
instrument" precedent as `test_mods_columns.py`'s `setupModsColumns` harness,
with its own local `node` runner so this module still imports nothing that
imports `forgepact`.
"""
import json
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
PANEL_DIR = pathlib.Path(os.environ.get("FORGEPACT_TEST_PANEL_DIR", str(ROOT / "src")))
HTML = (PANEL_DIR / "forgepact.py").read_text(encoding="utf-8-sig")
ICONS_SOURCE = (PANEL_DIR / "panel_icons.py").read_text(encoding="utf-8-sig")

# The assignment table (context "### Classification rule and assignment"),
# in the order the rows render.
QOL_CONTROL_IDS = [
    "mod_filter_max_relics",
    "mod_orb_pickup_radius",
    "map_reveal",
    "map_reveal_packs",
    "mod_pet_quest_pickup",
    "mod_auto_prospect",
    "mod_auto_prospect_bag",
    "mod_toggle_indicator",
    "mod_toggle_guard",
    "mod_skill_timer_style",
]
ITEMS_CONTROL_IDS = ["headhunter", "tyrant", "beacon"]
ALL_CONTROL_IDS = QOL_CONTROL_IDS + ITEMS_CONTROL_IDS

# (parent control id, child row id) pairs; the child shares its parent's card.
PARENT_CHILD_ROWS = [
    ("map_reveal", "map_reveal_packs_row"),
    ("mod_auto_prospect", "mod_auto_prospect_bag_row"),
]

FORGED_MECHANIC_TEXT = "forged with Mechanic:"
HINT_SUFFIX = "Settings apply immediately while the game is running."

_CARD_OPEN_RE = re.compile(r'<div class="card')
# Extra attributes (round 1: role/aria-labelledby for the sub-tab strip) may
# follow the id before the tag closes.
_MODS_CARD_RE = re.compile(r'<div class="card tab-card" data-tab="mods" id="([^"]+)"[^>]*>')


def _mods_cards(html):
    """[(card_id, body)] for every Mods-tab card, in document order.

    A card's body is the text from its own opening tag up to (but not
    including) the next card-open tag anywhere in the document, or the end
    of the file for the last card. Cards nest rows, never other cards, so
    this boundary never crosses into another card's rows. Never slice
    between two card ids by name - a stale phrase earlier in the markup (a
    hint, a heading) would move the slice to the wrong place.
    """
    card_starts = [m.start() for m in _CARD_OPEN_RE.finditer(html)]
    cards = []
    for match in _MODS_CARD_RE.finditer(html):
        start = match.start()
        later = [s for s in card_starts if s > start]
        end = later[0] if later else len(html)
        cards.append((match.group(1), html[start:end]))
    return cards


def _card_by_id(cards, card_id):
    """The body of the Mods-tab card with this id.

    A missing card fails the assertion the caller is making, rather than
    raising KeyError/ValueError - so a card that does not exist yet shows up
    as a test failure (`FAILED (failures=`), not an error.
    """
    for cid, body in cards:
        if cid == card_id:
            return body
    raise AssertionError(f"no Mods-tab card id={card_id!r}; found {[c for c, _ in cards]!r}")


def _sidebar_tabs(html):
    nav_start = html.index('<nav class="tabbar"')
    nav_end = html.index("</nav>", nav_start)
    return re.findall(r'data-tab="([a-z]+)"', html[nav_start:nav_end])


_MODS_CONVERSION_LOOP_HEADER_RE = re.compile(r"for\(const id of \[([^\]]*)\]\)\{")


def _mods_conversion_loop(html):
    """(id_list_text, body) for the `for(const id of [...]){...}` block that
    builds each card's mods-grid - unique in the file: the other id-list loop
    (`for(const id of ['spawners','dropSettings'])...`) has no `{` block, and
    the other brace loop that shares the exact line
    `const card=document.getElementById(id);` (the `summaries` loop) uses
    destructuring (`for(const [id,summary] of ...)`), which this header does
    not match. The body is found by brace-counting from the header's own `{`,
    not by a second regex, so a nested `{...}` (the `if(id===...)` grouping
    block) cannot end the match early.
    """
    header = _MODS_CONVERSION_LOOP_HEADER_RE.search(html)
    if not header:
        return None, None
    brace = header.end() - 1
    depth = 0
    for index in range(brace, len(html)):
        if html[index] == "{":
            depth += 1
        elif html[index] == "}":
            depth -= 1
            if depth == 0:
                return header.group(1), html[brace:index + 1]
    return header.group(1), None


def _conversion_loop_ids(html):
    """The id list `preparePanelUI` loops over to build each card's mods-grid."""
    id_list_text, _ = _mods_conversion_loop(html)
    if id_list_text is None:
        return None
    return re.findall(r"'([^']+)'", id_list_text)


def _grouping_condition_id(html):
    """The card id the parent/child `.feature-with-child` grouping is gated on."""
    _, body = _mods_conversion_loop(html)
    if not body:
        return None
    match = re.search(r"if\(id===\'([^\']+)\'\)\{", body)
    return match.group(1) if match else None


def _section_icons(icons_source):
    """{key: icon} from the `SECTION_ICONS` dict literal, parsed as text."""
    match = re.search(r"SECTION_ICONS\s*=\s*\{(.*?)\n\}", icons_source, re.S)
    assert match is not None, "SECTION_ICONS dict not found"
    return dict(re.findall(r"'([A-Za-z0-9_]+)'\s*:\s*'([a-z0-9-]+)'", match.group(1)))


class ModsCategoryBaselineTests(unittest.TestCase):
    """Pins what must survive the split. Names no card id except itemsCard."""

    def test_sidebar_is_the_five_tabs_in_order(self):
        self.assertEqual(
            _sidebar_tabs(HTML),
            ["setup", "modifiers", "world", "loot", "mods"],
        )

    def test_every_assignment_table_control_appears_once_in_a_mods_card(self):
        cards = _mods_cards(HTML)
        for control_id in ALL_CONTROL_IDS:
            needle = f'id="{control_id}"'
            self.assertEqual(
                HTML.count(needle), 1,
                f"{needle} should appear exactly once in the panel",
            )
            self.assertTrue(
                any(needle in body for _, body in cards),
                f"{needle} is not inside any data-tab=\"mods\" card",
            )

    def test_items_card_holds_exactly_headhunter_tyrant_beacon(self):
        cards = _mods_cards(HTML)
        items_body = _card_by_id(cards, "itemsCard")
        for control_id in ITEMS_CONTROL_IDS:
            self.assertIn(f'id="{control_id}"', items_body)
        for cid, body in cards:
            if cid == "itemsCard":
                continue
            for control_id in ITEMS_CONTROL_IDS:
                self.assertNotIn(f'id="{control_id}"', body,
                    f"{control_id} unexpectedly found in {cid!r}")

    def test_classification_rule_forged_mechanic_text(self):
        # The rule, made executable: a row whose description names a forged
        # Mechanic is an Items row, and only Items rows name one.
        cards = _mods_cards(HTML)
        for cid, body in cards:
            count = body.count(FORGED_MECHANIC_TEXT)
            if cid == "itemsCard":
                self.assertEqual(count, len(ITEMS_CONTROL_IDS),
                    "itemsCard should have one forged-Mechanic row per control")
            else:
                self.assertEqual(count, 0,
                    f"{cid} has a row naming a forged Mechanic; it belongs in itemsCard")

    def test_child_rows_share_their_parents_card(self):
        cards = _mods_cards(HTML)
        for parent_id, child_row_id in PARENT_CHILD_ROWS:
            parent_card = next(
                (cid for cid, body in cards if f'id="{parent_id}"' in body), None)
            child_card = next(
                (cid for cid, body in cards if f'id="{child_row_id}"' in body), None)
            self.assertIsNotNone(parent_card, f"{parent_id} not in any Mods-tab card")
            self.assertIsNotNone(child_card, f"{child_row_id} not in any Mods-tab card")
            self.assertEqual(
                parent_card, child_card,
                f"{parent_id} is in {parent_card!r} but {child_row_id} is in {child_card!r}",
            )

    def test_conversion_loop_ids_equal_the_mods_card_ids(self):
        loop_ids = _conversion_loop_ids(HTML)
        self.assertIsNotNone(loop_ids, "preparePanelUI's mods-grid conversion loop not found")
        card_ids = [cid for cid, _ in _mods_cards(HTML)]
        self.assertEqual(sorted(loop_ids), sorted(card_ids))

    def test_grouping_condition_id_is_the_card_holding_both_parents(self):
        grouping_id = _grouping_condition_id(HTML)
        self.assertIsNotNone(grouping_id, "parent/child grouping condition not found")
        cards = _mods_cards(HTML)
        card_with_parents = next(
            (cid for cid, body in cards
             if 'id="map_reveal"' in body and 'id="mod_auto_prospect"' in body),
            None,
        )
        self.assertIsNotNone(card_with_parents, "no card holds both map_reveal and mod_auto_prospect")
        self.assertEqual(grouping_id, card_with_parents)

    def test_every_mods_card_has_a_section_icon_and_every_icon_key_is_a_real_id(self):
        cards = _mods_cards(HTML)
        section_icons = _section_icons(ICONS_SOURCE)
        for cid, _ in cards:
            self.assertIn(cid, section_icons, f"{cid} has no SECTION_ICONS entry")
        for key in section_icons:
            self.assertGreater(
                HTML.count(f'id="{key}"'), 0,
                f"SECTION_ICONS key {key!r} is not an element id in the panel",
            )

    def test_every_mods_card_hint_ends_with_the_standard_sentence(self):
        for cid, body in _mods_cards(HTML):
            match = re.search(r'<div class="hint">(.*?)</div>', body)
            self.assertIsNotNone(match, f"{cid} has no hint")
            self.assertTrue(
                match.group(1).rstrip().endswith(HINT_SUFFIX),
                f"{cid}'s hint does not end with the standard sentence",
            )

    def test_data_tab_mods_occurs_exactly_three_times(self):
        # The sidebar button plus the two Mods-tab cards - never a third.
        self.assertEqual(HTML.count('data-tab="mods"'), 3)

    def test_open_tab_toolbar_list_is_loot_and_modifiers(self):
        match = re.search(r"controlToolbar'\)\.hidden=!\[([^\]]*)\]\.includes\(name\)", HTML)
        self.assertIsNotNone(match, "openTab's toolbar visibility list not found")
        names = re.findall(r"'([^']+)'", match.group(1))
        self.assertEqual(names, ["loot", "modifiers"])

    def test_open_tab_writes_forgepact_tab(self):
        self.assertIn("sessionStorage.setItem('forgepact_tab',name)", HTML)


class ModsCategorySplitTests(unittest.TestCase):
    """Pins the result of the split."""

    def test_mods_card_ids_in_order(self):
        self.assertEqual(
            [cid for cid, _ in _mods_cards(HTML)],
            ["qolCard", "itemsCard"],
        )

    def test_qol_card_heading_is_quality_of_life(self):
        body = _card_by_id(_mods_cards(HTML), "qolCard")
        self.assertIn("<h2>Quality of Life</h2>", body)

    def test_qol_card_controls_are_exactly_the_ten_qol_ids_in_order(self):
        body = _card_by_id(_mods_cards(HTML), "qolCard")
        positions = []
        for control_id in QOL_CONTROL_IDS:
            pos = body.find(f'id="{control_id}"')
            self.assertGreaterEqual(pos, 0, f"{control_id} missing from qolCard")
            positions.append((control_id, pos))
        ordered = [cid for cid, _ in sorted(positions, key=lambda pair: pair[1])]
        self.assertEqual(ordered, QOL_CONTROL_IDS)
        for control_id in ITEMS_CONTROL_IDS:
            self.assertNotIn(f'id="{control_id}"', body)

    def test_no_gameplay_wording_remains(self):
        for cid, body in _mods_cards(HTML):
            self.assertNotIn("gameplay", body.lower(), f"{cid} still says 'gameplay'")
        page_info_match = re.search(r"mods:\[([^\]]*)\]", HTML)
        self.assertIsNotNone(page_info_match, "PAGE_INFO.mods not found")
        self.assertNotIn("gameplay", page_info_match.group(1).lower())

    def test_gameplay_card_id_is_gone(self):
        self.assertNotIn("gameplayCard", HTML)
        self.assertNotIn("gameplayCard", ICONS_SOURCE)


def _brace_block(html, start_marker):
    """The text from `start_marker` (which must itself end with `{`) to its
    own balanced closing `}`.

    Raises AssertionError - a test failure, not a setUp/collection error -
    rather than KeyError/IndexError when the marker is missing, so a piece
    that does not exist yet on the pre-change panel shows up as
    `FAILED (failures=`, never `errors=`.
    """
    start = html.find(start_marker)
    if start < 0:
        raise AssertionError(f"{start_marker!r} not found in the panel page")
    brace = start + len(start_marker) - 1
    if html[brace] != "{":
        raise AssertionError(f"{start_marker!r} does not end with its own opening brace")
    depth = 0
    for index in range(brace, len(html)):
        if html[index] == "{":
            depth += 1
        elif html[index] == "}":
            depth -= 1
            if depth == 0:
                return html[start:index + 1]
    raise AssertionError(f"no matching closing brace for {start_marker!r}")


def _page_info_source(html):
    start = html.find("const PAGE_INFO={")
    if start < 0:
        raise AssertionError("PAGE_INFO not found in the panel page")
    end = html.find("};", start)
    if end < 0:
        raise AssertionError("PAGE_INFO has no closing '};'")
    return html[start:end + 2]


def _mods_state_declaration_source(html):
    match = re.search(r"let activeTab=[^;]*;", html)
    if not match:
        raise AssertionError("the activeTab/controlFilter/modsSubtab state declaration was not found")
    return match.group(0)


def _run_node(policy_js, driver_js):
    """Execute the extracted panel code through a real JS runtime, or skip.

    A local copy of `test_panel_performance.run_node` rather than an import
    of it: that module imports `forgepact`, which this file must not do, so
    it keeps running against an older fixture copy with no `hs_game_sdk` on
    the path.
    """
    node = shutil.which("node")
    if not node:
        raise unittest.SkipTest(
            "node is not on PATH; the sub-tab behaviour classes need a "
            "JavaScript runtime to execute")
    with tempfile.TemporaryDirectory() as tmp:
        script = pathlib.Path(tmp) / "policy.js"
        script.write_text(policy_js + "\n" + driver_js, encoding="utf-8")
        result = subprocess.run([node, str(script)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return json.loads(result.stdout.strip().splitlines()[-1])


_SUBTAB_STRIP_RE = re.compile(
    r'<div id="modsSubtabs" class="subtabbar" role="tablist" '
    r'aria-label="Mods categories" hidden>(.*?)</div>',
    re.S,
)
_SUBTAB_BUTTON_RE = re.compile(
    r'<button type="button" class="subtabbtn" role="tab"([^>]*)>([^<]*)</button>'
)


class ModsSubtabMarkupTests(unittest.TestCase):
    """Pins the Quality of Life | Items sub-tab strip's markup (round 1)."""

    def test_strip_is_first_child_of_workspace_before_any_card(self):
        workspace_start = HTML.index('<div id="workspace"')
        strip_match = _SUBTAB_STRIP_RE.search(HTML)
        self.assertIsNotNone(strip_match, "#modsSubtabs strip not found")
        first_card_start = HTML.index('<div class="card tab-card" data-tab="setup" id="setupCard">')
        self.assertGreater(strip_match.start(), workspace_start)
        self.assertLess(strip_match.start(), first_card_start)

    def test_strip_has_no_data_tab_and_is_not_a_tab_card(self):
        strip_match = _SUBTAB_STRIP_RE.search(HTML)
        self.assertIsNotNone(strip_match, "#modsSubtabs strip not found")
        opening_tag = HTML[strip_match.start():HTML.index(">", strip_match.start()) + 1]
        self.assertNotIn("data-tab", opening_tag)
        self.assertNotIn("tab-card", opening_tag)

    def test_subtab_buttons_order_ids_controls_labels_and_initial_state(self):
        buttons = _SUBTAB_BUTTON_RE.findall(HTML)
        self.assertEqual(len(buttons), 2, "expected exactly two sub-tab buttons")
        (qol_attrs, qol_label), (items_attrs, items_label) = buttons
        self.assertIn('id="subtab-qol"', qol_attrs)
        self.assertIn('aria-controls="qolCard"', qol_attrs)
        self.assertIn('aria-selected="true"', qol_attrs)
        self.assertIn('tabindex="0"', qol_attrs)
        self.assertEqual(qol_label, "Quality of Life")
        self.assertNotIn("tabbtn", qol_attrs)
        self.assertIn('id="subtab-items"', items_attrs)
        self.assertIn('aria-controls="itemsCard"', items_attrs)
        self.assertIn('aria-selected="false"', items_attrs)
        self.assertIn('tabindex="-1"', items_attrs)
        self.assertEqual(items_label, "Items")
        self.assertNotIn("tabbtn", items_attrs)

    def test_subtab_aria_controls_equals_the_mods_card_ids(self):
        controls = set(re.findall(r'class="subtabbtn"[^>]*aria-controls="([^"]+)"', HTML))
        card_ids = {cid for cid, _ in _mods_cards(HTML)}
        self.assertEqual(controls, card_ids)

    def test_mods_cards_have_role_tabpanel_and_aria_labelledby(self):
        self.assertIn('id="qolCard" role="tabpanel" aria-labelledby="subtab-qol"', HTML)
        self.assertIn('id="itemsCard" role="tabpanel" aria-labelledby="subtab-items"', HTML)

    def test_subtabbar_css_spans_the_grid(self):
        match = re.search(r"\.subtabbar\{([^}]*)\}", HTML)
        self.assertIsNotNone(match, ".subtabbar CSS rule not found")
        self.assertIn("grid-column:1/-1", match.group(1))

    def test_subtabbtn_shares_the_row_under_720px(self):
        block = _brace_block(HTML, "@media(max-width:720px){")
        subtab_match = re.search(r"\.subtabbtn\{([^}]*)\}", block)
        self.assertIsNotNone(subtab_match, ".subtabbtn has no rule inside the 720px block")
        self.assertIn("flex:1", subtab_match.group(1))

    def test_boot_reads_mods_subtab_storage_before_open_tab(self):
        boot_src = _brace_block(HTML, "async function boot(){")
        storage_pos = boot_src.find("forgepact_mods_subtab")
        open_tab_pos = boot_src.find("openTab(initial,false)")
        self.assertGreater(storage_pos, -1, "boot does not read forgepact_mods_subtab")
        self.assertGreater(open_tab_pos, -1, "boot does not call openTab(initial,false)")
        self.assertLess(storage_pos, open_tab_pos)

    def test_prepare_panel_ui_calls_bind_mods_subtabs(self):
        prepare_src = _brace_block(HTML, "function preparePanelUI(){")
        self.assertIn("bindModsSubtabs()", prepare_src)


# A stub DOM just capable enough to run openTab/openModsSubtab/bindModsSubtabs:
# elements with class/id/dataset/attributes, getElementById and a small
# selector engine (classes and [attr="value"]) for the compound selectors
# those functions use, sessionStorage and a no-op window.scrollTo.
_STUB_DOM = r"""
function makeElement(attrs){
  const el={
    className:attrs.class||'',
    id:attrs.id||'',
    hidden:!!attrs.hidden,
    tabIndex:0,
    value:'',
    textContent:'',
    dataset:{},
    _attrs:{},
    onclick:null,onkeydown:null,
    get classList(){
      const self=this;
      const asSet=()=>new Set((self.className||'').split(/\s+/).filter(Boolean));
      return {
        contains:c=>asSet().has(c),
        add:c=>{const s=asSet();s.add(c);self.className=[...s].join(' ')},
        remove:c=>{const s=asSet();s.delete(c);self.className=[...s].join(' ')},
        toggle:(c,on)=>{const s=asSet();on=on===undefined?!s.has(c):on;if(on)s.add(c);else s.delete(c);self.className=[...s].join(' ');return on}
      };
    },
    getAttribute(n){return Object.prototype.hasOwnProperty.call(this._attrs,n)?this._attrs[n]:null},
    setAttribute(n,v){this._attrs[n]=String(v)},
    hasAttribute(n){return Object.prototype.hasOwnProperty.call(this._attrs,n)},
    click(){if(this.onclick)this.onclick()},
    focus(){document.activeElement=this}
  };
  for(const [k,v] of Object.entries(attrs)){
    if(k==='class'||k==='id'||k==='hidden')continue;
    if(k.startsWith('data-')){
      const camel=k.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase());
      el.dataset[camel]=v;
    }
    el.setAttribute(k,v);
  }
  ALL.push(el);
  return el;
}
function selectorMatches(el,sel){
  const tokens=sel.match(/\.[\w-]+|\[[^\]]+\]|#[\w-]+/g)||[];
  return tokens.every(tok=>{
    if(tok[0]==='.')return el.classList.contains(tok.slice(1));
    if(tok[0]==='#')return el.id===tok.slice(1);
    const inner=tok.slice(1,-1);
    const eq=inner.match(/^([\w-]+)=["']?([^"'\]]*)["']?$/);
    if(eq)return el.getAttribute(eq[1])===eq[2];
    return el.hasAttribute(inner);
  });
}
const ALL=[];
const document={
  activeElement:null,
  getElementById(id){return ALL.find(e=>e.id===id)||null},
  querySelector(sel){return ALL.find(e=>selectorMatches(e,sel))||null},
  querySelectorAll(sel){return ALL.filter(e=>selectorMatches(e,sel))}
};
const sessionStorage={_s:{},getItem(k){return Object.prototype.hasOwnProperty.call(this._s,k)?this._s[k]:null},setItem(k,v){this._s[k]=v}};
const window={scrollTo(){}};
function filterControlRows(){}
makeElement({class:'tabbtn',id:'nav-setup','data-tab':'setup'});
makeElement({class:'tabbtn',id:'nav-modifiers','data-tab':'modifiers'});
makeElement({class:'tabbtn',id:'nav-world','data-tab':'world'});
makeElement({class:'tabbtn',id:'nav-loot','data-tab':'loot'});
makeElement({class:'tabbtn',id:'nav-mods','data-tab':'mods'});
makeElement({class:'tab-card',id:'setupCard','data-tab':'setup'});
makeElement({class:'tab-card',id:'densityCard','data-tab':'world'});
makeElement({class:'tab-card',id:'dropsCard','data-tab':'loot'});
makeElement({class:'tab-card',id:'modifierCard','data-tab':'modifiers'});
makeElement({class:'tab-card',id:'qolCard','data-tab':'mods'});
makeElement({class:'tab-card',id:'itemsCard','data-tab':'mods'});
makeElement({id:'workspace'});
makeElement({id:'pageTitle'});
makeElement({id:'pageDescription'});
makeElement({id:'breadcrumbPage'});
makeElement({id:'controlToolbar'});
makeElement({id:'controlSearch'});
makeElement({id:'modsSubtabs',class:'subtabbar',hidden:true});
makeElement({class:'subtabbtn',id:'subtab-qol','aria-controls':'qolCard'});
makeElement({class:'subtabbtn',id:'subtab-items','aria-controls':'itemsCard'});
"""

# (a)-(h) from context "Round 1: tests". Each step's DOM/state snapshot is
# captured before the next step runs, in one script, so the sequence exactly
# matches how a real session would call these functions.
_SUBTAB_DRIVER = r"""
const out={};

openTab('mods');
out.a={
  stripHidden: document.getElementById('modsSubtabs').hidden,
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active'),
  qolSelected: document.getElementById('subtab-qol').getAttribute('aria-selected'),
  itemsSelected: document.getElementById('subtab-items').getAttribute('aria-selected'),
  qolTabIndex: document.getElementById('subtab-qol').tabIndex,
  itemsTabIndex: document.getElementById('subtab-items').tabIndex,
  toolbarHidden: document.getElementById('controlToolbar').hidden,
  title: document.getElementById('pageTitle').textContent
};

openModsSubtab('itemsCard');
out.b={
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active'),
  qolSelected: document.getElementById('subtab-qol').getAttribute('aria-selected'),
  itemsSelected: document.getElementById('subtab-items').getAttribute('aria-selected'),
  stored: sessionStorage.getItem('forgepact_mods_subtab')
};

openTab('loot');
out.c={
  stripHidden: document.getElementById('modsSubtabs').hidden,
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active')
};

openTab('mods');
out.d={
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active')
};

openModsSubtab('doesNotExist');
out.e={
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active'),
  stored: sessionStorage.getItem('forgepact_mods_subtab')
};

sessionStorage.setItem('forgepact_mods_subtab','sentinel');
openModsSubtab('itemsCard',false);
out.f={stored: sessionStorage.getItem('forgepact_mods_subtab')};

openTab('loot');
openModsSubtab('itemsCard');
out.g={
  qolActive: document.getElementById('qolCard').classList.contains('active'),
  itemsActive: document.getElementById('itemsCard').classList.contains('active')
};

openTab('mods');
bindModsSubtabs();
document.getElementById('subtab-items').click();
out.hClick={itemsActive: document.getElementById('itemsCard').classList.contains('active')};

let prevented=false;
document.getElementById('subtab-items').onkeydown({key:'ArrowRight',preventDefault(){prevented=true}});
out.hArrowRight={activeButton: document.activeElement.id, prevented};

prevented=false;
document.getElementById('subtab-qol').onkeydown({key:'ArrowLeft',preventDefault(){prevented=true}});
out.hArrowLeft={activeButton: document.activeElement.id, prevented};

prevented=false;
document.getElementById('subtab-qol').onkeydown({key:'End',preventDefault(){prevented=true}});
out.hEnd={activeButton: document.activeElement.id, prevented};

prevented=false;
document.getElementById('subtab-items').onkeydown({key:'Home',preventDefault(){prevented=true}});
out.hHome={activeButton: document.activeElement.id, prevented};

prevented=false;
document.getElementById('subtab-qol').onkeydown({key:'ArrowDown',preventDefault(){prevented=true}});
out.hUnrelated={prevented};

console.log(JSON.stringify(out));
"""


class ModsSubtabBehaviourTests(unittest.TestCase):
    """Runs the real openTab/openModsSubtab/bindModsSubtabs through node.

    The node harness is only ever invoked from inside a test method (lazily
    cached on the class), never from setUpClass: a piece missing on the
    pre-change panel must show up as a test failure, not a setUp error.
    """

    _cache = None

    @classmethod
    def _results(cls):
        if cls._cache is None:
            policy_js = "\n".join([
                _STUB_DOM,
                _mods_state_declaration_source(HTML),
                _page_info_source(HTML),
                _brace_block(HTML, "function openTab(name,remember=true){"),
                _brace_block(HTML, "function openModsSubtab(id,remember=true){"),
                _brace_block(HTML, "function bindModsSubtabs(){"),
            ])
            cls._cache = _run_node(policy_js, _SUBTAB_DRIVER)
        return cls._cache

    def test_a_open_tab_mods_fresh_shows_only_quality_of_life(self):
        a = self._results()["a"]
        self.assertFalse(a["stripHidden"])
        self.assertTrue(a["qolActive"])
        self.assertFalse(a["itemsActive"])
        self.assertEqual(a["qolSelected"], "true")
        self.assertEqual(a["itemsSelected"], "false")
        self.assertEqual(a["qolTabIndex"], 0)
        self.assertEqual(a["itemsTabIndex"], -1)
        self.assertTrue(a["toolbarHidden"])
        self.assertEqual(a["title"], "Mods")

    def test_b_open_mods_subtab_items_flips_cards_and_buttons_and_stores(self):
        b = self._results()["b"]
        self.assertFalse(b["qolActive"])
        self.assertTrue(b["itemsActive"])
        self.assertEqual(b["qolSelected"], "false")
        self.assertEqual(b["itemsSelected"], "true")
        self.assertEqual(b["stored"], "itemsCard")

    def test_c_open_tab_loot_hides_strip_and_no_mods_card_active(self):
        c = self._results()["c"]
        self.assertTrue(c["stripHidden"])
        self.assertFalse(c["qolActive"])
        self.assertFalse(c["itemsActive"])

    def test_d_open_tab_mods_again_restores_items(self):
        d = self._results()["d"]
        self.assertFalse(d["qolActive"])
        self.assertTrue(d["itemsActive"])

    def test_e_unknown_subtab_id_falls_back_to_quality_of_life(self):
        e = self._results()["e"]
        self.assertTrue(e["qolActive"])
        self.assertFalse(e["itemsActive"])
        self.assertEqual(e["stored"], "qolCard")

    def test_f_remember_false_writes_nothing(self):
        self.assertEqual(self._results()["f"]["stored"], "sentinel")

    def test_g_no_mods_card_activates_while_active_tab_is_not_mods(self):
        g = self._results()["g"]
        self.assertFalse(g["qolActive"])
        self.assertFalse(g["itemsActive"])

    def test_h_click_selects_its_card(self):
        self.assertTrue(self._results()["hClick"]["itemsActive"])

    def test_h_arrow_right_wraps_and_focuses(self):
        result = self._results()["hArrowRight"]
        self.assertEqual(result["activeButton"], "subtab-qol")
        self.assertTrue(result["prevented"])

    def test_h_arrow_left_wraps_and_focuses(self):
        result = self._results()["hArrowLeft"]
        self.assertEqual(result["activeButton"], "subtab-items")
        self.assertTrue(result["prevented"])

    def test_h_home_and_end_jump(self):
        end_result = self._results()["hEnd"]
        self.assertEqual(end_result["activeButton"], "subtab-items")
        self.assertTrue(end_result["prevented"])
        home_result = self._results()["hHome"]
        self.assertEqual(home_result["activeButton"], "subtab-qol")
        self.assertTrue(home_result["prevented"])

    def test_h_unrelated_key_does_not_prevent_default(self):
        self.assertFalse(self._results()["hUnrelated"]["prevented"])


if __name__ == "__main__":
    unittest.main()
