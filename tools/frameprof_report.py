"""Turn a `frameprof` capture into a page a person can read.

    py tools/frameprof_report.py                      # newest capture of the configured game
    py tools/frameprof_report.py path\\to\\frameprof-20260928-120000.json
    py tools/frameprof_report.py --no-html            # summary only

`frameprof start` (the plugin, plugin/include/ForgePact/FrameProfiler.hpp)
writes three files into <game>\\bin\\bp_ipc\\perf: the JSON report, the
collapsed stacks (one line per distinct call stack, outermost frame first, a
sample count last) and the summary text it also prints to out.txt. This tool
prints the summary again in a fuller form and writes <stem>.html beside them:
the frame numbers, where the frame thread's time went, the heaviest events,
scripts and built-ins, the slow frames, a per-second timeline, CPU per thread
and an icicle chart of the stacks. The page is self-contained - no network,
no script - so it can be opened straight from disk or attached to a report.

From the stacks it also splits the frame thread's time into the runner's step
phase, its draw phase and the rest (presenting the frame, the frame limiter):
each phase is found as the outermost runtime function under which nearly all
the object events are that phase's, never from an address. The buckets it
derives from the stacks' labels are checked against the plugin's, and a
"runner phases" line says whether they agree.

Without a path it finds the game the way tools/ipc.ps1 does, from the panel's
own %LOCALAPPDATA%\\Hero_Siege\\forgepact.json, and takes the newest capture.
"""
import argparse
import html
import json
import os
import re
import sys
from pathlib import Path

#: The icicle chart leaves out any frame narrower than this share of all
#: samples; a 7,500-sample capture has thousands of one-sample leaves.
MIN_ICICLE_SHARE = 0.002
ICICLE_MAX_DEPTH = 48


def default_capture() -> Path:
    base = os.environ.get("LOCALAPPDATA")
    if not base:
        raise SystemExit("LOCALAPPDATA is not set; pass the capture's .json path")
    cfg_path = Path(base) / "Hero_Siege" / "forgepact.json"
    if not cfg_path.is_file():
        raise SystemExit(f"{cfg_path} not found - set the game path in the ForgePact panel once, or pass a .json path")
    game_exe = json.loads(cfg_path.read_text(encoding="utf-8")).get("game_exe")
    if not game_exe:
        raise SystemExit("forgepact.json has no game_exe - set the game path in the panel, or pass a .json path")
    perf = Path(game_exe).parent / "bp_ipc" / "perf"
    captures = sorted(perf.glob("frameprof-*.json"), key=lambda p: p.stat().st_mtime)
    if not captures:
        raise SystemExit(f"no frameprof-*.json in {perf} - run `frameprof start` in the game first")
    return captures[-1]


def load(path: Path):
    report = json.loads(path.read_text(encoding="utf-8"))
    if report.get("schema") != "forgepact-frameprof/1":
        raise SystemExit(f"{path} is not a frameprof report (schema {report.get('schema')!r})")
    stacks = []
    stacks_name = report.get("files", {}).get("stacks")
    if stacks_name and (path.parent / stacks_name).is_file():
        stacks = parse_stacks((path.parent / stacks_name).read_text(encoding="utf-8"))
    return report, stacks


def parse_stacks(text: str):
    """[(frames outermost first, count)] from collapsed-stack lines."""
    out = []
    for line in text.splitlines():
        line = line.rstrip()
        if not line:
            continue
        head, _, count = line.rpartition(" ")
        if not head or not count.isdigit():
            continue
        out.append((head.split(";"), int(count)))
    return out


def build_tree(stacks):
    """A call tree: {'name', 'value', 'children': {name: node}}, root first."""
    root = {"name": "all samples", "value": 0, "children": {}}
    for frames, count in stacks:
        root["value"] += count
        node = root
        for frame in frames[:ICICLE_MAX_DEPTH]:
            child = node["children"].get(frame)
            if child is None:
                child = node["children"][frame] = {"name": frame, "value": 0, "children": {}}
            child["value"] += count
            node = child
    return root


def frame_kind(name: str) -> str:
    """How a stacks-file label reads: which colour the chart gives it."""
    if name == "all samples":
        return "root"
    if name.endswith("()"):
        return "builtin"
    if "!" in name or name == "unknown code":
        lowered = name.lower()
        if any(g in lowered for g in ("d3d", "dxgi", "nvwgf", "nvldumd", "amdxx", "atidxx", "igd1", "igdumd", "gameoverlay")):
            return "graphics"
        if lowered.startswith("hero_siege") or lowered.startswith("herosiege"):
            return "runtime"
        if any(m in lowered for m in ("bloodpact", "yytoolkit", "auriecore", "hsafk", "hsoffline")):
            return "mod"
        return "system"
    events = (" Step", " Begin Step", " End Step", " Draw", " Create", " Destroy", " Alarm ", " User Event ",
              " Clean Up", " Collision with ", " Room Start", " Room End", " Other ", " Async ")
    if any(e in name for e in events):
        return "event"
    return "script"


