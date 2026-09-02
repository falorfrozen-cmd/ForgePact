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
        self.assertIn("isCreator && !specialChild", body)
        self.assertIn("!densityAlreadyApplied", body)
        self.assertIn("knownCreator || DensityWindowActive()", body)

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
        self.assertIn('HookOneScript("ZoneStateResetSingle", "fp_density_window_single"', SOURCE)
        reset_start = SOURCE.index("static void ForgetDensityPlacements")
        reset_end = SOURCE.index("static size_t DensityPlacementCount", reset_start)
        reset = SOURCE[reset_start:reset_end]
        self.assertIn("g_DensityKnownPlacements.clear();", reset)
        single_start = SOURCE.index("static RValue& HookZoneStateResetSingleDensityWindow")
        single_end = SOURCE.index("static void InstallDensityLifecycleHooks", single_start)
        single = SOURCE[single_start:single_end]
        self.assertIn("OpenDensityWindow();", single)
        self.assertNotIn("ForgetDensityPlacements();", single)
        self.assertNotIn("g_DensityKnownPlacements.clear();", single)
        density_command = SOURCE[SOURCE.index('lc == "density"'):]
        self.assertIn("InstallDensityLifecycleHooks();", density_command[:1200])

    def test_special_content_creators_are_exempt_from_density(self):
        start = SOURCE.index("static void DoMultiCreate")
        end = SOURCE.index("// --- Yaratim konumu kaydi", start)
        body = SOURCE[start:end]
        self.assertIn("g_SpecialCreateDepth > 0", body)
        self.assertIn("SpecialCreateScope specialScope(ozelIcerik);", body)
        self.assertIn("if (specialChild)", body)
        self.assertIn("!specialChild", body)

    def test_known_creators_remain_active_after_generation_window_closes(self):
        self.assertIn("static bool DensityWindowActive()", SOURCE)
        self.assertIn("kDensityWindowIdleFrames", SOURCE)
        self.assertIn("kKnownDensityCreatorObjects", SOURCE)
        self.assertIn("ResolveKnownCreatorObjects();", SOURCE)
        create_start = SOURCE.index("static void DoMultiCreate")
        create_end = SOURCE.index("// --- Yaratim konumu kaydi", create_start)
        create = SOURCE[create_start:create_end]
        self.assertIn("const bool knownCreator = IsCachedCreatorObject(objIdx);", create)
        self.assertIn("knownCreator || DensityWindowActive()", create)
        for signature in ("static void HookICD", "static void HookICL"):
            start = SOURCE.index(signature)
            end = SOURCE.index("\n}", start) + 2
            body = SOURCE[start:end]
            self.assertIn("IsCachedCreatorObject(objIdx)", body)
            self.assertIn("const bool densityCreate", body)
            self.assertIn("g_ObjMult.empty()", body)
            self.assertIn("ObjectMultiplier(objIdx) <= 1", body)

    def test_creator_cache_uses_names_not_build_specific_indices(self):
        start = SOURCE.index("kKnownDensityCreatorObjects")
        end = SOURCE.index("static bool IsEnemyObject", start)
        block = SOURCE[start:end]
        for name in (
            "Enemy_Creator_obj",
            "Enemy_Creator_Ambush_obj",
            "Enemy_Creator_Ancient_obj",
            "Enemy_Creator_Champion_obj",
            "Enemy_Creator_Colossal_Chest_obj",
            "Enemy_Creator_Legion_obj",
            "Enemy_Creator_Miniboss_obj",
        ):
            self.assertIn(f'"{name}"', block)
        self.assertIn('CallBuiltin("asset_get_index"', block)

    def test_no_global_object_event_observer_was_added(self):
        self.assertNotIn("CreateCallback(Module, EVENT_OBJECT_CALL", SOURCE)


if __name__ == "__main__":
    unittest.main()
