"""HS Offline Launcher engine embedded in ForgePact.

The standalone UI, configuration, server and polling loop are not started.
Launch uses the path explicitly supplied by ForgePact. Native validation and
Steam helpers come from the MIT-licensed HS-Offline-Launcher revision below.
The orchestration accepts a plugin preflight and owns a single launch lock.
"""
# MIT License
#
# Copyright (c) 2026 falorfrozen-cmd
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from __future__ import annotations

import ctypes
import os
import subprocess
import threading
import time
from ctypes import wintypes
from pathlib import Path
from typing import Callable

UPSTREAM_REVISION = "59108803f776e7dcbd9488b21b0428ea26de5617"
# Compare these helpers to the source launcher when updating the embedded copy.
UPSTREAM_DEFINITIONS = ('registry_steam_roots', 'unique_paths', 'steam_exe', 'PROCESSENTRY32W', 'SERVICE_STATUS_PROCESS', 'matching_processes', 'windows_service_state', 'eac_service_status', 'eac_is_inactive', '_exe_facts', 'pe_section_names', 'find_steam_runtime', 'validate_game', 'start_steam_if_needed', 'launch_safety_blocker')

APP_ID = "269210"


STEAM_RUNTIME_NAME = "steam_api64.dll"


PE_MACHINE_AMD64 = 0x8664


MAX_PE_OFFSET = 16 * 1024 * 1024


INACTIVE_EAC_STATES = frozenset({"stopped", "not-installed"})


EAC_SERVICE_NAME = "EasyAntiCheat_EOS"


EAC_PROCESS_NAMES = frozenset({
    "easyanticheat.exe",
    "easyanticheat_eos.exe",
    "easyanticheat_eossys.exe",
    "start_protected_game.exe",
})


SC_MANAGER_CONNECT = 0x0001


SERVICE_QUERY_STATUS = 0x0004


SC_STATUS_PROCESS_INFO = 0


SERVICE_STOPPED = 1


SERVICE_RUNNING = 4


ERROR_CALL_NOT_IMPLEMENTED = 120


ERROR_SERVICE_DOES_NOT_EXIST = 1060


EXE_FACTS_CACHE: dict[tuple[str, int, int], dict] = {}


EXE_FACTS_LOCK = threading.Lock()


def registry_steam_roots() -> list[Path]:
    roots: list[Path] = []
    try:
        import winreg

        for hive, key_name in (
            (winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam"),
        ):
            try:
                with winreg.OpenKey(hive, key_name) as key:
                    for value_name in ("SteamPath", "InstallPath"):
                        try:
                            value, _ = winreg.QueryValueEx(key, value_name)
                            roots.append(Path(value))
                        except OSError:
                            continue
            except OSError:
                continue
    except ImportError:
        pass
    roots.append(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Steam")
    return unique_paths(roots)


def unique_paths(paths: list[Path]) -> list[Path]:
    seen: set[str] = set()
    result: list[Path] = []
    for path in paths:
        key = str(path).lower()
        if key not in seen:
            seen.add(key)
            result.append(path)
    return result


def steam_exe() -> Path | None:
    for root in registry_steam_roots():
        candidate = root / "steam.exe"
        if candidate.is_file():
            return candidate
    return None


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class SERVICE_STATUS_PROCESS(ctypes.Structure):
    _fields_ = [
        ("dwServiceType", wintypes.DWORD),
        ("dwCurrentState", wintypes.DWORD),
        ("dwControlsAccepted", wintypes.DWORD),
        ("dwWin32ExitCode", wintypes.DWORD),
        ("dwServiceSpecificExitCode", wintypes.DWORD),
        ("dwCheckPoint", wintypes.DWORD),
        ("dwWaitHint", wintypes.DWORD),
        ("dwProcessId", wintypes.DWORD),
        ("dwServiceFlags", wintypes.DWORD),
    ]


def processes() -> list[tuple[int, str]]:
    if os.name != "nt":
        return []
    # Use a private DLL wrapper: ForgePact also scans processes on another
    # thread; ctypes.windll prototypes are mutable process-wide state.
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    invalid_handle = wintypes.HANDLE(-1).value
    if snapshot in (None, invalid_handle):
        raise OSError("Windows process snapshot could not be created")
    rows: list[tuple[int, str]] = []
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    try:
        ok = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        if not ok:
            raise OSError("Windows process snapshot could not be read")
        while ok:
            rows.append((int(entry.th32ProcessID), entry.szExeFile))
            ok = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return rows


def matching_processes(*names: str) -> list[tuple[int, str]]:
    wanted = {name.lower() for name in names}
    return [(pid, name) for pid, name in processes() if name.lower() in wanted]


def windows_service_state(service_name: str) -> tuple[int | None, int]:
    """Return a locale-independent SCM state and the Win32 error code."""
    if os.name != "nt":
        return None, ERROR_CALL_NOT_IMPLEMENTED

    try:
        advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
        advapi32.OpenSCManagerW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD]
        advapi32.OpenSCManagerW.restype = wintypes.HANDLE
        advapi32.OpenServiceW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.DWORD]
        advapi32.OpenServiceW.restype = wintypes.HANDLE
        advapi32.QueryServiceStatusEx.argtypes = [
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.LPBYTE,
            wintypes.DWORD,
            wintypes.LPDWORD,
        ]
        advapi32.QueryServiceStatusEx.restype = wintypes.BOOL
        advapi32.CloseServiceHandle.argtypes = [wintypes.HANDLE]
        advapi32.CloseServiceHandle.restype = wintypes.BOOL
    except (AttributeError, OSError):
        return None, ERROR_CALL_NOT_IMPLEMENTED

    manager = advapi32.OpenSCManagerW(None, None, SC_MANAGER_CONNECT)
    if not manager:
        return None, ctypes.get_last_error()
    try:
        service = advapi32.OpenServiceW(manager, service_name, SERVICE_QUERY_STATUS)
        if not service:
            return None, ctypes.get_last_error()
        try:
            status = SERVICE_STATUS_PROCESS()
            bytes_needed = wintypes.DWORD()
            buffer = ctypes.cast(ctypes.byref(status), wintypes.LPBYTE)
            if not advapi32.QueryServiceStatusEx(
                service,
                SC_STATUS_PROCESS_INFO,
                buffer,
                ctypes.sizeof(status),
                ctypes.byref(bytes_needed),
            ):
                return None, ctypes.get_last_error()
            return int(status.dwCurrentState), 0
        finally:
            advapi32.CloseServiceHandle(service)
    finally:
        advapi32.CloseServiceHandle(manager)