def pct(value: float) -> str:
    return f"{value:.1f}%"


def summary_lines(report):
    cap, frames, threads = report["capture"], report["frames"], report["threads"]
    lines = [
        f"capture: {cap['seconds']:.1f} s, {cap['samples']} samples at {cap['effectiveHz']:.0f}/s"
        + (f" (ended early: {cap['endedEarly']})" if cap.get("endedEarly") else ""),
        f"frames: {frames['count']} at {frames['fps']:.1f} fps; median {frames['msP50']:.1f} ms, "
        f"95% under {frames['msP95']:.1f} ms, worst {frames['msMax']:.1f} ms; "
        f"{frames['over50ms']} over 50 ms, {frames['over100ms']} over 100 ms",
        f"frame thread: working {pct(report['time']['workingPercent'])}, waiting {pct(report['time']['waitingPercent'])}; "
        f"CPU: frame thread {threads['frameThreadPercentOfCore']:.0f}% of one core, "
        f"every other game thread together {threads['otherThreadsPercentOfCore']:.0f}% of one core",
        "time split: " + "; ".join(f"{b['name']} {pct(b['percent'])}" for b in report["buckets"] if b["samples"]),
    ]
    if report.get("spinWait"):
        lines.append(f"frame limiter: spins instead of sleeping, {pct(report['spinWait']['percent'])} of the frame "
                     f"thread's time in {report['spinWait']['function']} (counted as waiting)")
    for key, title in (("events", "heaviest events"), ("gmlTotal", "heaviest game code"),
                       ("gmlSelf", "game code's own time"), ("builtins", "heaviest built-ins")):
        rows = report.get(key) or []
        lines.append(f"{title}: " + (" | ".join(f"{r['name']} {pct(r['percent'])}" for r in rows[:6]) or "(none)"))
    for h in report.get("hitches", [])[:5]:
        top = ", ".join(f"{r['name']} ({r['samples']})" for r in h.get("gmlTotal", [])[:3]) or "no game code"
        buckets = h.get("buckets") or []
        mostly = (f"mostly {buckets[0]['name']} ({buckets[0]['samples']} of {h['samples']} samples); "
                  if buckets else "")
        lines.append(f"slow frame {h['ms']:.0f} ms at {h['atSeconds']:.1f} s"
                     + (f" in {h['room']}" if h.get("room") else "") + f": {mostly}game code: {top}")
    lines.append(f"profiler cost: at most {cap['pauseUsAvg']:.0f} us per sample, "
                 f"at most {cap['pausePercentOfTime']:.2f}% of the frame thread's time"
                 + (f"; sampling slowed to {cap['hzLowest']}/s at times to stay near "
                    f"{cap['pauseBudgetPercent']:.0f}%" if cap.get("rateCuts") else ""))
    return lines


# ---- the runner's phases --------------------------------------------------------
#
# Each stack's bucket is derived again from its labels, mirroring Classify and
# IsSpinSample in plugin/include/ForgePact/FrameProfiler.hpp, so that a phase's
# time can be split the way the plugin splits the whole capture. The plugin
# also sorts modules by their path (\driverstore\, \mods\, \windows\), which a
# stacks file does not keep; the parity line shows any difference that makes.

#: The bucket keys, in the order the plugin writes them.
BUCKET_KEYS = ("game", "game_wait", "graphics", "gpu_wait", "mods", "runtime", "idle", "spin", "unknown")

#: ClassifyModule's kGraphics, kGraphicsPrefix and kMods.
GRAPHICS_MODULES = frozenset((
    "d3d11.dll", "dxgi.dll", "d3d12.dll", "d3d12core.dll", "d3d10warp.dll", "d3d9.dll", "dxcore.dll",
    "d3d11on12.dll", "d3dcompiler_47.dll", "opengl32.dll", "vulkan-1.dll", "gameoverlayrenderer64.dll",
))
GRAPHICS_PREFIXES = (
    "nvwgf2um", "nvldumd", "nvd3dum", "nvoglv", "amdxx", "atidxx", "atiumd", "amdxc", "amdihk",
    "igd10", "igd12", "igdumd", "igdusc", "igc64", "igdgmm", "igxelp",
)
MOD_PREFIXES = ("bloodpactplugin", "yytoolkit", "auriecore", "hsafkexpedition", "hsofflinetracker")

