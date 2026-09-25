#!/usr/bin/env python3
"""Serve the real panel HTTP handler against a throwaway config, for browser tests.

    py -3 tests/panel_sandbox_server.py [--legacy] [--dist <dir>] [--offline]
                                        [--satanic-minimum] [--seed <json file>]
                                        [--src <dir>]

Used by `panel/tests/` (the behaviour oracle, the screenshot tool and the e2e
suite). It reuses `test_satanic_panel.PanelSandbox` - an isolated
`forgepact.json` in a temp directory, `mod_chain` reporting nothing installed -
and re-patches it so every control's full path can be recorded:

- `game_running` answers True, so `/api/set` takes its live-send branches
  (`--offline` keeps it False, the state the old agent-browser harnesses ran
  in);
- `send_cmds` appends the lines to `<tmp>/cmds.txt` instead of the game's
  `bp_ipc/cmd.txt`, and answers `"<n> command(s) sent"` as the real one does;
- `op_install_mod`, `op_remove_mod` and `launch_modded_game` answer
  `{"ok": "stubbed <name>"}` - nothing is installed, patched or launched;
- `pick_exe_dialog` answers the sandbox's own (empty) `Hero_Siege.exe`.

`--satanic-minimum` starts at `PanelSandbox.at_minimum()` (3 buffs and 2
debuffs enabled, the Satanic card's floor), the fixture the Satanic checks
need.

`--seed <json file>` merges that file's top-level keys into the sandbox's
`forgepact.json` before it serves (after `--satanic-minimum`), so a caller can
start the panel in a given state - design-match.mjs opens each Figma screen in
the state its frame draws. The file must hold a JSON object, and every key must
be one of `forgepact.DEFAULTS`: an unknown key is refused with exit 2 and a
message naming it, so a misspelt key never passes as "seeded".

`--legacy` points `forgepact.PANEL_DIST` at a directory that does not exist,
so `/` serves the page embedded in `forgepact.py`. That page was removed once
the port landed, so `--legacy` now refuses (exit 2) unless the imported
`forgepact` still carries it: the behaviour oracle and the baseline screens
were recorded from it before then, and re-recording needs a tree from before
the port. `--dist <dir>` serves that build; neither serves `panel/dist`.

`--src <dir>` imports `forgepact` from `<dir>` (the directory holding its
`forgepact.py`) instead of this checkout's `src/`, with the hub's
`hs-game-sdk/python` first on `sys.path`, so any older tree can be served: a
`git archive <ref> src` extracted outside the checkout, with `--legacy`, serves
that ref's embedded page. The recording of the Gems of Incarnation controls
(`oracle.mjs record --legacy --only gems --src <dir>`) is made this way from
main's last pre-port page. It refuses (exit 2) when the module it imported is
not the one in `<dir>`.

Prints `port=<n>` then `cmds=<path>` and serves until stdin closes. No route
is added to the product for testing: everything here is a patch on the
module the product already runs.
"""
import argparse
import json
import sys
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
SDK_PYTHON = ROOT.parent / "hs-game-sdk" / "python"


def load(parser, src):
    """PanelSandbox and the forgepact module it drives: this checkout's, or `src`'s."""
    if src is not None:
        src = src.resolve()
        if not (src / "forgepact.py").is_file():
            parser.error(f"--src: {src} holds no forgepact.py")
        if not SDK_PYTHON.is_dir():
            parser.error(f"--src: the hub's {SDK_PYTHON} is missing")
        sys.path.insert(0, str(src))
        sys.path.insert(0, str(SDK_PYTHON))
        # Imported first, so test_satanic_panel's own `import forgepact` finds
        # this module already loaded instead of this checkout's src/.
        import forgepact  # noqa: F401
    sys.path.insert(0, str(TESTS))
    from test_satanic_panel import PanelSandbox, forgepact  # noqa: E402
    if src is not None and Path(forgepact.__file__).resolve().parent != src:
        parser.error(f"--src: imported {forgepact.__file__}, not the one in {src}")
    return PanelSandbox, forgepact


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--legacy", action="store_true",
                        help="serve the page embedded in forgepact.py, not a build")
    parser.add_argument("--dist", type=Path, default=None,
                        help="serve this build directory (default: panel/dist)")
    parser.add_argument("--offline", action="store_true",
                        help="report the game as not running (no live commands)")
    parser.add_argument("--satanic-minimum", action="store_true",
                        help="start with only 3 buffs and 2 debuffs enabled (PanelSandbox.at_minimum)")
    parser.add_argument("--seed", type=Path, default=None,
                        help="merge this JSON object's top-level keys into the sandbox's forgepact.json")
    parser.add_argument("--src", type=Path, default=None,
                        help="import forgepact from this directory (e.g. an extracted git archive of src)")
    args = parser.parse_args(argv)
    PanelSandbox, forgepact = load(parser, args.src)
    if args.legacy and not hasattr(forgepact, "HTML"):
        # Without this, / answers the 503 "panel not built" JSON and a caller
        # waiting for the page reports a timeout instead of the reason.
        parser.error("--legacy: this forgepact.py no longer embeds the old page "
                     "(removed by the UI port); record from a tree before the port "
                     "(--src <dir>, an extracted git archive of its src)")
    seed = None
    if args.seed is not None:
        try:
            seed = json.loads(args.seed.read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            parser.error(f"--seed: cannot read {args.seed}: {e}")
        if not isinstance(seed, dict):
            parser.error(f"--seed: {args.seed} must hold a JSON object")
        unknown = sorted(k for k in seed if k not in forgepact.DEFAULTS)
        if unknown:
            parser.error(f"--seed: unknown config key(s) {', '.join(unknown)} "
                         "(not in forgepact.DEFAULTS)")

    with PanelSandbox() as sandbox:
        if args.satanic_minimum:
            sandbox.at_minimum()
        if seed:
            cfg = json.loads(sandbox.config.read_text(encoding="utf-8"))
            cfg.update(seed)
            sandbox.config.write_text(json.dumps(cfg), encoding="utf-8")
        temp = Path(sandbox.temp.name)
        exe = temp / "Hero_Siege.exe"
        exe.write_bytes(b"")
        cmds = temp / "cmds.txt"
        cmds.write_text("", encoding="ascii")

        def record(lines, cfg=None):
            with cmds.open("a", encoding="ascii") as out:
                out.write("".join(f"{line}\n" for line in lines))
            return f"{len(lines)} command(s) sent"

        def stub(name):
            return lambda *a, **k: {"ok": f"stubbed {name}"}

        if args.legacy:
            dist = temp / "no-panel-build"
        else:
            dist = (args.dist or ROOT / "panel" / "dist").resolve()
        patches = [
            patch.object(forgepact, "game_running", return_value=not args.offline),
            patch.object(forgepact, "send_cmds", side_effect=record),
            patch.object(forgepact, "op_install_mod", side_effect=stub("op_install_mod")),
            patch.object(forgepact, "op_remove_mod", side_effect=stub("op_remove_mod")),
            patch.object(forgepact, "launch_modded_game", side_effect=stub("launch_modded_game")),
            patch.object(forgepact, "pick_exe_dialog", return_value=str(exe)),
            # create=True: this works on a tree where the static serving (and
            # so the name) does not exist yet, which is where --legacy records.
            patch.object(forgepact, "PANEL_DIST", dist, create=True),
        ]
        for p in patches:
            p.start()
        try:
            print(f"port={sandbox.port}", flush=True)
            print(f"cmds={cmds}", flush=True)
            # Serve until whoever started us closes our stdin (or dies).
            sys.stdin.read()
        finally:
            for p in reversed(patches):
                p.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