def eac_service_status() -> str:
    state, error = windows_service_state(EAC_SERVICE_NAME)
    if state == SERVICE_STOPPED:
        return "stopped"
    if state == SERVICE_RUNNING:
        return "running"
    if state is not None:
        return "transitioning"
    if error == ERROR_SERVICE_DOES_NOT_EXIST:
        return "not-installed"
    return "unknown"


def eac_is_inactive(status: str) -> bool:
    return status in INACTIVE_EAC_STATES


def _exe_facts(path: Path, use_cache: bool = True) -> dict:
    """One stat() and one PE header parse for the selected executable.

    game_details() used to parse the header twice per status poll - once
    inside validate_game() and once again for the .aurie check - and the UI
    polled every two seconds for the whole session.

    None of these facts can change while the game is running, which is what
    makes a cache safe; the key is (path, size, mtime_ns), the same shape
    file_sha256 already uses, so a changed file is a different key. The stat()
    that produces the key stays: it is metadata-only and O(1) whatever the file
    size, and it is not what costs.

    The lock is here because handler threads share this
    (ThreadingHTTPServer, daemon_threads). The allowed race is two threads
    parsing the same header at once - redundant, identical. What is NOT
    allowed is a launch decision served from a cache, and that is prevented
    structurally rather than by locking harder: launch_game_locked() passes
    use_cache=False, and the process/EAC safety gate is never cached at all.
    """
    stat = path.stat()
    key = (str(path).lower(), stat.st_size, stat.st_mtime_ns)
    if use_cache:
        with EXE_FACTS_LOCK:
            cached = EXE_FACTS_CACHE.get(key)
        if cached is not None:
            return cached
    facts: dict = {"size": stat.st_size, "sections": set(), "error": None}
    try:
        facts["sections"] = pe_section_names(path)
    except ValueError as exc:
        facts["error"] = ("invalid", str(exc))
    except OSError as exc:
        facts["error"] = ("unreadable", str(exc))
    if use_cache:
        with EXE_FACTS_LOCK:
            EXE_FACTS_CACHE.clear()          # size 1, for the same reason as HASH_CACHE
            EXE_FACTS_CACHE[key] = facts
    return facts


