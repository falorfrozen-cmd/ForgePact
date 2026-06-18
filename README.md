# ForgePact — Hero Siege Offline Mod Panel

A one-click desktop control panel for tuning your **offline** Hero Siege runs. Native window, no browser, no setup hassle.

## ✨ Features
- **Monster Density** — 1–5×
- **Special Content Spawns** — Rift Portals, Battlefields, Cursed Orbs, Summon Portals, Chaos Pillars (up to 100×)
- **Drop Rates** — Relics, Dungeon Keys, Gold (up to 100×)
- **Full Map Reveal** — no fog of war, in every zone
- **Relic Gate** — relics drop from every kill in Satanic Zones
- **Fast launch** + settings auto-applied every time

## 🔧 How to use
1. Run `ForgePact.exe`, set the path to your `Hero_Siege.exe`, and hit **Save**.
2. Click **Install Mod Plugin** (with the game **closed**). It **backs up** your exe, copies the mod, and patches the game so it loads on startup. A small black command-prompt (CMD) window briefly opens during the patch — **this is normal, just let it finish** (~1 minute, one time only).
3. Click **Launch Modded Game** and play.

> The first modded launch does a one-time ~1 minute setup (the mod scans the game once), then it's cached and every launch after that starts instantly. Your settings are saved and re-applied automatically.

**To uninstall:** click **Remove Plugin** (with the game closed) — it restores your original exe from the backup and removes the mod files, returning the game to normal. The backup is kept, so you can re-install any time.

## ⚠️ Offline only — read this
This works **only** with anti-cheat (**EAC**) disabled — it does **NOT** work on the normal online Steam client, and it never will (EAC blocks it by design). This is a **single-player / offline** tool.

It does **not** include, provide, or explain any anti-cheat bypass, crack, or any way to get the game — you must already have a copy set up for offline play. **Do not use it online** (it won't load anyway, and modding online games is against their rules).

## 📥 Download
Grab the latest **[Release](../../releases/latest)** → download `ForgePact.zip`, extract it anywhere, and run `ForgePact.exe`.

## 📦 Source layout
- `src/forgepact.py` — the control panel (Python). The released `.exe` is a PyInstaller build of this file.
- `plugin/` — **BloodPactPlugin** source: the C++ mod that hooks the game (`ModuleMain.cpp`) + build scripts + [BUILD.md](plugin/BUILD.md).
- `yytoolkit-modified/` — our one-file change to YYToolkit, with build/upstream notes ([NOTICE.md](yytoolkit-modified/NOTICE.md)).

## 📜 License — AGPL-3.0
ForgePact is built on **Aurie Framework** and **YYToolkit**, both licensed under the **GNU Affero General Public License v3.0**. Because of their copyleft, **all of ForgePact** — this control panel, the BloodPactPlugin, and our YYToolkit modification — **is released under AGPL-3.0** (see [LICENSE](LICENSE)).

You may use, study, modify, and redistribute it under the same license. If you distribute a modified version, you must also make its complete source available under AGPL-3.0.

## 🙏 Credits
ForgePact stands entirely on the work of the **AurieFramework** authors:
- **Aurie Framework** — https://github.com/AurieFramework/Aurie (AGPL-3.0)
- **YYToolkit (YYTK)** — https://github.com/AurieFramework/YYToolkit (AGPL-3.0)

ForgePact is an independent, fan-made project and is **not affiliated with or endorsed by** AurieFramework, Panic Art Studios, or Hero Siege. See [CREDITS.md](CREDITS.md).