#: Windows' own DLLs, standing in for the plugin's \windows\ path rule: only an
#: export of one of these can be a blocking wait (IsWaitExport).
SYSTEM_MODULES = frozenset((
    "ntdll.dll", "kernelbase.dll", "kernel32.dll", "win32u.dll", "user32.dll", "gdi32.dll", "gdi32full.dll",
    "combase.dll", "rpcrt4.dll", "sechost.dll", "advapi32.dll", "ucrtbase.dll", "msvcrt.dll", "msvcp_win.dll",
    "ole32.dll", "oleaut32.dll", "shell32.dll", "shlwapi.dll", "ws2_32.dll", "mswsock.dll", "winmm.dll",
    "dsound.dll", "xaudio2_9.dll", "xinput1_4.dll", "dinput8.dll", "hid.dll", "setupapi.dll", "cfgmgr32.dll",
    "bcrypt.dll", "bcryptprimitives.dll", "crypt32.dll", "uxtheme.dll", "dwmapi.dll", "imm32.dll", "msctf.dll",
    "powrprof.dll", "mmdevapi.dll", "audioses.dll", "avrt.dll", "version.dll", "wininet.dll", "winhttp.dll",
    "iphlpapi.dll", "dnsapi.dll", "nsi.dll", "sspicli.dll", "kernel.appcore.dll", "windows.storage.dll",
    "coremessaging.dll", "textinputframework.dll",
))
WAIT_EXPORT_WORDS = ("Wait", "Delay", "YieldExecution", "RemoveIoCompletion")
CLOCK_READS = frozenset(("RtlQueryPerformanceCounter", "QueryPerformanceCounter"))
NO_FRAMES = "(no frames)"

#: The runner's event kinds by phase, as GmlEventName writes them. Every
#: other event kind (Create, User Event, Room Start, ...) is in neither.
STEP_EVENTS = frozenset(("Begin Step", "Step", "End Step"))
STEP_EVENT_PATTERN = re.compile(r"Alarm \d+|Collision with .+")
DRAW_EVENTS = frozenset(("Draw", "Draw Begin", "Draw End", "Draw GUI", "Draw GUI Begin", "Draw GUI End",
                         "Pre-Draw", "Post-Draw"))
DRAW_EVENT_PATTERN = re.compile(r"Draw \d+")

#: A runtime function is a phase's dispatcher when at least this share of the
#: object events beneath it are that phase's ...
DISPATCHER_PURITY_PERCENT = 95
#: ... and those events are at least this share of all samples.
DISPATCHER_MIN_PERCENT = 1
#: The callees listed under each dispatcher.
DISPATCHER_CALLEES = 8


def _module_and_name(label: str):
    """('module.dll' lowercased, 'Export' or '0x<rva>') for a module frame, else None."""
    module, bang, name = label.partition("!")
    return (module.lower(), name) if bang else None


def is_game_code(label: str) -> bool:
    """A game script or object event: no module, not a built-in, not unknown."""
    return "!" not in label and label not in ("unknown code", NO_FRAMES) and not label.endswith("()")


def _frame_class(label: str):
    """'game', 'graphics' or 'mod' for a frame that decides a bucket; None for
    one that passes through (runtime, built-ins, system, unknown code)."""
    if is_game_code(label):
        return "game"
    parts = _module_and_name(label)
    if not parts:
        return None
    module = parts[0]
    if module in GRAPHICS_MODULES or module.startswith(GRAPHICS_PREFIXES):
        return "graphics"
    if module.startswith(MOD_PREFIXES):
        return "mod"
    return None


def _is_export(name: str) -> bool:
    return not re.fullmatch(r"0x[0-9A-Fa-f]+", name)


def _is_wait(label: str) -> bool:
    parts = _module_and_name(label)
    if not parts or parts[0] not in SYSTEM_MODULES or not _is_export(parts[1]):
        return False
    return any(word in parts[1] for word in WAIT_EXPORT_WORDS)


def _is_clock_read(label: str) -> bool:
    parts = _module_and_name(label)
    return bool(parts) and parts[1] in CLOCK_READS


def runtime_only(frames) -> bool:
    """No game code, graphics driver or mod anywhere on the stack."""
    return all(_frame_class(f) is None for f in frames)


def stack_bucket(frames, spin_label) -> str:
    """The plugin's bucket key for one stack (outermost frame first), from its
    labels; `spin_label` is the capture's spinWait.function, or None."""
    if not frames or frames == [NO_FRAMES]:
        return "unknown"
    leaf = frames[-1]
    if spin_label and runtime_only(frames):
        if leaf == spin_label or (len(frames) >= 2 and _is_clock_read(leaf) and frames[-2] == spin_label):
            return "spin"
    wait = _is_wait(leaf)
    for label in reversed(frames):
        kind = _frame_class(label)
        if kind == "graphics":
            return "gpu_wait" if wait else "graphics"
        if kind == "game":
            return "game_wait" if wait else "game"
        if kind == "mod":
            return "mods"
    return "idle" if wait else "runtime"


