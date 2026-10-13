"""The tracker's journal writer and the game's exit, without a game.

HS Offline Tracker's producer owned a writer thread, and every graceful exit
with it loaded ended in ucrtbase!abort with a crash dump (HS-Offline-Tracker
#201): ExitProcess ended the thread and then ran the DLL's static destructors,
which found it still joinable. ForgePact's tracker has no thread and no
exit-time code: the core lives on the heap and is never deleted, and the
session file's writer (ForgePact::TrackerJournalFile, TrackerMod.hpp) is a
plain global with no destructor, so whatever is still buffered at exit is
lost and nothing runs.

tests/tracker_mod_exit_probe.cpp is a DLL that holds that writer open with
one batch appended and more records buffered; tests/tracker_mod_exit_harness.cpp
runs each case in a child process and ends it the way the game ends.

Target: exit 0, and the session file holds only whole lines, each one JSON,
exactly the batch the probe appended. Positive control: the same probe with a
static std::thread left running exits with the harness's abort code (0x7E2),
which proves the harness sees an exit that aborts. The harness turns an abort
into that code before Windows Error Reporting sees it, so no case writes a
crash dump. Both are built with /MD, as the plugin is. Writes only under
build/tracker-mod-exit-tests.
"""
import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/tracker-mod-exit-tests'

EXIT_CLEAN = 0
EXIT_ABORT = 0x7E2
CASE_TIMEOUT_SECONDS = 60


def find_msvc():
    finder = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not finder.exists():
        return None
    install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    return install or None


class TrackerModExitBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != 'nt':
            raise unittest.SkipTest('the exit under test is the Windows loader\'s')
        install = find_msvc()
        if not install:
            raise unittest.SkipTest('MSVC not installed')
        OUTPUT.mkdir(parents=True, exist_ok=True)
        include = ROOT / 'plugin/include'
        cls.probe = OUTPUT / 'tracker_mod_exit_probe.dll'
        cls.harness = OUTPUT / 'tracker_mod_exit_harness.exe'
        flags = '/nologo /std:c++20 /EHsc /O2 /W4 /MD'
        script = OUTPUT / 'compile.cmd'
        script.write_text(
            f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl {flags} /LD /I "{include}" "{ROOT / "tests/tracker_mod_exit_probe.cpp"}" '
            f'/Fe:"{cls.probe}" /Fo:"{OUTPUT / "tracker_mod_exit_probe.obj"}"\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl {flags} "{ROOT / "tests/tracker_mod_exit_harness.cpp"}" '
            f'/Fe:"{cls.harness}" /Fo:"{OUTPUT / "tracker_mod_exit_harness.obj"}"\n'
            'exit /b %errorlevel%\n')
        result = subprocess.run(['cmd', '/d', '/c', str(script)], cwd=OUTPUT, capture_output=True, text=True,
                                errors='replace')
        (OUTPUT / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode != 0:
            raise AssertionError('compile failed:\n' + result.stdout + result.stderr)

    def run_case(self, name):
        directory = OUTPUT / f'{name}-bp_ipc'
        shutil.rmtree(directory, ignore_errors=True)
        directory.mkdir(parents=True)
        result = subprocess.run([str(self.harness), '--case', name, str(self.probe), str(directory)],
                                capture_output=True, text=True, errors='replace', timeout=CASE_TIMEOUT_SECONDS)
        (OUTPUT / f'{name}.log').write_text(f'exit 0x{result.returncode:X}\n' + result.stdout + result.stderr,
                                            encoding='utf-8')
        return result, directory

    def describe(self, result):
        return f'exit 0x{result.returncode:X}\n{result.stdout}{result.stderr}'

    # Target: the writer held open with lines buffered, then ExitProcess.
    def test_target_the_exit_is_clean_and_the_file_holds_only_whole_lines(self):
        result, directory = self.run_case('exit')
        self.assertEqual(EXIT_CLEAN, result.returncode, self.describe(result))
        appended = int(result.stdout.split('appended=')[1].split()[0])
        files = sorted((directory / 'tracker').glob('session-*.ndjson'))
        self.assertEqual(1, len(files), files)
        data = files[0].read_bytes()
        self.assertTrue(data.endswith(b'\n'), data[-200:])
        lines = data.decode('utf-8').split('\n')[:-1]
        # Exactly the appended batch: the records still buffered at the exit
        # are lost, and nothing half-written reached the file.
        self.assertEqual(appended, len(lines))
        records = [json.loads(line) for line in lines]
        self.assertEqual([r['sequence'] for r in records], list(range(1, appended + 1)))
        self.assertEqual({r['kind'] for r in records}, {'bridge_status', 'room', 'progress', 'session_delta', 'heartbeat'})

    # Positive control: a static std::thread still running at ExitProcess
    # aborts the exit, and this harness reports it as 0x7E2. A harness that
    # could not see it would pass the target without proving anything.
    def test_control_a_static_std_thread_aborts_the_exit(self):
        result, _ = self.run_case('control')
        self.assertEqual(EXIT_ABORT, result.returncode, self.describe(result))


if __name__ == '__main__':
    unittest.main()
