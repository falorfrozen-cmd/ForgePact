"""Slider on/off switches and the saved theme: the backend half.

Every slider except Monster Density gets a switch that keeps its value, the way
`density_on` keeps `density`. Off makes the backend behave exactly as if the
slider stood at its default - startup's `build_cmds` and the live `/api/set`
send alike - and on restores the saved value, with no new command kind.

The baseline classes pin today's output as literals, so a switch that leaked
into the all-on path (every existing config carries no `switches` key) shows up
as a changed list. Runs against `PanelSandbox`: isolated settings, `send_cmds`
captured, never the game's IPC.
"""
import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import forgepact
from test_satanic_panel import PanelSandbox


def non_default_cfg():
    """Several sliders off their default: every table section, plus the four
    top-level sliders (rarity twice, angelic, enemy speed)."""
    cfg = copy.deepcopy(forgepact.DEFAULTS)
    cfg["stats"]["exp"] = 3
    cfg["stats"]["movespeed"] = 2.5
    cfg["percent_stats"]["damage"] = 25
    cfg["percent_stats"]["castrate"] = 40
    cfg["spawners"]["rift"] = 5
    cfg["spawners"]["shadowrealm"] = 3
    cfg["drops"]["gold"] = 10
    cfg["drops"]["mining_ore"] = 4
    cfg["keys"]["dungeon"] = 3
    cfg["keys"]["relic"] = 2
    cfg["keys"]["rune"] = 4
    cfg["rarity_rare"] = 20
    cfg["rarity_ancient"] = 10
    cfg["angelic_items"] = 3
    cfg["enemy_speed"] = 50
    return cfg


# build_cmds(non_default_cfg()) as it was before switches existed.
BASELINE_BUILD_CMDS = [
    "rarity 20 10",
    "angelicdrop 3750",
    "enemyspeed 1.5 ct",
    "specialrate rift 5",
    "specialrate shadowrealm 3",
    "dropmult gold 10",
    "miningore 4",
    "stat exp 3",
    "stat movespeed 2.5",
    "stat damage 1.25",
    "statadd castrate 40",
    "dungeonkey add 12 1",
    "dungeonkey add 41 2",
    "dungeonkey chance auto",
    "dungeonkey on",
    "droprate group dungeon 3",
    "droprate group relic 2",
    "droprate group rune 4",
]

_KEY_TAIL = ["droprate group angelic 1", "droprate group chaos 1", "droprate group bifrost 1",
             "droprate group relic 1", "droprate group rune 1", "droprate group stone 1",
             "droprate group bossgem 1", "droprate group orb 1", "droprate group scrollofra 1",
             "droprate group dimshard 1", "droprate group battlefrag 1",
             "droprate group colosfrag 1", "droprate group ruby 1"]
_KEY_RESETS = ["dungeonkey del 12", "dungeonkey del 16", "dungeonkey del 41"]

# One slider per section: (section, key, max, min, lines at max, lines at min),
# each the live /api/set send recorded before switches existed.
LIVE_SLIDERS = [
    ("stats", "exp", 100, 1, ["stat exp 100"], ["stat exp 1"]),
    ("percent_stats", "damage", 1000, 0, ["stat damage 11"], ["stat damage 1"]),
    ("spawners", "rift", 100, 1, ["specialrate rift 100"], ["specialrate rift 1"]),
    ("drops", "gold", 100, 1, ["dropmult gold 100"], ["dropmult gold 1"]),
    ("keys", "dungeon", 100, 1,
     _KEY_RESETS + ["dungeonkey add 12 1", "dungeonkey chance auto", "dungeonkey on",
                    "droprate group dungeon 100"] + _KEY_TAIL,
     _KEY_RESETS + ["dungeonkey off", "droprate group dungeon 1"] + _KEY_TAIL),
    (None, "rarity_rare", 100, 0, ["rarity 100 0"], ["rarity off"]),
    (None, "rarity_ancient", 100, 0, ["rarity 0 100"], ["rarity off"]),
    (None, "angelic_items", 100, 1, ["angelicdrop 76"], ["angelicdrop off"]),
    (None, "enemy_speed", 300, 0, ["enemyspeed 4 ct"], ["enemyspeed 1 ct"]),
]


def switch_id(section, key):
    return f"{section}.{key}" if section else key


class LiveSandbox:
    """PanelSandbox with the game reported running and each POST's sends read back."""

    def __init__(self, test):
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        test.addCleanup(self.sandbox.__exit__)
        self.sandbox.mocks[1].return_value = True   # game_running
        self.sent = self.sandbox.mocks[3]           # send_cmds

    def post(self, section, key, value):
        self.sent.reset_mock()
        code, body = self.sandbox.request(dict(section=section, key=key, value=value))
        return code, body, [c.args[0] for c in self.sent.call_args_list]

    def saved(self):
        return json.loads(self.sandbox.config.read_text(encoding="utf-8"))


