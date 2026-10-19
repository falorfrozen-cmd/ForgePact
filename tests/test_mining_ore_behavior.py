"""Compile the real adapter against fake game calls; never opens Hero Siege."""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MiningOreBehaviorTests(unittest.TestCase):
    def test_adapter_scenarios(self):
        out = ROOT / 'build/mining-ore-behavior'
        out.mkdir(parents=True, exist_ok=True)
        header = ROOT / 'plugin/include/ForgePact/MiningOreMod.hpp'
        # The shared CombatText detour is real in both runs: MiningOreMod.hpp
        # uses it, and the harness drives it through what was installed.
        combat_text = ROOT / 'plugin/include/ForgePact/CombatTextHook.hpp'

        def body(path):
            return '\n'.join(line for line in path.read_text(encoding='utf8').splitlines()
                             if not line.startswith(('#include', '#pragma')))

        if os.environ.get('MINING_ORE_BASELINE'):
            # The vanilla game: both detours pass straight through, no lever does anything.
            production = body(combat_text) + '''
namespace ForgePact::MiningOre {
constexpr int kMaxRolls=10;
int multiplier=1; bool installTried=false, ready=false, unavailable=false, loggedReward=false, loggedFailure=false;
CInstance* activeNode=nullptr; bool inReward=false, inExtraRun=false;
PFUNC_YYGMLScript originalStep=nullptr, originalLoot=nullptr;
int (*rewardMultiplier)(CInstance*)=nullptr; void (*rewardDispatched)(CInstance*,double,double)=nullptr;
std::string installFailure, rollsFailure;
int rolls=1; bool rollsInstallTried=false, rollsReady=false, rollsUnavailable=false;
bool loggedRollPaid=false, loggedRollUnpaid=false, loggedHpReset=false, loggedRollFailed=false;
uint64_t extraRuns=0, extraRunsUnpaid=0, silencedCalls=0, removedTexts=0, textsLeft=0;
const char* (*exceptionType)()=nullptr;
struct SilencedScript { const char* name; const char* hookId; PFUNC_YYGMLScript original; bool native; uint64_t passed, silenced; };
std::array<SilencedScript,4> silencedScripts{};
struct ExperienceCapture { bool counted; bool hasAmount; int64_t amount; };
ExperienceCapture (*experienceCapture)(int,RValue**)=nullptr; void (*experienceCount)(const ExperienceCapture&,const RValue&)=nullptr;
bool experienceInstallTried=false, experienceInstalled=false;
bool InstallExperience(){return false;}
const char* ExperienceRoute(){return "not-installed";}
uint64_t SilencedCalls(){return 0;}
bool Install(){return false;}
void Command(const std::string&){}
void RollsCommand(const std::string&){}
RValue& HookStep(CInstance* s,CInstance* o,RValue& r,int n,RValue** a){return originalStep(s,o,r,n,a);}
RValue& HookLoot(CInstance* s,CInstance* o,RValue& r,int n,RValue** a){return originalLoot(s,o,r,n,a);}
}'''
        else:
            production = body(combat_text) + '\n' + body(header)
        cpp = out / 'mining.cpp'
        cpp.write_text((ROOT / 'tests/mining_ore_harness.cpp').read_text(encoding='utf8')
                       .replace('// PRODUCTION_MINING_ORE', production), encoding='utf8')
        binary = out / ('mining.exe' if os.name == 'nt' else 'mining')
        include = ROOT.parent / 'hs-game-sdk/cpp/include'
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not vswhere.is_file():
                raise unittest.SkipTest('MSVC is needed for native behavior tests')
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                       'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install: raise unittest.SkipTest('MSVC not installed')
            batch = out / 'compile.cmd'
            batch.write_text(f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                             'if errorlevel 1 exit /b 1\n'
                             f'cl /nologo /std:c++20 /EHsc /O2 /DFORGEPACT_RELEASE /I "{include}" "{cpp}" /Fe:"{binary}" /Fo:"{out / "mining.obj"}"\n'
                             'exit /b %errorlevel%\n', encoding='utf8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler: raise unittest.SkipTest('C++20 compiler needed')
            command = [compiler, '-std=c++20', '-O2', '-DFORGEPACT_RELEASE', '-I', str(include), str(cpp), '-o', str(binary)]
        build = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out/'compile.log').write_text(build.stdout+build.stderr, encoding='utf8')
        self.assertEqual(0, build.returncode, build.stdout+build.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        (out/('baseline.log' if os.environ.get('MINING_ORE_BASELINE') else 'run.log')).write_text(run.stdout+run.stderr, encoding='utf8')
        self.assertEqual(0, run.returncode, run.stdout+run.stderr)
        self.assertIn('RESULT OK', run.stdout)
        # ForgePact #185: ExperienceUpdate's one detour is shared with the
        # tracker's XP count, and the extra rolls stay available when the
        # tracker installed it first.
        self.assertIn('PASS target/tracker_first_then_rolls_share_one_native_xp_detour', run.stdout)
        # ForgePact #222: the dig sets fields on the instance CombatText
        # returns, so an extra roll lets the text run and removes it after,
        # and an extra roll that throws never passes the throw to the game.
        for line in ('PASS baseline/rolls1_step_uses_combattext_return',
                     'PASS target/extra_roll_combattext_return_used',
                     'PASS target/extra_roll_texts_removed',
                     'PASS target/extra_run_exception_stops_rolls_no_rethrow',
                     'PASS target/extra_roll_other_object_left_alone',
                     'PASS target/extra_roll_text_object_unknown_destroys_nothing'):
            self.assertIn(line, run.stdout)
            self.assertNotIn(line.replace('PASS ', 'FAIL ', 1), run.stdout)


if __name__ == '__main__':
    unittest.main()