def pe_section_names(path: Path) -> set[str]:
    """Read enough PE metadata to identify a real 64-bit Windows executable."""
    size = path.stat().st_size
    with path.open("rb") as stream:
        dos_header = stream.read(64)
        if len(dos_header) != 64 or dos_header[:2] != b"MZ":
            raise ValueError("missing DOS header")
        pe_offset = int.from_bytes(dos_header[0x3C:0x40], "little")
        if pe_offset < 64 or pe_offset > MAX_PE_OFFSET or pe_offset + 24 > size:
            raise ValueError("invalid PE header offset")
        stream.seek(pe_offset)
        pe_header = stream.read(24)
        if len(pe_header) != 24 or pe_header[:4] != b"PE\0\0":
            raise ValueError("missing PE signature")
        machine = int.from_bytes(pe_header[4:6], "little")
        if machine != PE_MACHINE_AMD64:
            raise ValueError("the executable is not 64-bit")
        section_count = int.from_bytes(pe_header[6:8], "little")
        optional_header_size = int.from_bytes(pe_header[20:22], "little")
        characteristics = int.from_bytes(pe_header[22:24], "little")
        if not 1 <= section_count <= 96:
            raise ValueError("invalid PE section count")
        if characteristics & 0x0002 == 0:
            raise ValueError("PE image is not executable")
        if optional_header_size < 2:
            raise ValueError("missing PE optional header")
        stream.seek(pe_offset + 24)
        if int.from_bytes(stream.read(2), "little") != 0x020B:
            raise ValueError("the executable is not PE32+")
        section_table = pe_offset + 24 + optional_header_size
        if section_table + section_count * 40 > size:
            raise ValueError("truncated PE section table")
        stream.seek(section_table)
        result: set[str] = set()
        has_file_data = False
        for _ in range(section_count):
            section = stream.read(40)
            name = section[:8].split(b"\0", 1)[0].decode("ascii", errors="ignore").lower()
            if name:
                result.add(name)
            raw_size = int.from_bytes(section[16:20], "little")
            raw_offset = int.from_bytes(section[20:24], "little")
            if raw_size and raw_offset and raw_offset + raw_size <= size:
                has_file_data = True
        if not has_file_data:
            raise ValueError("PE sections contain no file data")
        return result


def find_steam_runtime(path: Path) -> Path | None:
    """Support both current bin-local and older game-root Steam layouts."""
    for directory in unique_paths([path.parent, path.parent.parent]):
        candidate = directory / STEAM_RUNTIME_NAME
        if candidate.is_file():
            return candidate
    return None


def validate_game(path: Path | None, use_cache: bool = True) -> tuple[bool, str]:
    """Is this a Hero Siege executable we are willing to start?

    use_cache=False is not an optimisation switch: the launch path passes it so
    that the decision which actually starts a process is always made against
    the file as it is right now, never against a status poll's memory of it.
    """
    if path is None or not path.is_file():
        return False, "Hero_Siege.exe was not found"
    if path.name.lower() != "hero_siege.exe":
        return False, "Select the clean Hero_Siege.exe, not a modded or protected launcher"
    try:
        facts = _exe_facts(path, use_cache=use_cache)
    except OSError as exc:
        return False, f"The executable could not be read: {exc}"
    if facts["error"]:
        kind, detail = facts["error"]
        if kind == "invalid":
            return False, f"The selected file is not a valid Hero Siege Windows executable ({detail})"
        return False, f"The executable could not be read: {detail}"
    if not find_steam_runtime(path):
        return False, "steam_api64.dll is missing. Verify Hero Siege files in Steam, then try again."
    if ".aurie" in facts["sections"]:
        return True, "Compatible Aurie/ForgePact build — offline use only"
    return True, "Clean Steam executable"


def start_steam_if_needed() -> tuple[bool, str]:
    try:
        if matching_processes("steam.exe"):
            return True, "Steam is running"
    except OSError:
        return False, "Running processes could not be verified"
    exe = steam_exe()
    if not exe:
        return False, "Steam was not found"
    try:
        subprocess.Popen([str(exe), "-silent"], cwd=str(exe.parent))
    except OSError as exc:
        return False, f"Steam could not be started: {exc}"
    for _ in range(20):
        time.sleep(0.5)
        try:
            if matching_processes("steam.exe"):
                return True, "Steam started"
        except OSError:
            return False, "Running processes could not be verified"
    return False, "Steam did not become ready"


def launch_safety_blocker() -> str:
    try:
        rows = processes()
    except OSError:
        return "Running processes could not be verified, so launch was blocked"
    if any(name.lower() == "hero_siege.exe" for _, name in rows):
        return "Hero Siege is already running"

    service = eac_service_status()
    active_eac = [(pid, name) for pid, name in rows if name.lower() in EAC_PROCESS_NAMES]
    if eac_is_inactive(service) and not active_eac:
        return ""
    if service == "unknown":
        return "EAC status could not be verified, so launch was blocked"
    if service == "transitioning":
        return "EAC is changing state. Wait a moment, then try again"
    return (
        "EAC is currently active. Close the protected game/session first; "
        "this launcher will not stop or modify EAC."
    )