class BaselineBuildCmdsTests(unittest.TestCase):
    def test_baseline_build_cmds_for_non_default_sliders(self):
        self.assertEqual(forgepact.build_cmds(non_default_cfg()), BASELINE_BUILD_CMDS)

    def test_baseline_config_without_switches_key(self):
        cfg = non_default_cfg()
        cfg.pop("switches", None)
        cfg.pop("theme", None)
        self.assertEqual(forgepact.build_cmds(cfg), BASELINE_BUILD_CMDS)


class BaselineLiveSendTests(unittest.TestCase):
    def test_baseline_live_min_and_max_per_section(self):
        live = LiveSandbox(self)
        for section, key, hi, lo, at_hi, at_lo in LIVE_SLIDERS:
            with self.subTest(slider=switch_id(section, key)):
                code, _, sent = live.post(section, key, hi)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [at_hi])
                code, _, sent = live.post(section, key, lo)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [at_lo])


class SliderSwitchTests(unittest.TestCase):
    def test_switch_ids_cover_every_slider_but_density(self):
        ids = forgepact.SLIDER_SWITCH_IDS
        expected = ([f"stats.{k}" for k, *_ in forgepact.STATS]
                    + [f"percent_stats.{k}" for k, *_ in forgepact.PERCENT_STATS]
                    + [f"spawners.{k}" for k, *_ in forgepact.SPAWNERS]
                    + [f"drops.{k}" for k, *_ in forgepact.DROPS]
                    + [f"keys.{k}" for k, *_ in forgepact.KEYS]
                    + ["rarity_rare", "rarity_ancient", "angelic_items", "enemy_speed"])
        self.assertEqual(list(ids), expected)
        self.assertEqual(len(ids), 40)
        self.assertEqual(len(set(ids)), 40)
        for excluded in ("density", "density_on", "enemy_speed_ct"):
            self.assertNotIn(excluded, ids)

    def test_defaults_carry_switches_and_theme(self):
        self.assertEqual(forgepact.DEFAULTS["switches"], {})
        self.assertEqual(forgepact.DEFAULTS["theme"], "default")
        with PanelSandbox() as sandbox:
            code, state = sandbox.request()
        self.assertEqual(code, 200)
        self.assertEqual(state["cfg"]["switches"], {})
        self.assertEqual(state["cfg"]["theme"], "default")

    def test_build_cmds_unchanged_when_every_switch_is_on(self):
        for label, switches in (("empty", {}),
                                ("every id true", {i: True for i in forgepact.SLIDER_SWITCH_IDS})):
            with self.subTest(switches=label):
                cfg = non_default_cfg()
                cfg["switches"] = switches
                self.assertEqual(forgepact.build_cmds(cfg), BASELINE_BUILD_CMDS)

    def test_off_switch_makes_build_cmds_emit_the_default(self):
        # Literal cases first: the slider's own command goes, nothing else moves.
        cfg = non_default_cfg()
        cfg["switches"] = {"stats.exp": False, "keys.relic": False, "enemy_speed": False}
        expected = [c for c in BASELINE_BUILD_CMDS
                    if c not in ("stat exp 3", "enemyspeed 1.5 ct",
                                 "dungeonkey add 41 2", "droprate group relic 2")]
        self.assertEqual(forgepact.build_cmds(cfg), expected)
        # The saved value is untouched: off is not a reset.
        self.assertEqual(cfg["stats"]["exp"], 3)
        self.assertEqual(cfg["keys"]["relic"], 2)
        self.assertEqual(cfg["enemy_speed"], 50)
        # Every id: off equals the slider standing at its default.
        for sid in forgepact.SLIDER_SWITCH_IDS:
            with self.subTest(switch=sid):
                off = non_default_cfg()
                off["switches"] = {sid: False}
                at_default = non_default_cfg()
                if "." in sid:
                    sec, key = sid.split(".")
                    at_default[sec][key] = forgepact.DEFAULTS[sec][key]
                else:
                    at_default[sid] = forgepact.DEFAULTS[sid]
                self.assertEqual(forgepact.build_cmds(off), forgepact.build_cmds(at_default))

    def test_set_switch_off_live_sends_the_sliders_own_default_command(self):
        live = LiveSandbox(self)
        for section, key, hi, _lo, _at_hi, at_lo in LIVE_SLIDERS:
            sid = switch_id(section, key)
            with self.subTest(switch=sid):
                live.post(section, key, hi)
                code, body, sent = live.post("switches", sid, False)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [at_lo])
                self.assertIs(body["cfg"]["switches"][sid], False)
                saved = live.saved()
                self.assertIs(saved["switches"][sid], False)
                value = saved[section][key] if section else saved[key]
                self.assertEqual(value, hi, "the switch must keep the slider's value")
                # Put it back so the next row starts from all-on.
                live.post("switches", sid, True)
                live.post(section, key, forgepact.DEFAULTS[section][key] if section
                          else forgepact.DEFAULTS[key])

    def test_set_switch_on_live_sends_the_remembered_value(self):
        live = LiveSandbox(self)
        for section, key, hi, _lo, at_hi, _at_lo in LIVE_SLIDERS:
            sid = switch_id(section, key)
            with self.subTest(switch=sid):
                live.post(section, key, hi)
                live.post("switches", sid, False)
                code, body, sent = live.post("switches", sid, True)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [at_hi])
                # Only off entries are stored: on removes the entry.
                self.assertNotIn(sid, body["cfg"]["switches"])
                self.assertNotIn(sid, live.saved()["switches"])
                live.post(section, key, forgepact.DEFAULTS[section][key] if section
                          else forgepact.DEFAULTS[key])

    def test_slider_change_while_off_saves_value_and_sends_default(self):
        live = LiveSandbox(self)
        for section, key, hi, _lo, _at_hi, at_lo in LIVE_SLIDERS:
            sid = switch_id(section, key)
            with self.subTest(switch=sid):
                live.post("switches", sid, False)
                code, _, sent = live.post(section, key, hi)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [at_lo])
                saved = live.saved()
                self.assertEqual(saved[section][key] if section else saved[key], hi)
                self.assertIs(saved["switches"][sid], False)
                live.post("switches", sid, True)
                live.post(section, key, forgepact.DEFAULTS[section][key] if section
                          else forgepact.DEFAULTS[key])

    def test_switch_off_leaves_other_sliders_live_values_alone(self):
        live = LiveSandbox(self)
        live.post(None, "rarity_ancient", 30)
        live.post(None, "rarity_rare", 40)
        code, _, sent = live.post("switches", "rarity_rare", False)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["rarity 0 30"]])
        # enemy_speed_ct is a scope, not a value: while the speed switch is off
        # a scope change still sends the default speed.
        live.post(None, "enemy_speed", 100)
        live.post("switches", "enemy_speed", False)
        _, _, sent = live.post(None, "enemy_speed_ct", False)
        self.assertEqual(sent, [["enemyspeed 1 all"]])

    def test_switch_without_game_saves_and_sends_nothing(self):
        live = LiveSandbox(self)
        live.sandbox.mocks[1].return_value = False
        code, body, sent = live.post("switches", "stats.exp", False)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [])
        self.assertEqual(body["ok"], "saved")
        self.assertIs(live.saved()["switches"]["stats.exp"], False)

    def test_unknown_switch_is_refused(self):
        live = LiveSandbox(self)
        before = live.sandbox.config.read_bytes()
        for bad in ("density", "density_on", "enemy_speed_ct", "stats.nope", "exp",
                    "theme", "", None, ["stats.exp"]):
            with self.subTest(key=bad):
                code, body, sent = live.post("switches", bad, False)
                self.assertEqual(code, 400)
                self.assertEqual(body, {"err": "unknown switch"})
                self.assertEqual(sent, [])
                self.assertEqual(live.sandbox.config.read_bytes(), before)

    def test_theme_is_saved_and_validated(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(None, "theme", "alt")
        self.assertEqual(code, 200)
        self.assertEqual(sent, [], "a theme is a panel setting, never a command")
        self.assertEqual(body["cfg"]["theme"], "alt")
        self.assertEqual(live.saved()["theme"], "alt")
        code, body, _ = live.post(None, "theme", "night-2")
        self.assertEqual(code, 200)
        self.assertEqual(live.saved()["theme"], "night-2")
        before = live.sandbox.config.read_bytes()
        for bad in ("Alt", "", "2dark", "-x", "a b", "a" * 33, "alt\n", None, 5, ["alt"]):
            with self.subTest(theme=bad):
                code, body, sent = live.post(None, "theme", bad)
                self.assertEqual(code, 400)
                self.assertEqual(body, {"err": "invalid theme"})
                self.assertEqual(sent, [])
                self.assertEqual(live.sandbox.config.read_bytes(), before)
        code, _, _ = live.post(None, "theme", "a" * 32)
        self.assertEqual(code, 200)

    def test_saved_switches_survive_load_cfg(self):
        with PanelSandbox() as sandbox:
            cfg = json.loads(sandbox.config.read_text(encoding="utf-8"))
            cfg["switches"] = {"keys.relic": False}
            cfg["theme"] = "alt"
            sandbox.config.write_text(json.dumps(cfg), encoding="utf-8")
            _, state = sandbox.request()
        self.assertEqual(state["cfg"]["switches"], {"keys.relic": False})
        self.assertEqual(state["cfg"]["theme"], "alt")
        self.assertEqual(forgepact.DEFAULTS["switches"], {}, "DEFAULTS must never be mutated")


if __name__ == "__main__":
    unittest.main()
