"""Kill a boss repeatedly through the game's own death path and count Prime Evil part drops.

Research build only (it uses the spawn, call and inspection commands a player
build refuses). The hero must already be in a zone, not in town: a boss spawned
in town removes itself. HS-AFK-Expedition's `tools/game_session.py prepare`,
then `travel --room Act_01_01`, gets them there without any input.

For each kill:
1. Spawn the boss 1200 px to the hero's right (--dx to change it), so it cannot reach them.
2. Wait for its drop table.
3. Set its protected HP to 0 with PC_SetVariableGMLWrapper.
4. The game's own step kills it: Destroy, then DropItem, LoadDrops and DropBossParts.
5. With --destroy, a boss that outlives HP 0 (Uber Endrixia, Uber Anubis) gets HP 0
   again and instance_destroy: its Destroy event then runs with HP at 0, and the enemy
   parent's Destroy takes the drop path. Never use it on Uber_Luna_obj: that closed the game.

A boss that dies where loot cannot land drops nothing, for example past the room's
edge. Check the spot with a Karp King kill first: it should drop loot there.

The drops are read from bp_ipc\\itemdrops.jsonl, which only the research build
writes. See docs/prime-evil-parts-research.md.

usage: py -3 tools/boss_drop_trial.py <multiplier> <kills> [Boss_obj] [out.json] [--dx N] [--destroy]
"""
from __future__ import annotations

import json
import os
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / "hs-game-sdk" / "python"))
from hs_game_sdk import GameObject  # noqa: E402

PARTS = ("Gurag's Soul", "Death's Sigil", "Damien's Eye", "Anubis' Ankh", "Karp King's Bellybutton",
         "Satan's Horn", "Gurag's Infernal Soul", "Death's Infernal Sigil", "Damien's Infernal Eye",
         "Anubis Infernal Ankh", "Karp King's Infernal Bellybutton", "Satan's Infernal Horn")
# What DropUberParts creates instead: category 13's items 14-18 and their infernal versions 49-53.
UBER_ITEMS = ("Soul of Anguish", "Soul of Despair", "Soul of Corruption", "Scroll of Ra",
              "Colosseum Fragment", "Soul of Infernal Anguish", "Soul of Infernal Despair",
              "Soul of Infernal Corruption", "Infernal Scroll of Ra", "Infernal Colosseum Fragment")


def ipc_dir() -> Path:
    config = Path(os.environ["LOCALAPPDATA"]) / "Hero_Siege" / "forgepact.json"
    exe = Path(json.loads(config.read_text(encoding="utf-8"))["game_exe"])
    return exe.parent / "bp_ipc"


def _size(p: Path) -> int:
    try:
        return p.stat().st_size
    except OSError:
        return 0


def _read_from(p: Path, offset: int) -> str:
    try:
        with open(p, "rb") as f:
            f.seek(offset)
            return f.read().decode("utf-8", "replace")
    except OSError:
        return ""


def send(ipc: Path, cmds: list[str], wait: float) -> tuple[list[str], list[str]]:
    """Append commands as the panel does, wait, and return the new out.txt lines and drop names."""
    out, drops = ipc / "out.txt", ipc / "itemdrops.jsonl"
    o0, d0 = _size(out), _size(drops)
    with open(ipc / "cmd.txt", "a", encoding="ascii") as f:
        f.writelines(c + "\n" for c in cmds)
    time.sleep(wait)
    lines = [line for line in _read_from(out, o0).splitlines() if line.strip()]
    names = []
    for line in _read_from(drops, d0).splitlines():
        try:
            item = json.loads(line).get("it", {})
        except ValueError:
            continue
        name = (item.get("itemInfoStruct") or {}).get("28") or item.get("28") or ""
        if name:
            names.append(name)
    return lines, names


def _number(lines: list[str], pattern: str) -> float | None:
    for line in lines:
        m = re.search(pattern, line)
        if m:
            return float(m.group(1))
    return None


def main(argv: list[str]) -> int:
    dx, destroy, rest = 1200.0, False, []
    args = iter(argv)
    for arg in args:
        if arg == "--dx":
            dx = float(next(args))
        elif arg == "--destroy":
            destroy = True
        else:
            rest.append(arg)
    mult, kills = int(rest[0]), int(rest[1])
    boss = rest[2] if len(rest) > 2 else "Karp_King_obj"
    out_path = Path(rest[3]) if len(rest) > 3 else None
    ipc, boss_idx = ipc_dir(), GameObject[boss].value
    lines, _ = send(ipc, [f"droprate group primeevil {mult}"], 1.5)
    print(" | ".join(line for line in lines if "droprate group" in line), flush=True)
    results = []
    for k in range(1, kills + 1):
        lines, _ = send(ipc, ["oget Player_obj x", "oget Player_obj y"], 1.0)
        px = _number(lines, r"Player_obj\.x -> real:([-0-9.]+)")
        py = _number(lines, r"Player_obj\.y -> real:([-0-9.]+)")
        if px is None or py is None:
            print("no hero in the room - stopping", flush=True)
            break
        send(ipc, [f"cb instance_create_depth {px + dx:.0f} {py:.0f} 0 {boss_idx}"], 2.0)
        lines, _ = send(ipc, [f"oget {boss} enemy_hp"], 1.0)
        key = _number(lines, r"enemy_hp -> real:([0-9.]+)")
        if key is None:
            print(f"kill {k}: the boss did not stay - skipped", flush=True)
            continue
        _, names = send(ipc, [f"callnum PC_SetVariableGMLWrapper {key:.0f} 0"], 5.0)
        lines, more = send(ipc, [f"cb instance_number {boss_idx}"], 1.0)
        names += more
        gone = _number(lines, r"instance_number .*real:([0-9.]+)") == 0
        if destroy and not gone:
            send(ipc, [f"callnum PC_SetVariableGMLWrapper {key:.0f} 0"], 0.5)
            _, more = send(ipc, [f"cb instance_destroy {boss_idx}"], 5.0)
            names += more
            lines, more = send(ipc, [f"cb instance_number {boss_idx}"], 1.0)
            names += more
            gone = _number(lines, r"instance_number .*real:([0-9.]+)") == 0
        parts = [n for n in names if n in PARTS]
        uber = [n for n in names if n in UBER_ITEMS]
        results.append({"kill": k, "boss_gone": gone, "parts": parts, "uber_items": uber,
                        "named_items": len(set(names))})
        print(f"kill {k}: gone={gone} parts={len(parts)} uber items={len(uber)} "
              f"named items={len(set(names))}", flush=True)
    summary = {"multiplier": mult, "boss": boss, "kills": len(results),
               "parts": sum(len(r["parts"]) for r in results),
               "uber_items": sum(len(r["uber_items"]) for r in results), "results": results}
    print(json.dumps({k: v for k, v in summary.items() if k != "results"}), flush=True)
    if out_path:
        out_path.write_text(json.dumps(summary, indent=1), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
