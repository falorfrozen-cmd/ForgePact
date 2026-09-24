"""Start a packaged ForgePact.exe and check that it serves the built panel.

    py -3 tools/panel_smoke.py --exe dist/ForgePact/ForgePact.exe [--timeout 60]

The panel's frontend is bundled inside the onefile exe (build_release.py's
--add-data), so "the exe built" says nothing about whether a player sees a
page. This starts the exe, finds the port it bound, and checks four things:

    api      /api/state answers JSON carrying a version
    index    / answers the built index.html (it has <div id="app">)
    assets   the first /assets/*.js the index names answers as text/javascript
    window   a top-level window titled ForgePact belongs to the process tree
             this tool started (pywebview's native window; missing means the
             exe fell back to the browser)

and prints one line:

    window=found url=http://127.0.0.1:8766 index=ok assets=ok api=ok

Exit 0 only when all four are good. It stops exactly the processes it started
(the onefile bootloader and the child it unpacks into), by pid, never by image
name: a ForgePact the player already has open is left alone, and its port is
skipped because it was answering before this tool started anything.

Windows only (the window check and the process tree use the Win32 API).
"""
import argparse
import ctypes
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "forgepact.py"
WINDOW_TITLE = "ForgePact"
# Vite writes /assets/<name>-<hash>.js (or ./assets/... under a relative base).
ASSET_RE = re.compile(r"""["']\.?(/assets/[^"'?#]+\.js)["']""")


def port_candidates() -> list[int]:
    """The ports the panel tries, read from its own source (one site, no copy)."""
    match = re.search(r"^PORT_CANDIDATES = \[([^\]]*)\]", SRC.read_text(encoding="utf-8-sig"), re.M)
    if not match:
        raise SystemExit(f"ERROR: no PORT_CANDIDATES line in {SRC}")
    return [int(p) for p in match.group(1).split(",") if p.strip()]


def fetch(url: str, timeout: float = 3.0):
    """(status, content type, body) or None when nothing answered."""
    try:
        with urllib.request.urlopen(url, timeout=timeout) as response:
            return response.status, response.headers.get("Content-Type", ""), response.read()
    except urllib.error.HTTPError as err:
        return err.code, err.headers.get("Content-Type", ""), err.read()
    except (OSError, ValueError):
        return None


def api_ok(answer) -> bool:
    if not answer or answer[0] != 200:
        return False
    try:
        return "version" in json.loads(answer[2])
    except ValueError:
        return False


def index_ok(answer) -> bool:
    return bool(answer) and answer[0] == 200 and b'<div id="app">' in answer[2]


def first_asset(index_html: bytes):
    match = ASSET_RE.search(index_html.decode("utf-8", "replace"))
    return match.group(1) if match else None


def asset_ok(answer) -> bool:
    return bool(answer) and answer[0] == 200 and answer[1].split(";")[0].strip() == "text/javascript"


def verdict_line(window: bool, url, index: bool, assets: bool, api: bool) -> str:
    good = {True: "ok", False: "bad"}
    return (f"window={'found' if window else 'missing'} url={url or 'none'} "
            f"index={good[index]} assets={good[assets]} api={good[api]}")


# --- Win32: the process tree we started, and the windows it owns -------------

class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wintypes.DWORD), ("szExeFile", ctypes.c_wchar * 260)]


def process_tree(root_pid: int) -> set[int]:
    """root_pid and every descendant alive right now."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    snapshot = kernel32.CreateToolhelp32Snapshot(0x2, 0)  # TH32CS_SNAPPROCESS
    if snapshot in (None, wintypes.HANDLE(-1).value):
        return {root_pid}
    parents = {}
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            parents[entry.th32ProcessID] = entry.th32ParentProcessID
            ok = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    tree, grew = {root_pid}, True
    while grew:
        grew = False
        for pid, parent in parents.items():
            if parent in tree and pid not in tree and pid != parent:
                tree.add(pid)
                grew = True
    return tree


def window_owned_by(pids: set[int]) -> bool:
    """A visible top-level window titled ForgePact whose process is in pids."""
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        buffer = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buffer, 256)
        if buffer.value == WINDOW_TITLE:
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value in pids:
                found.append(hwnd)
                return False
        return True

    user32.EnumWindows(visit, 0)
    return bool(found)


def stop_tree(proc: subprocess.Popen) -> None:
    """Stop the bootloader and its unpacked child, by pid (/T walks the tree)."""
    pids = process_tree(proc.pid)
    subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)], capture_output=True)
    for pid in pids - {proc.pid}:
        # A child the bootloader already lost track of is still ours to stop.
        subprocess.run(["taskkill", "/F", "/PID", str(pid)], capture_output=True)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


def answering(port: int) -> bool:
    return fetch(f"http://127.0.0.1:{port}/api/state", timeout=0.5) is not None


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe", required=True, type=Path, help="the packaged ForgePact.exe")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="seconds to wait for the port and the window (default 60)")
    args = parser.parse_args(argv)
    exe = args.exe.resolve()
    if not exe.is_file():
        print(f"ERROR: {exe} does not exist")
        return 1

    # Ports already answering belong to something else, most likely a panel the
    # player has open; the exe skips them too, so they are never ours.
    busy = {p for p in port_candidates() if answering(p)}
    env = dict(os.environ)
    env.pop("FORGEPACT_PANEL_DIST", None)  # measure the bundled build, not a checkout's
    proc = subprocess.Popen([str(exe)], cwd=str(exe.parent), env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    url, api, index, assets, window = None, False, False, False, False
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline and url is None and proc.poll() is None:
            for port in port_candidates():
                if port in busy:
                    continue
                candidate = f"http://127.0.0.1:{port}"
                if api_ok(fetch(candidate + "/api/state")):
                    url = candidate
                    break
            else:
                time.sleep(0.5)
        if url:
            api = True
            page = fetch(url + "/")
            index = index_ok(page)
            asset = first_asset(page[2]) if page else None
            assets = bool(asset) and asset_ok(fetch(url + asset))
            while time.monotonic() < deadline and proc.poll() is None:
                if window_owned_by(process_tree(proc.pid)):
                    window = True
                    break
                time.sleep(0.5)
    finally:
        stop_tree(proc)

    print(verdict_line(window, url, index, assets, api))
    return 0 if (window and url and index and assets and api) else 1


if __name__ == "__main__":
    sys.exit(main())
