import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('population_summary',Path(__file__).resolve().parents[1]/'tools/summarize_population_profile.py')
summary=importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)

def row(t,frames,calls,samples,cost,room=1):
    return dict(elapsedUs=t,frames=frames,room=room,queuedPacks=0,queuedCopies=0,windowFrames=80,
        metrics={'protected_get_total':dict(calls=calls,samples=samples,sampledUs=cost)})

class PopulationProfileSummaryTests(unittest.TestCase):
    def test_silent_groups_and_missing_accounting_are_not_completed_spawns(self):
        a,b=row(0,0,0,0,0),row(1000000,20,128,2,4)
        self.assertIsNone(list(summary.intervals([a,b]))[0]['unconfirmedPacks'])
        b['unconfirmedPacks']=71
        result=list(summary.intervals([a,b]))[0]
        self.assertEqual(result['queuedPacks'],0)
        self.assertEqual(result['unconfirmedPacks'],71)
        self.assertIsNone(result['observedNativeBirthPacks'])
        b['observedNativeBirthPacks']=29
        self.assertEqual(list(summary.intervals([a,b]))[0]['observedNativeBirthPacks'],29)

    def test_native_hook_coverage_and_cpu_time_are_not_inferred_from_wall_time(self):
        a,b=row(0,0,0,0,0),row(1000000,20,128,2,4)
        a.update(frameThread=7,frameThreadCpuUs=100,processCpuUs=400)
        b.update(frameThread=7,frameThreadCpuUs=200100,processCpuUs=300400,
                 scriptCoverage={'DrawMinimap':{'table':True,'native':False}})
        result=list(summary.intervals([a,b]))[0]
        self.assertEqual(result['cpu'],{'frameThreadCpuMsPerFrame':10,'processCpuMsPerFrame':15})
        self.assertEqual(result['frameMs'],50)
        self.assertFalse(result['scriptCoverage']['DrawMinimap']['native'])
        b['frameThread']=8
        result=list(summary.intervals([a,b]))[0]
        self.assertNotIn('frameThreadCpuMsPerFrame',result['cpu'])

    def test_unavailable_cpu_time_is_unknown(self):
        result=list(summary.intervals([row(0,0,0,0,0),row(1000000,20,128,2,4)]))[0]
        self.assertEqual(result['cpu'],{})

    def test_sampled_cost_is_estimated_from_interval_not_cumulative_total(self):
        result=list(summary.intervals([row(1000000,10,128,2,4),row(2000000,20,256,4,12)]))[0]
        self.assertAlmostEqual(result['frameMs'],100)
        self.assertAlmostEqual(result['metrics']['protected_get_total']['estimatedMsPerFrame'],0.0512)
        self.assertTrue(result['sameRoom'])

    def test_missing_sample_does_not_claim_zero_time(self):
        result=list(summary.intervals([row(0,0,1,1,3),row(1000000,30,31,1,3)]))[0]
        self.assertEqual(result['metrics'],{})

    def test_zone_change_is_explicit_and_invalid_intervals_are_skipped(self):
        result=list(summary.intervals([row(0,0,0,0,0),row(1000000,30,128,2,4,room=2)]))[0]
        self.assertFalse(result['sameRoom'])
        self.assertEqual(list(summary.intervals([row(1,3,1,1,1),row(1,3,1,1,1)])),[])

    def test_initial_generation_work_before_first_present_is_not_discarded(self):
        first=row(300000,1,1,1,250000)
        first.update(captureStart='map_generation',frameThread=7,frameThreadCpuUs=800000,processCpuUs=900000)
        result=list(summary.intervals([first]))[0]
        self.assertTrue(result['initialInterval'])
        self.assertFalse(result['sameRoom'])
        self.assertEqual(result['cpu'],{})
        self.assertEqual(result['captureStart'],'map_generation')
        self.assertEqual(result['metrics']['protected_get_total']['estimatedMsPerFrame'],250)
        self.assertEqual(result['endSeconds'],0.3)
