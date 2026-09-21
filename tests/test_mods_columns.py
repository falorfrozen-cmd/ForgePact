#!/usr/bin/env python3
"""The Mods tab's two card columns: order kept, left column the taller one.

The cards used to sit in a CSS `columns:2` container, where the browser's own
balancing could leave the right column taller. `setupModsColumns` now splits
the cards between two explicit columns at the earliest point where the left
column is at least as tall as the right. The function is taken out of the
panel's page source and executed through `node` against a stub DOM, so the
tests exercise the shipped code rather than a copy of its rule.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import forgepact
from test_panel_performance import run_node


def setup_source() -> str:
    match = re.search(r"function setupModsColumns\(grid\)\{.*?\n\}\n", forgepact.HTML, re.S)
    if not match:
        raise AssertionError("setupModsColumns not found in the panel page")
    return match.group(0)


# A stub DOM just wide enough for setupModsColumns: elements that can be
# appended and measured, and a ResizeObserver whose callback the driver fires.
STUB = """
let observerCallback=null;
class ResizeObserver{constructor(cb){observerCallback=cb}observe(){}}
const el=(height=0)=>({children:[],height,hidden:false,offsetParent:{},parent:null,className:'',
  append(...nodes){for(const n of nodes){if(n.parent)n.parent.children.splice(n.parent.children.indexOf(n),1);n.parent=this;this.children.push(n)}},
  getBoundingClientRect(){return {height:this.height}}});
const document={createElement:()=>el()};
function layout(heights,opts={}){
  const grid=el();grid.clientWidth=opts.width??900;
  const items=heights.map((h,i)=>{const it=el(h);it.id=i;if(opts.hidden?.includes(i)){it.hidden=true;it.offsetParent=null}return it});
  grid.append(...items);setupModsColumns(grid);
  const cols=()=>grid.children.map(c=>c.children.map(x=>x.id));
  const run=()=>{observerCallback();return cols()};
  return {grid,items,cols,run};
}
"""

DRIVER = """
const out={};
out.even=layout([100,100,100,100]).run();
out.tallLast=layout([100,100,100,400]).run();
out.tallFirst=layout([400,100,100,100]).run();
out.odd=layout([100,100,100]).run();
out.single=layout([100]).run();
out.hiddenIgnored=layout([100,100,500,100,100],{hidden:[2]}).run();
const hidden=layout([100,100,100,100],{width:0});out.invisible=hidden.run();
const grow=layout([100,100,100,100]);grow.run();grow.items[3].height=500;out.grown=grow.run();
grow.items[3].height=100;out.shrunk=grow.run();
console.log(JSON.stringify(out));
"""


class ModsColumnsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.got = run_node(STUB + setup_source(), DRIVER)

    def test_equal_heights_split_evenly(self):
        self.assertEqual(self.got["even"], [[0, 1], [2, 3]])

    def test_left_column_is_taller_when_the_last_card_is_tall(self):
        # [0,1,2]=300 against [3]=400 would leave the right taller.
        self.assertEqual(self.got["tallLast"], [[0, 1, 2, 3], []])

    def test_a_tall_first_card_stands_alone_on_the_left(self):
        self.assertEqual(self.got["tallFirst"], [[0], [1, 2, 3]])

    def test_an_odd_count_puts_the_extra_card_on_the_left(self):
        self.assertEqual(self.got["odd"], [[0, 1], [2]])

    def test_a_single_card_stays_on_the_left(self):
        self.assertEqual(self.got["single"], [[0], []])

    def test_hidden_cards_count_as_zero_height_and_keep_their_place(self):
        # 100+100 on each side; the hidden 500px card adds nothing.
        self.assertEqual(self.got["hiddenIgnored"], [[0, 1], [2, 3, 4]])

    def test_an_unlaid_grid_is_left_alone(self):
        # The Mods tab is display:none until opened; nothing can be measured yet.
        self.assertEqual(self.got["invisible"], [[0, 1, 2, 3], []])

    def test_a_card_that_grows_or_shrinks_rebalances(self):
        self.assertEqual(self.got["grown"], [[0, 1, 2, 3], []])
        self.assertEqual(self.got["shrunk"], [[0, 1], [2, 3]])

    def test_css_columns_are_gone_and_the_narrow_layout_stacks(self):
        css = forgepact.HTML
        self.assertNotIn(".mods-grid{columns:", css)
        self.assertIn(".mods-grid{display:flex", css)
        self.assertIn(".mods-grid{flex-direction:column", css)

    def test_both_mods_cards_are_balanced(self):
        self.assertIn("setupModsColumns(grid);", forgepact.HTML)


if __name__ == "__main__":
    unittest.main()
