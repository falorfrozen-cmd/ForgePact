"""The coop receive thread and the game's exit, without a game.

`coopstart` (research build only) starts ModuleMain.cpp's coop receive thread,
and only `coopstop` ends it. The game ends through ExitProcess, which terminates
every other thread and then runs this DLL's static destructors, so a static
std::thread that was still running there is still joinable, and destroying it
calls std::terminate: the exit ends in ucrtbase!abort (0xc0000409, fast-fail 7)
with a crash dump, as the HS-Offline-Tracker producer's exits did. The thread is
now owned by a ForgePact::ExitSafeThread (plugin/include/ForgePact/
ExitSafeThread.hpp), which has no destructor.

tests/coop_thread_exit_probe.cpp is a DLL that owns a receive thread in the old
shape or the new one, and tests/coop_thread_exit_harness.cpp runs each case in
a child process and ends it the way the game ends. The harness turns an abort
into exit code 0x7E2 before Windows Error Reporting sees it, so no case writes
a crash dump. Both are built with /MD, as the plugin is; the harness checks the
probe shares its CRT before anything can abort. Skips without MSVC.

Writes only under build/coop-thread-exit-tests."""
import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/coop-thread-exit-tests'

EXIT_CLEAN = 0
EXIT_TERMINATE = 0x7E1
EXIT_ABORT = 0x7E2
CASE_TIMEOUT_SECONDS = 60


def find_msvc():
    finder = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not finder.exists():
        return None
    install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    return install or None


class CoopThreadExitBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != 'nt':
            raise unittest.SkipTest('the exit under test is the Windows loader\'s')
        install = find_msvc()
        if not install:
            raise unittest.SkipTest('MSVC not installed')
        OUTPUT.mkdir(parents=True, exist_ok=True)
        include = ROOT / 'plugin/include'
        cls.probe = OUTPUT / 'coop_thread_exit_probe.dll'
        cls.harness = OUTPUT / 'coop_thread_exit_harness.exe'
        flags = '/nologo /std:c++20 /EHsc /O2 /W4 /MD'
        script = OUTPUT / 'compile.cmd'
        script.write_text(
            f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl {flags} /LD /I "{include}" "{ROOT / "tests/coop_thread_exit_probe.cpp"}" '
            f'/Fe:"{cls.probe}" /Fo:"{OUTPUT / "coop_thread_exit_probe.obj"}" /link ws2_32.lib\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl {flags} "{ROOT / "tests/coop_thread_exit_harness.cpp"}" '
            f'/Fe:"{cls.harness}" /Fo:"{OUTPUT / "coop_thread_exit_harness.obj"}" /link ws2_32.lib\n'
            'exit /b %errorlevel%\n')
        result = subprocess.run(['cmd', '/d', '/c', str(script)], cwd=OUTPUT, capture_output=True, text=True,
                                errors='replace')
        (OUTPUT / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode != 0:
            raise AssertionError('compile failed:\n' + result.stdout + result.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.harness), '--case', name, str(self.probe)],
                                capture_output=True, text=True, errors='replace', timeout=CASE_TIMEOUT_SECONDS)
        (OUTPUT / f'{name}.log').write_text(f'exit 0x{result.returncode:X}\n' + result.stdout + result.stderr,
                                            encoding='utf-8')
        return result

    def describe(self, result):
        return f'exit 0x{result.returncode:X}\n{result.stdout}{result.stderr}'

    # Baseline: the old g_CoopRecvThread, a static std::thread still running at
    # ExitProcess. It is also the control for the target below: a harness that
    # could not see this abort would pass the target without proving anything.
    def test_baseline_a_static_std_thread_aborts_the_exit(self):
        result = self.run_case('exit-before-fix')
        self.assertIn(result.returncode, (EXIT_ABORT, EXIT_TERMINATE), self.describe(result))

    # Target: the same thread, owned by ForgePact::ExitSafeThread as
    # ModuleMain.cpp now owns it, still waiting in recvfrom at ExitProcess.
    def test_target_the_exit_safe_holder_lets_the_exit_finish(self):
        result = self.run_case('exit')
        self.assertEqual(EXIT_CLEAN, result.returncode, self.describe(result))

    def test_coopstop_joins_the_thread_and_coopstart_can_start_again(self):
        result = self.run_case('stop')
        self.assertEqual(EXIT_CLEAN, result.returncode, self.describe(result))

    def test_a_thread_coopstop_gives_up_on_is_kept_and_the_exit_stays_clean(self):
        result = self.run_case('stop-stalled')
        self.assertEqual(EXIT_CLEAN, result.returncode, self.describe(result))

    def test_a_second_coopstop_waits_for_a_slow_thread_and_frees_the_holder(self):
        result = self.run_case('stop-stalled-retry')
        self.assertEqual(EXIT_CLEAN, result.returncode, self.describe(result))


if __name__ == '__main__':
    unittest.main()