def _spin_label(report):
    return (report.get("spinWait") or {}).get("function")


def derived_buckets(report, stacks):
    """{bucket key: samples} summed over the stacks' own buckets."""
    spin = _spin_label(report)
    totals = dict.fromkeys(BUCKET_KEYS, 0)
    for frames, count in stacks:
        key = stack_bucket(frames, spin)
        totals[key] = totals.get(key, 0) + count
    return totals


def parity_line(report, stacks) -> str:
    derived = derived_buckets(report, stacks)
    capture = {b["key"]: b["samples"] for b in report.get("buckets", [])}
    keys = list(BUCKET_KEYS) + [k for k in capture if k not in BUCKET_KEYS]
    differ = [(k, derived.get(k, 0), capture.get(k, 0)) for k in keys if derived.get(k, 0) != capture.get(k, 0)]
    if not differ:
        return "runner phases: buckets agree with the capture"
    by = sum(abs(a - b) for _, a, b in differ)
    return (f"runner phases: buckets differ from the capture by {by} samples ("
            + ", ".join(f"{k} tool {a} capture {b}" for k, a, b in differ) + ")")


def event_phase(label: str):
    """'step' or 'draw' for an object event of that phase, 'other' for any
    other object event, None for a label that is not an object event."""
    if not is_game_code(label) or " " not in label or label.endswith(" (script file)"):
        return None
    event = label.split(" ", 1)[1]
    if event in STEP_EVENTS or STEP_EVENT_PATTERN.fullmatch(event):
        return "step"
    if event in DRAW_EVENTS or DRAW_EVENT_PATTERN.fullmatch(event):
        return "draw"
    return "other"


def _new_node(label, parent):
    return {"label": label, "parent": parent, "events": {"step": 0, "draw": 0, "other": 0},
            "rows": [], "children": {}}


def runner_phases(report, stacks):
    """The runner's step and draw dispatchers on the stacks, and what lies
    outside both.

    Each stack's runtime side is its frames before the first game-code frame
    (all of them when it has none), and its event is its outermost game-code
    frame's, when that is an object event. A runtime-side node is phase P's
    dispatcher when at least DISPATCHER_PURITY_PERCENT of the event samples
    beneath it are P's, those are at least DISPATCHER_MIN_PERCENT of all
    samples, and no node above it qualifies for P.
    """
    spin = _spin_label(report)
    rows = []
    root = _new_node("all samples", None)
    total = 0
    for frames, count in stacks:
        bucket = stack_bucket(frames, spin)
        rows.append((frames, count, bucket, runtime_only(frames)))
        total += count
        if bucket == "unknown":
            continue
        first_game = next((i for i, f in enumerate(frames) if is_game_code(f)), len(frames))
        phase = event_phase(frames[first_game]) if first_game < len(frames) else None
        node = root
        for depth, label in enumerate(frames[:first_game]):
            child = node["children"].get(label)
            if child is None:
                child = node["children"][label] = _new_node(label, node)
                child["depth"] = depth + 1
            child["rows"].append(len(rows) - 1)
            if phase:
                child["events"][phase] += count
            node = child

    def qualifies(node, phase):
        kind = node["events"][phase]
        events = sum(node["events"].values())
        return (kind * 100 >= DISPATCHER_PURITY_PERCENT * events
                and kind * 100 >= DISPATCHER_MIN_PERCENT * total and kind > 0)

    def find(node, phase, out):
        for child in sorted(node["children"].values(), key=lambda c: (-sum(rows[i][1] for i in c["rows"]), c["label"])):
            if qualifies(child, phase):
                out.append(child)
            else:
                find(child, phase, out)
        return out

    def split(indices):
        buckets = dict.fromkeys(BUCKET_KEYS, 0)
        for i in indices:
            buckets[rows[i][2]] += rows[i][1]
        return buckets

    result = {"total": total, "step": [], "draw": []}
    # A stack under both a step and a draw dispatcher (one phase's handful of
    # events below the other's) is inside the phases once, not twice.
    inside = set()
    for phase in ("step", "draw"):
        for node in find(root, phase, []):
            callees = {}
            for i in node["rows"]:
                frames, count, _, only = rows[i]
                if len(frames) > node["depth"]:
                    c = callees.setdefault(frames[node["depth"]], [0, 0])
                    c[0] += count
                    c[1] += count if only else 0
            buckets = split(node["rows"])
            inside.update(node["rows"])
            result[phase].append({
                "label": node["label"],
                "parent": node["parent"]["label"] if node["parent"] is not root else "(none)",
                "samples": sum(buckets.values()),
                "buckets": buckets,
                "callees": [{"label": label, "samples": c[0], "runtimeOnly": c[1]}
                            for label, c in sorted(callees.items(), key=lambda kv: (-kv[1][0], kv[0]))
                            ][:DISPATCHER_CALLEES],
            })
    outside = split(i for i in range(len(rows)) if i not in inside)
    result["outside"] = {"samples": sum(outside.values()), "buckets": outside}
    return result


