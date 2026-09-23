from pathlib import Path
import unittest
import test_adaptive_population as compiler
from test_map_reveal_behavior import implementation

ROOT=Path(__file__).resolve().parents[1]

class PopulationBirthTests(unittest.TestCase):
    compile_and_run=compiler.AdaptivePopulationTests.compile_and_run

    def test_real_observer_accepts_only_successful_native_enemy_births(self):
        source=(ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8-sig')
        harness=(ROOT/'tests/population_birth_harness.cpp').read_text(encoding='utf-8')
        harness=harness.replace('// PRODUCTION_CALLER_INFO',implementation(source,'struct CreationCallerInfo')+';')
        harness=harness.replace('// PRODUCTION_BIRTH_SCOPE',implementation(source,'struct PopulationBirthScope')+';')
        self.compile_and_run('population-birth',harness,'population native birth observation PASS')

    def test_both_existing_create_hooks_report_each_successful_path(self):
        source=(ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8-sig')
        for signature in ('static void HookICD(', 'static void HookICL('):
            hook=implementation(source,signature)
            self.assertIn('PopulationBirthScope populationBirth(Result,S,argc,Args,callerInfo);',hook)
            self.assertEqual(hook.count('populationBirth.Completed();'),3)

    def test_shared_caller_removes_duplicate_reads_and_preserves_nested_chains(self):
        source=(ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8-sig')
        harness=(ROOT/'tests/creation_caller_harness.cpp').read_text(encoding='utf-8')
        for marker,signature in [('CALLER_INFO','struct CreationCallerInfo'),('BIRTH_SCOPE','struct PopulationBirthScope'),('ENEMY_SCOPE','struct EnemyBornScope')]:
            harness=harness.replace('// PRODUCTION_'+marker,implementation(source,signature)+';')
        harness=harness.replace('// MAKE_SCOPES','CreationCallerInfo info(caller); PopulationBirthScope observation(result,caller,4,args,info); EnemyBornScope born(result,caller,4,args,info);')
        harness=harness.replace('// EXPECT_READ_COUNT','check(objectReads==1000,"one caller lookup per native birth");')
        self.compile_and_run('creation-caller-shared',harness,'creation caller forwarding PASS')
