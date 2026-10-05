import os
from pathlib import Path
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
# vcvars64 plus one cl /O2 takes a few seconds on an idle machine, but beside
# other load a comparable harness compile was measured at about 2 minutes, so
# a fixed 90 s over compile and run together failed for reasons that had
# nothing to do with the code (ForgePact #165). The compile gets a wide bound.
COMPILE_TIMEOUT_SECONDS=600
# Each compiled harness finishes in a few seconds; this bound only catches a
# hang, so it stays tight and separate from the compile.
RUN_TIMEOUT_SECONDS=90

class AdaptivePopulationTests(unittest.TestCase):
    def test_density_queue_selection_cost_and_lifecycle(self):
        self.compile_and_run('density-queue-cost',(ROOT/'tests/density_queue_cost.cpp').read_text(encoding='utf-8'),'density queue cost and behavior PASS')

    def test_five_second_population_with_delayed_native_polls(self):
        self.compile_and_run('deadline',(ROOT/'tests/population_deadline.cpp').read_text(encoding='utf-8'),'five-second population PASS')

    def test_production_density_adapter(self):
        from test_map_reveal_behavior import implementation
        source=(ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8')
        keys=source[source.index('struct DensityPlacementKey'):source.index('static void ForgetDensityPlacements()')]
        runtime=source[source.index('struct DensityContext'):source.index('static uint64_t g_KuyrukToplam')]
        harness=(ROOT/'tests/density_population_harness.cpp').read_text(encoding='utf-8')
        harness=harness.replace('// PRODUCTION_DENSITY_KEYS',keys).replace('// PRODUCTION_DENSITY_RUNTIME',runtime).replace('// PRODUCTION_MULTI_CREATE',implementation(source,'static void DoMultiCreate('))
        self.compile_and_run('density-adapter',harness,'RESULT OK')

    def test_production_budget_and_density_lifecycle(self):
        self.compile_and_run('test',(ROOT/'tests/adaptive_population.cpp').read_text(encoding='utf-8'),'due within reach PASS')

    def compile_and_run(self,name,code,expected):
        if os.name!='nt':self.skipTest('MSVC harness runs on Windows')
        vswhere=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        if not vswhere.exists():self.skipTest('MSVC not installed')
        vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
        out=ROOT/'build/adaptive-population';out.mkdir(parents=True,exist_ok=True)
        cpp=out/(name+'.cpp');cpp.write_text(code,encoding='utf-8')
        cmd=out/(name+'.cmd')
        cmd.write_text(f'@echo off\ncall "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{out / (name+".exe")}" /Fo:"{out / (name+".obj")}"\n',encoding='utf-8')
        log=out/(name+'.log')
        build=subprocess.run(['cmd','/d','/c',str(cmd)],capture_output=True,text=True,timeout=COMPILE_TIMEOUT_SECONDS)
        log.write_text(build.stdout+build.stderr,encoding='utf-8')
        self.assertEqual(build.returncode,0,build.stdout+build.stderr)
        run=subprocess.run([str(out/(name+'.exe'))],capture_output=True,text=True,timeout=RUN_TIMEOUT_SECONDS)
        log.write_text(build.stdout+build.stderr+run.stdout+run.stderr,encoding='utf-8')
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        self.assertIn(expected,run.stdout)
