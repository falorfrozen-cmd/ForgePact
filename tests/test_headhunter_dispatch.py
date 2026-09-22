"""Execute the actual C++ dispatch functions against controlled game API responses."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


def implementation(source, signature):
    start = source.rfind(signature)
    if start < 0:
        return ''
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f'Unterminated function: {signature}')


class HeadhunterDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source_path = Path(os.environ.get('FORGEPACT_TEST_PLUGIN_SOURCE', ROOT / 'plugin/ModuleMain.cpp'))
        source = source_path.read_text(encoding='utf-8')
        functions = '\n\n'.join(implementation(source, signature) for signature in (
            'static bool HhTakeOnce(',
            'static void HhReleaseClaim(',
            'static CInstance* HhResolveInstance(',
            'static bool HhIsPlayerInstance(',
            'static void HhSteal(',
            # The kill drops, real: the kill hook below calls them.
            'static void SignatureDropOnKill(',
            'static void AngelicDropOnKill(',
            # Headhunter/Tyrant's Crown joining the Angelic pool (#63) - absent pre-#63.
            'static void AppendSignatureCandidates(',
            'static RValue& Hook_EnemyDestroyKillProc(',
            'static RValue& Hook_HhDeathEffects(',
            'static void EnableHeadhunter()',
            'static void HeadhunterActivityTick()',
        ))
        if 'static void EnableHeadhunter()' in source:
            functions = '#define HAS_ENABLE_HEADHUNTER\n' + functions
        if 'static void AppendSignatureCandidates(std::vector<AngelicCandidate>& pool)' in source:
            functions = '#define HAS_APPENDSIGNATURECANDIDATES\n' + functions
        output = ROOT / 'build/headhunter-native-tests'
        output.mkdir(parents=True, exist_ok=True)
        code = (ROOT / 'tests/headhunter_dispatch_harness.cpp').read_text(encoding='utf-8')
        cpp = output / 'dispatch.cpp'
        cpp.write_text(code.replace('// PRODUCTION_FUNCTIONS', functions), encoding='utf-8')
        cls.binary = output / ('dispatch.exe' if os.name == 'nt' else 'dispatch')
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not vswhere.is_file():
                raise unittest.SkipTest('Visual Studio C++ compiler is required for native dispatch tests')
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install:
                raise unittest.SkipTest('Visual Studio C++ toolchain not installed')
            vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
            batch = output / 'compile.cmd'
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{output / "dispatch.obj"}"\nexit /b %errorlevel%\n', encoding='utf-8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler:
                raise unittest.SkipTest('A C++20 compiler is required for native dispatch tests')
            command = [compiler, '-std=c++20', '-O2', str(cpp), '-o', str(cls.binary)]
        # The compiler speaks the machine's locale (/W4 notes included); decode
        # leniently so a localized line cannot crash the test itself.
        result = subprocess.run(command, cwd=output, capture_output=True, text=True, encoding='utf-8', errors='replace')
        (output / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_runtime_scenarios(self):
        for scenario in (
            'missing_player_retry', 'failed_buff_retry', 'exception_retry',
            'projectile_context', 'object_player_reference', 'deduplicate_success',
            'typed_instance_id_killer', 'typed_instance_id_local_fallback',
            'native_lookup_with_broken_sdk_room', 'native_numeric_lookup_with_broken_sdk_room',
            'native_lookup_unavailable', 'native_lookup_wrong_identity',
            'reject_non_enemy', 'disabled', 'player_subclass', 'bounded_cache',
            'capture_before_cleanup', 'player_self_call_shape', 'kill_without_arguments',
            'standalone_fallback_install', 'fallback_without_primary', 'no_trigger_available',
            'death_without_visual_effect', 'both_death_paths', 'death_script_only',
            'automatic_combat_log', 'disabled_combat_log',
        ):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_signature_drop_baseline(self):
        # The kill-order pin: both drops read and spawn while the enemy is live, before the
        # original kill proc, whether that spawn comes from the old standalone die (pre-#63
        # source) or from a forced/pool pick (post-#63 source) - dropsCertain() drives both.
        for scenario in (
            'drops_off_no_spawn', 'drop_skips_non_monster', 'drop_hit_at_enemy_position',
            'angelic_spawns_before_cleanup', 'sigdrop_spawns_before_cleanup',
            'drops_read_nothing_after_original', 'drop_throw_still_calls_original',
        ):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_signature_drop_target(self):
        # Headhunter and Tyrant's Crown join the Angelic/Unholy pool (#63) instead of rolling
        # on their own die; every one of these fails against the pre-#63 source.
        for scenario in (
            'signature_pool_append', 'angelic_pick_crown_spawns_signature',
            'angelic_pick_belt_spawns_signature', 'signature_equal_share',
            'sigdrop_force_belt', 'sigdrop_force_no_alternation',
        ):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