def _share(value, total) -> str:
    return pct(100.0 * value / total if total else 0.0)


def _bucket_split(buckets, total) -> str:
    parts = ", ".join(f"{k} {_share(v, total)}" for k, v in buckets.items() if v)
    return f" ({parts})" if parts else ""


def phase_lines(report, stacks):
    """The `runner phases:` block printed after the summary lines. `stacks` is
    None (or empty) when the capture has no stacks file."""
    if not stacks:
        return ["runner phases: not found (no stacks file)"]
    found = runner_phases(report, stacks)
    total = found["total"]
    if not found["step"] and not found["draw"]:
        return ["runner phases: not found (no step or draw dispatcher on the stacks)", parity_line(report, stacks)]
    lines = ["runner phases:"]
    for phase in ("step", "draw"):
        if not found[phase]:
            lines.append(f"  {phase} phase: not found")
        for d in found[phase]:
            lines.append(f"  {phase} phase: {d['label']}, called from {d['parent']}: "
                         f"{_share(d['samples'], total)} of samples{_bucket_split(d['buckets'], total)}")
            for c in d["callees"]:
                lines.append(f"    {c['label']} {_share(c['samples'], total)}, "
                             f"runtime only {_share(c['runtimeOnly'], total)}")
    outside = found["outside"]
    lines.append(f"  outside the phases: {_share(outside['samples'], total)} of samples"
                 f"{_bucket_split(outside['buckets'], total)}")
    lines.append(parity_line(report, stacks))
    return lines


# ---- the page -----------------------------------------------------------------

STYLE = """
:root{--bg:#f7f7f5;--panel:#fff;--ink:#1c1d1f;--muted:#5f636b;--line:#e2e2de;--bar:#3b6fd8;
--event:#3b6fd8;--script:#6a8ee6;--builtin:#d08a2e;--runtime:#8a8f98;--system:#b5b8bf;--graphics:#3aa37a;
--mod:#b0569b;--root:#4a4d55;--warn:#c2410c}
@media (prefers-color-scheme:dark){:root{--bg:#141518;--panel:#1c1e22;--ink:#e8e9ec;--muted:#9aa0aa;
--line:#2c2f35;--bar:#6b95ef;--event:#5b8def;--script:#88a7f0;--builtin:#e0a04a;--runtime:#6f747d;
--system:#50545c;--graphics:#4cc08f;--mod:#cc72b7;--root:#80848d;--warn:#fb923c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);
font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:1180px;margin:0 auto;padding:24px 16px 48px}
h1{font-size:22px;margin:0 0 4px}h2{font-size:17px;margin:32px 0 10px}
.sub{color:var(--muted);margin:0 0 20px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px}
.card b{display:block;font-size:22px;font-variant-numeric:tabular-nums}.card span{color:var(--muted);font-size:13px}
.split{display:flex;height:26px;border-radius:6px;overflow:hidden;border:1px solid var(--line)}
.split div{height:100%}.legend{display:flex;flex-wrap:wrap;gap:6px 16px;margin-top:8px;font-size:13px}
.legend i{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:6px}
table{width:100%;border-collapse:collapse;background:var(--panel);border:1px solid var(--line);border-radius:8px;
overflow:hidden;font-size:14px}th,td{padding:6px 10px;text-align:left;border-bottom:1px solid var(--line)}
th{color:var(--muted);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.03em}
td.num{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap}
td.name{word-break:break-word}.bar{height:8px;background:var(--bar);border-radius:4px;min-width:1px}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:16px}
.scroll{overflow-x:auto}svg text{fill:var(--ink);font:11px system-ui,sans-serif}
.icicle{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:6px}
.note{color:var(--muted);font-size:13px}.warn{color:var(--warn)}
"""

BUCKET_COLORS = {
    "game": "var(--event)", "game_wait": "var(--script)", "graphics": "var(--graphics)",
    "gpu_wait": "#8fd3b6", "mods": "var(--mod)", "runtime": "var(--runtime)", "idle": "var(--system)",
    "spin": "#d9c7a0", "unknown": "var(--line)",
}


def esc(value) -> str:
    return html.escape(str(value), quote=True)


