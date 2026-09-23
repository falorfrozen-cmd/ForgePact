#pragma once

// Included after HookOneScript and the game's script ABI are available.
// No raw object events, RVAs, or gameplay calls are generated here. Every
// wrapper forwards the original call exactly once, even outside the capture.
#ifdef FORGEPACT_POPULATION_PROFILE
namespace ForgePact::PopulationProfile {
#define FP_POP_SCRIPT_LIST(X) \
    X(EnemyStepHandleNew, EnemyStep) \
    X(EnemyParentBeginStepMain, EnemyBeginStep) \
    X(EnemyChildStepGlobalTimers, EnemyGlobalTimers) \
    X(EnemyChildStepEffectTimers, EnemyEffectTimers) \
    X(PathFindStep, PathStep) \
    X(PathFindControllerStep, PathController) \
    X(PathFindSeparateTick, PathSeparate) \
    X(PathFindRepathToSocket, PathRepath) \
    X(PathFindAggroBroadcast, PathBroadcast) \
    X(DrawMinimap, MinimapDraw) \
    X(DrawMinimapDynamic, MinimapDynamicDraw) \
    X(DrawEnemyPack, EnemyPackDraw) \
    X(DrawEnemyHealthBars, EnemyHealthDraw) \
    X(PopulatePresetData, PresetData) \
    X(ZoneGenGenerateKeyPresets, ZoneKeyPresets) \
    X(ZoneGenGenerateHeatMap, ZoneHeatMap) \
    X(ZoneGenPopulatePresetObjects, ZonePresetObjects) \
    X(ZoneGenPopulateGroundPresets, ZoneGroundPresets) \
    X(ZoneGenMakeZoneWalls, ZoneWalls) \
    X(tilemap_autotile, ZoneAutotile) \
    X(PathFindBuildStaticBlockers, ZoneStaticBlockers) \
    X(ZoneStateLoad, ZoneStateLoad)

#define FP_POP_SCRIPT_WRAPPER(name, metric) \
    static PFUNC_YYGMLScript original_##name = nullptr; \
    static bool native_##name = false; \
    static RValue& Hook_##name(CInstance* self, CInstance* other, RValue& result, int argc, RValue** args) { \
        if constexpr (Metric::metric == Metric::PresetData) Instance().Begin(90000000, StartPoint::MapGeneration); \
        FP_POP_SCOPE(metric); \
        return original_##name ? original_##name(self, other, result, argc, args) : result; \
    }
FP_POP_SCRIPT_LIST(FP_POP_SCRIPT_WRAPPER)
#undef FP_POP_SCRIPT_WRAPPER

inline void InstallScriptTimings() {
    static bool attempted=false;
    if(attempted)return;
    attempted=true;
#define FP_POP_INSTALL(name, metric) \
    HookOneScript(#name, "fp_pop_" #name, reinterpret_cast<PVOID>(Hook_##name), &original_##name, &native_##name);
    FP_POP_SCRIPT_LIST(FP_POP_INSTALL)
#undef FP_POP_INSTALL
}

// Persist coverage separately from samples: a missing/direct-call-blind hook
// is not a function proved cheap just because its counter stayed at zero.
inline void WriteScriptCoverage(std::ostream& out) {
    bool first=true;
    out << '{';
#define FP_POP_COVERAGE(name, metric) \
    if(!first)out << ',';first=false; \
    out << '\"' << #name << "\":{\"table\":" << (original_##name ? "true":"false") \
        << ",\"native\":" << (native_##name ? "true":"false") << '}';
    FP_POP_SCRIPT_LIST(FP_POP_COVERAGE)
#undef FP_POP_COVERAGE
    out << '}';
}
#undef FP_POP_SCRIPT_LIST
}
#endif
