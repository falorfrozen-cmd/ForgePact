import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "plugin" / "ModuleMain.cpp").read_text(encoding="utf-8")


class DensityReentryContractTests(unittest.TestCase):
    def test_creator_placements_use_stable_identity_only(self):
        self.assertIn("struct DensityPlacementKey", SOURCE)
        for field in ("objectIndex;", "x;", "y;"):
            self.assertIn(field, SOURCE)
        block = SOURCE[
            SOURCE.index("struct DensityPlacementKey"):
            SOURCE.index("static bool RememberDensityPlacement")
        ]
        for transient in ("zone;", "plane;", "layerRoute;", "global.room", "runnerRoom"):
            self.assertNotIn(transient, block)

    def test_hot_create_key_does_not_call_back_into_gamemaker(self):
        start = SOURCE.index("static DensityPlacementKey MakeDensityPlacementKey")
        end = SOURCE.index("static bool RememberDensityPlacement", start)
        body = SOURCE[start:end]
        self.assertNotIn("CallBuiltin", body)
        self.assertNotIn("CallGameScript", body)
        self.assertNotIn("GetBuiltin", body)
        self.assertNotIn("GetInstanceMember", body)

    def test_revisit_is_blocked_before_fractional_accumulator_advances(self):
        start = SOURCE.index("static void DoMultiCreate")
        end = SOURCE.index("// --- Yaratim konumu kaydi", start)
        body = SOURCE[start:end]
        guard = body.index("densityAlreadyApplied = true")
        fractional = body.index("g_CreatorFrac += kesir")
        self.assertLess(guard, fractional)
        self.assertIn("isCreator && !densityAlreadyApplied", body)

    def test_generated_copies_are_registered_before_zone_state_can_see_them(self):
        start = SOURCE.index("static void DoMultiCreate")
        end = SOURCE.index("// --- Yaratim konumu kaydi", start)
        body = SOURCE[start:end]
        remember = body.index("RememberDensityPlacement(MakeDensityPlacementKey")
        create = body.index("orig(tmp, S, O, argc, a.data())", remember)
        self.assertLess(remember, create)

    def test_density_guard_does_not_filter_special_content_markers(self):
        self.assertNotIn("g_SpecialKnownPlacements", SOURCE)
        self.assertNotIn("RememberSpecialPlacement", SOURCE)
        start = SOURCE.index("static void DoMultiCreate")
        end = SOURCE.index("// --- Yaratim konumu kaydi", start)
        body = SOURCE[start:end]
        self.assertNotIn("g_SpecialRevisitSkips", body)

    def test_only_full_zone_state_reset_releases_placement_guards(self):
        self.assertIn('HookOneScript("ZoneStateResetAll", "fp_density_reset_all"', SOURCE)
        self.assertNotIn("HookZoneStateResetSingleDensity", SOURCE)
        self.assertIn('HookOneScript("ZoneStateResetSingle", "fp_special_queue_reset_single"', SOURCE)
        single_start = SOURCE.index("static RValue& HookZoneStateResetSingleSpecial")
        single_end = SOURCE.index("static void InstallSpecialLifecycleHook", single_start)
        self.assertNotIn("ForgetDensityPlacements", SOURCE[single_start:single_end])
        reset_start = SOURCE.index("static void ForgetDensityPlacements")
        reset_end = SOURCE.index("static size_t DensityPlacementCount", reset_start)
        reset = SOURCE[reset_start:reset_end]
        self.assertIn("g_DensityKnownPlacements.clear();", reset)
        density_command = SOURCE[SOURCE.index('lc == "density"'):]
        self.assertIn("InstallDensityLifecycleHooks();", density_command[:1200])

    def test_no_global_object_event_observer_was_added(self):
        self.assertNotIn("CreateCallback(Module, EVENT_OBJECT_CALL", SOURCE)


if __name__ == "__main__":
    unittest.main()
