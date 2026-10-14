#!/usr/bin/env python3
"""Serve the real panel HTTP handler against a throwaway config, for browser tests.

    py -3 tests/panel_sandbox_server.py [--legacy] [--dist <dir>] [--offline]
                                        [--satanic-minimum] [--seed <json file>]
                                        [--src <dir>] [--start-delay <seconds>]
                                        [--tracker-session <file>]
                                        [--tracker-source <dir>]

Used by `panel/tests/` (the behaviour oracle, the screenshot tool and the e2e
suite). It reuses `test_satanic_panel.PanelSandbox` - an isolated
`forgepact.json` in a temp directory, `mod_chain` reporting nothing installed,
a port re-bound off `CHROMIUM_RESTRICTED_PORTS` so the printed port is never
one headless Edge refuses with `net::ERR_UNSAFE_PORT`, and a listen queue of
128 (`_SandboxServer`) so a browser's burst of connections waits instead of
being refused while this process is slow to accept - and re-patches it so
every control's full path can be recorded:

- `game_running` answers True, so `/api/set` takes its live-send branches
  (`--offline` keeps it False, the state the old agent-browser harnesses ran
  in);
- `send_cmds` appends the lines to `<tmp>/cmds.txt` instead of the game's
  `bp_ipc/cmd.txt`, and answers `"<n> command(s) sent"` as the real one does;
- `op_install_mod`, `op_remove_mod`, `launch_modded_game` and
  `open_reports_folder` answer `{"ok": "stubbed <name>"}` - nothing is
  installed, patched, launched or opened in Explorer;
- `pick_exe_dialog` answers the sandbox's own (empty) `Hero_Siege.exe`;
- the tracker (#185) never touches the developer's own data: `forgepact.ROOT`
  (so the tracker's store, `ROOT\\tracker`) is `<tmp>/Hero_Siege`, the session
  folder `tracker_sessions_dir()` answers is `<tmp>/bp_ipc/tracker` whatever
  the seed's `game_exe`, `TRACKER_SOURCE` (HS Offline Tracker's folder, which
  the store imports from on its first start) is a folder that does not exist,
  and `tracker_pid_is_live` answers from the sandbox, never the process list.
  Before this, the first suite that read `/api/tracker` built the service
  under the real `%LOCALAPPDATA%\\Hero_Siege\\tracker` and imported the
  developer's real Tracker data into it.

`--tracker-session <file>` copies a protocol-v1 session file into the session
folder as `session-<pid>-<ms>.ndjson`, byte for byte, and reports that pid as
a running game, so `/api/tracker` reads it as a live session. The pid and ms
come from the file's name when it is already named so, else from its first
record's `process_id` and `emitted_at_unix_ms`
(`tests/fixtures/tracker/session-fixture.ndjson` gives pid 777). A suite
appends records to the copy to drive the panel. `--tracker-source <dir>` is
the HS Offline Tracker folder the store's first start imports from (read
only, as the import always is).

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

`--start-delay <seconds>` waits that long before anything is imported or the
seed is read, so the port is reported at least that much later, on any
machine: `tests/test_start_sandbox_report.py` needs a sandbox that misses
`startSandbox`'s start limit. A start is no longer slow enough to count on:
about 0.5 s since hs_game_sdk loads its tables on first use (hub PR #286), and
0.25 s without the SDK beside the checkout.

Prints `port=<n>`, then `cmds=<path>`, then `tracker=<path>` (the session
file's copy, or nothing after the `=` without `--tracker-session`), and serves
until stdin closes. No route is added to the product for testing: everything
here is a patch on the module the product already runs.
"""
import argparse
import json
import re
import shutil
import sys
import time
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
SDK_PYTHON = ROOT.parent / "hs-game-sdk" / "python"
SESSION_NAME = re.compile(r"^session-(\d+)-(\d+)\.ndjson$")


def session_identity(parser, path):
    """(pid, ms) for a session file: from its name, else its first record."""
    m = SESSION_NAME.match(path.name)
    if m:
        return int(m.group(1)), int(m.group(2))
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as e:
        parser.error(f"--tracker-session: cannot read {path}: {e}")
    for line in lines:
        try:
            record = json.loads(line)
        except ValueError:
            continue
        pid, ms = record.get("process_id"), record.get("emitted_at_unix_ms")
        if isinstance(pid, int) and pid > 0 and isinstance(ms, int) and ms > 0:
            return pid, ms
        break
    parser.error(f"--tracker-session: {path}'s first record names no process_id and emitted_at_unix_ms")


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
    parser.add_argument("--start-delay", type=float, default=0.0, metavar="SECONDS",
                        help="wait this long before importing anything or reading --seed "
                             "(a start that is late on any machine)")
    parser.add_argument("--tracker-session", type=Path, default=None, metavar="FILE",
                        help="copy this protocol-v1 session file into the sandbox's session folder, "
                             "its pid reported live")
    parser.add_argument("--tracker-source", type=Path, default=None, metavar="DIR",
                        help="the HS Offline Tracker folder the tracker store imports from")
    args = parser.parse_args(argv)
    if args.start_delay < 0:
        parser.error("--start-delay: must not be negative")
    if args.tracker_session is not None and not args.tracker_session.is_file():
        parser.error(f"--tracker-session: {args.tracker_session} is not a file")
    if args.tracker_source is not None and not args.tracker_source.is_dir():
        parser.error(f"--tracker-source: {args.tracker_source} is not a folder")
    if args.start_delay:
        # Before load(), where a slow start spent its time, and so before the
        # seed is read (checking its keys needs forgepact.DEFAULTS): a late
        # sandbox left running would find its seed gone, as one once did.
        time.sleep(args.start_delay)
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

        # The tracker's folders, all inside the sandbox (see the docstring).
        store_root = temp / "Hero_Siege"
        store_root.mkdir()
        sessions = temp / "bp_ipc" / "tracker"
        source = args.tracker_source.resolve() if args.tracker_source else temp / "no-hs-offline-tracker"
        live_pids = set()
        tracker_session = ""
        if args.tracker_session is not None:
            pid, ms = session_identity(parser, args.tracker_session)
            sessions.mkdir(parents=True, exist_ok=True)
            copy = sessions / f"session-{pid}-{ms}.ndjson"
            shutil.copyfile(args.tracker_session, copy)
            live_pids.add(pid)
            tracker_session = str(copy)

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
            # create=True for the same reason as PANEL_DIST below: an older
            # tree served with --src has no Incident reports card to click.
            patch.object(forgepact, "open_reports_folder", side_effect=stub("open_reports_folder"), create=True),
            patch.object(forgepact, "pick_exe_dialog", return_value=str(exe)),
            # create=True: this works on a tree where the static serving (and
            # so the name) does not exist yet, which is where --legacy records.
            patch.object(forgepact, "PANEL_DIST", dist, create=True),
            # The tracker never reads or writes the developer's own data.
            # create=True: an older tree served with --src has no tracker.
            patch.object(forgepact, "ROOT", store_root),
            patch.object(forgepact, "TRACKER_SOURCE", source, create=True),
            patch.object(forgepact, "tracker_sessions_dir", lambda: sessions, create=True),
            patch.object(forgepact, "tracker_pid_is_live", lambda pid: pid in live_pids, create=True),
        ]
        for p in patches:
            p.start()
        try:
            print(f"port={sandbox.port}", flush=True)
            print(f"cmds={cmds}", flush=True)
            print(f"tracker={tracker_session}", flush=True)
            # Serve until whoever started us closes our stdin (or dies).
            sys.stdin.read()
        finally:
            for p in reversed(patches):
                p.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
