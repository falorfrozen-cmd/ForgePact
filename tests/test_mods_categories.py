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
"""
import os
import pathlib
import re
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
_MODS_CARD_RE = re.compile(r'<div class="card tab-card" data-tab="mods" id="([^"]+)">')


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


if __name__ == "__main__":
    unittest.main()
