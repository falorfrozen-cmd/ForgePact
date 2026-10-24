"""Run the real ForgePact::AutoSellMod class against a controlled game API.

Auto-sell sells a ground item the player's own loot filter hides, at the end
of the frame it dropped in: it credits the price a merchant pays,
ceil(itemInfoStruct["9"] x the stack), through the game's own gold credit,
writes the game's gold log, and removes the ground item. These scenarios pin
the baseline (switched off, the class asks the game nothing at all), the
target (a hidden drop is credited 113 once, logged once and destroyed), what
it leaves alone and counts instead (a shown drop, no verdict, a persistent
room, an unsellable price, no local player, a refused credit, a failed
destroy, a struct already credited in the room), the call order, room
changes, the price function `autosell quote` shares, and the stat line's
fields.

Replan 1 (after Live 1): the game's credit answers `undefined` when it pays
(static reading), so the balance read either side of the credit decides
whether the player was paid, and the refusal names its cause (the call did
not dispatch, it answered false, or it paid nothing). The five item types the
game will not buy (key, tarot card, material, socketable, relic) and an item
whose type cannot be read are never sold. The header includes the SDK's
`hs_game_sdk/item_type.hpp`, so the harness builds against the hub's
`hs-game-sdk/cpp/include`, as tests/test_mining_ore_behavior.py does.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AutoSellBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/AutoSellMod.hpp").read_text(encoding="utf-8")
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        # One directory per process, as tests/test_hidden_loot_behavior.py
        # does: a suite run and a targeted run may compile at the same time.
        out = ROOT / "build/auto-sell-behavior" / f"pid-{os.getpid()}"
        out.mkdir(parents=True, exist_ok=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        code = (ROOT / "tests/auto_sell_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_AUTOSELL", klass)
        cpp = out / "autosell.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("autosell.exe" if os.name == "nt" else "autosell")
        sdk = ROOT.parent / "hs-game-sdk/cpp/include"
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" /I "{sdk}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "autosell.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-I", str(ROOT / "plugin/include"), "-I", str(sdk), str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        # Also covers what no single method below names: no builtin outside
        # the world's list is called, and the first target item stays gone.
        self.assertIn("RESULT OK", self.output, self.output)
        for label in ("calls/only_listed_builtins", "sold/first_target_item_gone"):
            self.assertScenario(label)

    def test_baseline(self):
        """Switched off: a noted hidden drop stays and the class makes no call."""
        for label in ("off/starts_off", "off/no_runner_calls", "off/no_game_calls", "off/drop_stays",
                      "off/no_gold", "off/notes_not_counted", "off/disable_drops_pending_and_asks_nothing"):
            self.assertScenario(label)

    def test_target(self):
        """Switched on: info 9 of 37.4 times a stack of 3 is credited 113 once, logged once, destroyed."""
        for label in ("on/enable_no_calls", "target/nothing_inside_the_call", "target/credited_113_once",
                      "target/logged_once", "target/destroyed", "target/counted", "target/last",
                      "target/sold_ids", "target/sold_ids_cleared_next_frame", "target/sold_once"):
            self.assertScenario(label)

    def test_call_order_credit_log_destroy(self):
        self.assertScenario("order/credit_log_destroy")
        self.assertScenario("order/one_of_each")

    def test_missing_or_non_positive_count_is_one(self):
        self.assertScenario("count/missing_is_one")
        self.assertScenario("count/non_positive_is_one")

    def test_shown_drop_untouched(self):
        self.assertScenario("visible/never_touched")

    def test_no_verdict_untouched(self):
        self.assertScenario("nofilter/counted_not_touched")

    def test_not_a_ground_item_untouched(self):
        self.assertScenario("unidentified/counted_not_touched")

    def test_instance_already_gone(self):
        self.assertScenario("gone/counted_no_sale")

    def test_unsellable_price_left_on_the_ground(self):
        self.assertScenario("unsellable/left_on_the_ground")
        self.assertScenario("unsellable/counted")

    def test_no_local_player_no_sale(self):
        self.assertScenario("noplayer/no_sale")

    def test_refused_credit_leaves_the_item(self):
        """Answered false (the hash check): credit-false, the item left and its struct not remembered."""
        for label in ("refused/item_left", "refused/counted", "refused/last_false", "refused/not_remembered"):
            self.assertScenario(label)

    def test_credit_answer_undefined_pays(self):
        """Live 1's failure: a credit that answers undefined and raises the balance by the price is a sale."""
        for label in ("credit/undefined_pays", "credit/undefined_last"):
            self.assertScenario(label)

    def test_answer_alone_never_decides_payment(self):
        for label in ("credit/true_without_payment_unpaid", "credit/unpaid_item_left", "credit/unpaid_not_remembered"):
            self.assertScenario(label)

    def test_credit_call_failed(self):
        self.assertScenario("credit/call_failed")
        self.assertScenario("credit/call_failed_last")

    def test_gold_unreadable_before_makes_no_credit(self):
        self.assertScenario("credit/gold_unreadable_before")
        self.assertScenario("credit/gold_unreadable_no_last")

    def test_unverified_credit(self):
        """Unreadable after a dispatched, not-false answer: sold, inexact and unverified; after false or a failure: not paid."""
        self.assertScenario("inexact/unreadable_gold")
        self.assertScenario("credit/unverified_needs_dispatch_and_not_false")

    def test_gold_log_refused_sale_stands(self):
        self.assertScenario("log/refused_sale_stands")

    def test_key_lookup_failure_is_no_key(self):
        self.assertScenario("nokey/counted_not_error")

    def test_unsellable_item_types(self):
        """Key, tarot, material, socketable, relic and an unreadable type are never sold; a ring is."""
        for label in ("types/left_on_the_ground", "types/counted", "types/no_credit_no_balance", "types/ring_sells",
                      "quote/refuses_unsellable_types", "quote/refuses_unreadable_type", "quote/ring_type"):
            self.assertScenario(label)

    def test_inexact_gold(self):
        for label in ("inexact/counted", "inexact/last", "inexact/unreadable_gold"):
            self.assertScenario(label)

    def test_destroy_failed(self):
        self.assertScenario("destroy/failed_counted")
        self.assertScenario("destroy/failed_not_credited_twice")

    def test_same_struct_twice_in_a_room_credited_once(self):
        self.assertScenario("repeat/held_one_credit")
        self.assertScenario("drop/same_instance_once")

    def test_persistent_room_sells_nothing(self):
        for label in ("persistent/nothing_sold", "persistent/room_asked_once",
                      "persistent/unreadable_room_sells_nothing"):
            self.assertScenario(label)

    def test_room_change_forgets(self):
        self.assertScenario("room/forgets_pending_without_calls")
        self.assertScenario("room/forgets_credited_keys")

    def test_exception_in_one_item_does_not_stop_the_next(self):
        self.assertScenario("errors/one_item_isolated")

    def test_quote_uses_the_sale_price(self):
        for label in ("quote/fields", "quote/refused_with_reason", "price/ceil_of_product"):
            self.assertScenario(label)

    def test_stat_line_fields(self):
        for label in ("stat/fields_in_order", "stat/no_other_fields", "stat/credit_refused_is_the_sum",
                      "stat/type_unsellable_within_unsellable", "stat/gold_now_read", "stat/route_both",
                      "stat/route_table_only", "stat/route_none", "stat/gold_now_unreadable"):
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