def table(rows, title, note=""):
    if not rows:
        return f"<h2>{esc(title)}</h2><p class='note'>Nothing sampled here.</p>"
    top = max(r["percent"] for r in rows) or 1
    body = "".join(
        f"<tr><td class='name'>{esc(r['name'])}</td><td class='num'>{r['percent']:.1f}%</td>"
        f"<td style='width:28%'><div class='bar' style='width:{100 * r['percent'] / top:.1f}%'></div></td>"
        f"<td class='num'>{r['samples']}</td></tr>"
        for r in rows[:25])
    return (f"<div><h2>{esc(title)}</h2>" + (f"<p class='note'>{esc(note)}</p>" if note else "")
            + "<table><tr><th>Name</th><th>Share</th><th></th><th>Samples</th></tr>" + body + "</table></div>")


def timeline_svg(timeline):
    if not timeline:
        return "<p class='note'>No per-second data.</p>"
    width, height, pad = 1100, 220, 34
    n = len(timeline)
    ms_max = max(max((s["msMax"] for s in timeline), default=0), 34.0)
    monsters = [s.get("monsters", -1) for s in timeline]
    mon_max = max(max(monsters), 1)

    def x(i):
        return pad + (width - 2 * pad) * (i / max(n - 1, 1))

    def y_ms(v):
        return height - pad - (height - 2 * pad) * min(v / ms_max, 1.0)

    def y_mon(v):
        return height - pad - (height - 2 * pad) * (v / mon_max)

    parts = [f"<svg viewBox='0 0 {width} {height}' width='100%' role='img' aria-label='Frame time per second'>"]
    for ref in (16.7, 33.3):
        if ref < ms_max:
            parts.append(f"<line x1='{pad}' x2='{width - pad}' y1='{y_ms(ref):.1f}' y2='{y_ms(ref):.1f}' "
                         f"stroke='var(--line)' stroke-dasharray='4 4'/>"
                         f"<text x='{width - pad + 4}' y='{y_ms(ref) + 4:.1f}'>{ref:.0f} ms</text>")
    worst = " ".join(f"{x(i):.1f},{y_ms(s['msMax']):.1f}" for i, s in enumerate(timeline))
    mean = " ".join(f"{x(i):.1f},{y_ms(s['msMean']):.1f}" for i, s in enumerate(timeline))
    parts.append(f"<polyline fill='none' stroke='var(--warn)' stroke-width='1.5' points='{worst}'/>")
    parts.append(f"<polyline fill='none' stroke='var(--bar)' stroke-width='2' points='{mean}'/>")
    if any(m >= 0 for m in monsters):
        # Only the seconds that carried a reading: a second without one is
        # unknown, not zero monsters.
        pts = " ".join(f"{x(i):.1f},{y_mon(m):.1f}" for i, m in enumerate(monsters) if m >= 0)
        parts.append(f"<polyline fill='none' stroke='var(--graphics)' stroke-width='1.5' stroke-dasharray='2 3' points='{pts}'/>")
    parts.append(f"<text x='{pad}' y='14'>frame time (blue: mean, orange: worst per second; top = {ms_max:.0f} ms)"
                 + (f" · monsters alive (green dashes; top = {mon_max})" if any(m >= 0 for m in monsters) else "")
                 + "</text>")
    step = max(1, n // 12)
    for i in range(0, n, step):
        parts.append(f"<text x='{x(i):.1f}' y='{height - 10}' text-anchor='middle'>{timeline[i]['second']} s</text>")
    parts.append("</svg>")
    return "".join(parts)


def icicle_svg(root):
    total = root["value"]
    if not total:
        return "<p class='note'>No stacks were recorded.</p>"
    width, row = 1100, 18
    rects = []
    depth_seen = [0]

    def walk(node, x0, depth):
        w = width * node["value"] / total
        if node["value"] / total < MIN_ICICLE_SHARE:
            return
        depth_seen[0] = max(depth_seen[0], depth)
        kind = frame_kind(node["name"])
        share = 100 * node["value"] / total
        label = node["name"] if w > 60 else ""
        max_chars = int(w / 6.2)
        if label and len(label) > max_chars:
            label = label[:max(max_chars - 1, 0)] + "…"
        rects.append(
            f"<g><title>{esc(node['name'])} - {share:.1f}% ({node['value']} samples)</title>"
            f"<rect x='{x0:.2f}' y='{depth * row}' width='{max(w - 0.6, 0.4):.2f}' height='{row - 1}' "
            f"fill='var(--{kind})' rx='2'/>"
            + (f"<text x='{x0 + 3:.2f}' y='{depth * row + 12.5}'>{esc(label)}</text>" if label else "")
            + "</g>")
        cx = x0
        for child in sorted(node["children"].values(), key=lambda c: -c["value"]):
            walk(child, cx, depth + 1)
            cx += width * child["value"] / total

    walk(root, 0.0, 0)
    height = (depth_seen[0] + 1) * row
    return (f"<div class='icicle scroll'><svg viewBox='0 0 {width} {height}' width='100%' "
            f"style='min-width:900px' role='img' aria-label='Call stacks'>" + "".join(rects) + "</svg></div>")


def phases_html(report, stacks):
    """The page's "Runner phases" section: phase_lines() as tables."""
    head = ("<h2>Runner phases</h2><p class='note'>The runner's step phase (Begin Step, Step, End Step, alarms, "
            "collisions) and its draw phase, each found on the stacks as the outermost runtime function under "
            f"which at least {DISPATCHER_PURITY_PERCENT}% of the object events are that phase's. Shares are of all "
            "samples; runtime only means no game code, graphics driver or mod anywhere on the stack.</p>")
    if not stacks:
        return head + "<p class='note'>Runner phases: not found (no stacks file).</p>"
    found = runner_phases(report, stacks)
    total = found["total"]
    names = {b["key"]: b["name"] for b in report.get("buckets", [])}

    def split(buckets):
        return ", ".join(f"{names.get(k, k)} {_share(v, total)}" for k, v in buckets.items() if v) or "nothing"

    parts = [head]
    if not found["step"] and not found["draw"]:
        parts.append("<p class='note'>Runner phases: not found (no step or draw dispatcher on the stacks).</p>")
    else:
        for phase in ("step", "draw"):
            if not found[phase]:
                parts.append(f"<p class='note'>{phase.capitalize()} phase: not found.</p>")
            for d in found[phase]:
                rows = "".join(
                    f"<tr><td class='name'>{esc(c['label'])}</td><td class='num'>{_share(c['samples'], total)}</td>"
                    f"<td class='num'>{_share(c['runtimeOnly'], total)}</td></tr>" for c in d["callees"])
                parts.append(
                    f"<p><b>{phase.capitalize()} phase</b>: <code>{esc(d['label'])}</code>, called from "
                    f"<code>{esc(d['parent'])}</code> - {_share(d['samples'], total)} of samples "
                    f"({esc(split(d['buckets']))})</p>"
                    "<table><tr><th>Calls</th><th>Share</th><th>Runtime only</th></tr>" + rows + "</table>")
        outside = found["outside"]
        parts.append(f"<p><b>Outside the phases</b>: {_share(outside['samples'], total)} of samples "
                     f"({esc(split(outside['buckets']))})</p>")
    parity = parity_line(report, stacks)
    agree = parity.endswith("agree with the capture")
    parts.append(f"<p class='{'note' if agree else 'warn'}'>{esc(parity[0].upper() + parity[1:])}.</p>")
    return "".join(parts)


def build_html(report, stacks, title="Frame profile"):
    cap, frames, threads = report["capture"], report["frames"], report["threads"]
    cards = [
        (f"{frames['fps']:.1f}", "frames per second"),
        (f"{frames['msP50']:.1f} ms", "median frame"),
        (f"{frames['msP95']:.1f} ms", "95% of frames faster than"),
        (f"{frames['msMax']:.0f} ms", "worst frame"),
        (str(frames["over50ms"]), "frames over 50 ms"),
        (pct(report["time"]["workingPercent"]), "frame thread working"),
        (f"{threads['frameThreadPercentOfCore']:.0f}%", "frame thread, % of one core"),
        (f"{threads['otherThreadsPercentOfCore']:.0f}%", "all other threads, % of one core"),
    ]
    split = "".join(
        f"<div title='{esc(b['name'])} {b['percent']:.1f}%' style='width:{b['percent']:.3f}%;"
        f"background:{BUCKET_COLORS.get(b['key'], 'var(--line)')}'></div>"
        for b in report["buckets"] if b["percent"] > 0)
    legend = "".join(
        f"<span><i style='background:{BUCKET_COLORS.get(b['key'], 'var(--line)')}'></i>{esc(b['name'])} "
        f"{b['percent']:.1f}%</span>" for b in report["buckets"] if b["percent"] > 0)
    hitches = report.get("hitches") or []
    hitch_rows = "".join(
        f"<tr><td class='num'>{h['atSeconds']:.1f} s</td><td class='num'>{h['ms']:.0f} ms</td>"
        f"<td>{esc(h.get('room', ''))}</td><td class='name'>"
        + esc(", ".join(f"{b['name']} {b['samples']}" for b in (h.get("buckets") or [])[:3]) or "-")
        + "</td><td class='name'>"
        + esc(", ".join(f"{r['name']} ({r['samples']})" for r in h.get("gmlTotal", [])[:4]) or "no game code")
        + "</td></tr>" for h in hitches)
    thread_rows = "".join(
        f"<tr><td class='num'>{t['id']}</td><td>{esc(t['name'] or '')}"
        + (" <b>(frame thread)</b>" if t["frameThread"] else "") + (" (this profiler)" if t["profiler"] else "")
        + f"</td><td class='num'>{t['percentOfCore']:.1f}%</td></tr>" for t in threads["top"] if t["percentOfCore"] >= 0.1)
    ended = f"<p class='warn'>Ended early: {esc(cap['endedEarly'])}</p>" if cap.get("endedEarly") else ""
    body = [
        f"<h1>{esc(title)}</h1>",
        f"<p class='sub'>{esc(report.get('tool', ''))} · finished {esc(report.get('finished', ''))} · "
        f"{cap['seconds']:.1f} s sampled, {cap['samples']} samples ({cap['effectiveHz']:.0f}/s) · "
        f"{report['game'].get('gmlNamed', 0)} game functions and {report['game'].get('builtinsNamed', 0)} built-ins named</p>",
        ended,
        "<div class='cards'>" + "".join(f"<div class='card'><b>{esc(v)}</b><span>{esc(k)}</span></div>" for v, k in cards) + "</div>",
        "<h2>Where the frame thread's time goes</h2>",
        f"<div class='split'>{split}</div><div class='legend'>{legend}</div>",
        "<p class='note'>Working: game code, the GameMaker runtime, the graphics driver and mods. Waiting: the frame "
        "limiter (sleeping, or spinning on the clock), the GPU or display (Present), and game code blocked on files "
        "or locks.</p>"
        + (f"<p class='note'>The frame limiter spins instead of sleeping: {report['spinWait']['percent']:.1f}% of the "
           f"frame thread's time, in <code>{esc(report['spinWait']['function'])}</code>. That time is free for more "
           "game work; it only keeps a CPU core busy.</p>" if report.get("spinWait") else ""),
        "<div class='grid2'>",
        table(report.get("events"), "Heaviest events", "Each object event with everything it calls."),
        table(report.get("gmlTotal"), "Heaviest game code", "Scripts and events, each with everything it calls."),
        table(report.get("gmlSelf"), "Game code's own time",
              "Time in each function's own code and the runtime calls it makes directly, not in other game code."),
        table(report.get("builtins"), "Heaviest built-ins", "GameMaker built-in functions, with what they call."),
        table(report.get("leafFunctions"), "Innermost functions", "Where the CPU was when the sample was taken."),
        table(report.get("leafModules"), "By module", "Which program file the CPU was in."),
        "</div>",
        "<h2>Per second</h2>", timeline_svg(report.get("timeline") or []),
        "<h2>Slowest frames</h2>",
        ("<table><tr><th>At</th><th>Frame</th><th>Room</th><th>Where the time went (samples)</th>"
         "<th>Game code in it (samples)</th></tr>" + hitch_rows + "</table>")
        if hitches else "<p class='note'>No frame took longer than 50 ms.</p>",
        "<h2>Call stacks</h2>",
        "<p class='note'>Outermost call at the top, the functions it called below it; width is share of samples. "
        "Hover a box for its name. Blue: game events and scripts, orange: built-ins, grey: runtime and system, "
        "green: graphics, purple: mods.</p>",
        icicle_svg(build_tree(stacks)),
        phases_html(report, stacks),
        "<h2>CPU per thread</h2>",
        "<table><tr><th>Thread</th><th>Name</th><th>% of one core</th></tr>" + thread_rows + "</table>",
        f"<p class='note'>Profiler cost: at most {cap['pauseUsAvg']:.0f} µs per sample, at most "
        f"{cap['pausePercentOfTime']:.2f}% of the frame thread's time"
        + (f"; sampling slowed to {cap['hzLowest']}/s at times to stay near {cap['pauseBudgetPercent']:.0f}%"
           if cap.get("rateCuts") else "") + ".</p>",
    ]
    return ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            f"<title>{esc(title)}</title><style>{STYLE}</style></head><body><main>"
            + "".join(body) + "</main></body></html>")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("capture", nargs="?", help="a frameprof-*.json report (default: the newest one)")
    parser.add_argument("--html", help="where to write the page (default: <capture>.html)")
    parser.add_argument("--no-html", action="store_true", help="print the summary only")
    args = parser.parse_args(argv)
    path = Path(args.capture) if args.capture else default_capture()
    report, stacks = load(path)
    for line in summary_lines(report) + phase_lines(report, stacks):
        print(line)
    if not args.no_html:
        out = Path(args.html) if args.html else path.with_suffix(".html")
        out.write_text(build_html(report, stacks, f"Frame profile - {path.stem}"), encoding="utf-8")
        print(f"page: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
