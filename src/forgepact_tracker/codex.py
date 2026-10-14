"""The item codex: every named item, how rare it is and where to farm it.

HS Offline Tracker's "Items" view (`HS-Offline-Tracker/src/Codex.svelte`, MIT,
see `CREDITS.md`) ported to the panel's API: one row per item name the tables
know, built from `item_table.py`'s identities and the four drop tables that
came from the Tracker's `items.js`. Two chances matter, and they are not the
same number: `rate`, the general "one in N" anywhere, and `chase`, the better
one inside the places the item is tied to (the number the game's tooltip
prints in green). An item with a `chase` is worth farming somewhere in
particular; one with only a `rate` is not.

A row is `{name, rarity, tier, type, group, rate, chase, zones, places}`:

- `type` is the item's kind as the Tracker labels it (a weapon's own kind,
  "Sword", else its item type, "Helmet"); `group` is the item type alone, for
  a filter by kind;
- `rarity` is `""` and `tier` 0 where the tables have none, `rate` and
  `chase` 0 where they cannot price it;
- `zones` are zone codes ("8-2" act 8 zone 2, "8-D" a dungeon, "8-BD" a boss
  dungeon) and `places` the bosses and chests in words, blanks left out.

One name can be worn by several identities; the first in the identity table
gives the row its kind, as the Tracker's first sighting did.
"""

from __future__ import annotations

from . import item_table

_TYPE_NAMES = dict(item_table.TYPE_NAMES)
_WEAPON_NAMES = dict(item_table.WEAPON_NAMES)
_RARITY = dict(item_table.RARITY_BY_NAME)
_TIER = dict(item_table.TIER_BY_NAME)
_RATE = dict(item_table.DROP_RATE)
_CHASE = dict(item_table.DROP_CHASE)
_ZONES = dict(item_table.DROP_ZONES)
_PLACES = dict(item_table.DROP_PLACES)
_WEAPON = 3

_ROWS: list | None = None


def group_label(item_type: int) -> str:
    """The item type's label ("Helmet"), or `Type <n>` for one the table lacks."""
    return _TYPE_NAMES.get(item_type, "Type %d" % item_type)


def type_label(item_type: int, weapon_type: int) -> str:
    """The item's kind: a weapon's own kind, else its item type (the Tracker's
    `typeLabel`)."""
    if item_type == _WEAPON and weapon_type > 0:
        return _WEAPON_NAMES.get(weapon_type, "Weapon %d" % weapon_type)
    return group_label(item_type)


def _build() -> list:
    seen = {}
    for packed, name in item_table.ITEMS:
        if name in seen:
            continue
        item_type, weapon_type = packed >> 24, packed & 0xFF
        key = name.lower()
        seen[name] = {
            "name": name,
            "rarity": _RARITY.get(key, ""),
            "tier": _TIER.get(key, 0),
            "type": type_label(item_type, weapon_type),
            "group": group_label(item_type),
            "rate": _RATE.get(key, 0),
            "chase": _CHASE.get(key, 0),
            "zones": [z for z in _ZONES.get(key, ()) if z],
            "places": [p for p in _PLACES.get(key, ()) if p],
        }
    return list(seen.values())


def rows() -> list:
    """Every codex row, in the identity table's order. Built once; each call
    answers fresh copies, so a caller may change what it gets."""
    global _ROWS
    if _ROWS is None:
        _ROWS = _build()
    return [dict(r, zones=list(r["zones"]), places=list(r["places"])) for r in _ROWS]