LAUNCH_LOCK = threading.Lock()
_STATE_LOCK = threading.Lock()
_STATE = {"phase": "idle", "message": "HS Offline Launcher is built in. No separate installation needed.",
          "pid": 0, "attempt": 0}


def launch_status() -> dict:
    """Cached result for the panel poll; never scans processes or files."""
    with _STATE_LOCK:
        return dict(_STATE)


def _report(attempt: int, phase: str, message: str, pid: int = 0) -> None:
    with _STATE_LOCK:
        if _STATE["attempt"] == attempt:
            _STATE.update(phase=phase, message=message, pid=pid)


def verify_launch(pid: int, attempt: int) -> None:
    """One delayed check, not a permanent watcher; never closes any process."""
    time.sleep(5)
    try:
        rows = processes()
        running = any(p == pid and name.lower() == "hero_siege.exe" for p, name in rows)
        service = eac_service_status()
        protected = any(name.lower() in EAC_PROCESS_NAMES for _, name in rows)
        if not running:
            phase, message = "error", "The game exited during startup. Check your Steam session and try again."
        elif eac_is_inactive(service) and not protected:
            phase, message = "verified", "Game process verified; EAC is inactive. Use offline characters."
        else:
            phase, message = "warning", "Game started, but inactive protection could not be confirmed."
    except OSError:
        phase, message = "warning", "Game was started, but its process could not be verified."
    _report(attempt, phase, message, pid)


def launch_game(path: Path, *, validate_extra: Callable[[], str] | None = None,
                prepare: Callable[[], object] | None = None) -> dict:
    """Launch exactly the supplied game, using HS Offline Launcher's checks.

    All native/process calls are replaceable in tests. No game or Steam process
    is started at import, during status polling, or before validation succeeds.
    """
    if not LAUNCH_LOCK.acquire(blocking=False):
        return {"err": "A launch request is already in progress", "launch": launch_status()}
    with _STATE_LOCK:
        attempt = _STATE["attempt"] + 1
        _STATE.update(phase="starting", message="Checking the game and Steam...", pid=0, attempt=attempt)

    def fail(message: str) -> dict:
        _report(attempt, "error", message)
        return {"err": message, "launch": launch_status()}

    def check_target() -> str:
        valid, reason = validate_game(path, use_cache=False)
        if not valid:
            return reason
        return validate_extra() if validate_extra else ""

    try:
        if os.name != "nt":
            return fail("Offline launch is supported on Windows only.")
        path = Path(path).resolve()
        reason = check_target() or launch_safety_blocker()
        if reason:
            return fail(reason)
        _report(attempt, "starting", "Preparing Steam for offline launch...")
        steam_ok, message = start_steam_if_needed()
        if not steam_ok:
            return fail(message)
        # Steam startup can take several seconds. Re-read the file and plugin
        # state before touching the optional cache and recheck protection after it.
        reason = check_target() or launch_safety_blocker()
        if reason:
            return fail(reason)
        if prepare:
            prepare()
        reason = check_target() or launch_safety_blocker()
        if reason:
            return fail(reason)
        runtime = find_steam_runtime(path)
        if runtime is None:
            return fail("steam_api64.dll is missing. Verify Hero Siege files in Steam, then try again.")
        child_env = os.environ.copy()
        child_env["SteamAppId"] = APP_ID
        child_env["SteamGameId"] = APP_ID
        if runtime.parent != path.parent:
            child_env["PATH"] = str(runtime.parent) + os.pathsep + child_env.get("PATH", "")
        process = subprocess.Popen([str(path)], cwd=str(path.parent), env=child_env)
        message = "Modded Hero Siege launch requested through the built-in HS Offline Launcher."
        _report(attempt, "started", message, process.pid)
        try:
            threading.Thread(target=verify_launch, args=(process.pid, attempt), daemon=True,
                             name="forgepact-launch-check").start()
        except RuntimeError:
            _report(attempt, "warning", "Game started; the startup check could not be scheduled.", process.pid)
        return {"ok": message, "pid": process.pid, "launch": launch_status()}
    except Exception as exc:
        return fail(f"Offline launch failed: {exc}")
    finally:
        LAUNCH_LOCK.release()
