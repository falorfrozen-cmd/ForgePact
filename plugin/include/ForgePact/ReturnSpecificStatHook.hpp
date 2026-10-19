#pragma once
#include <hs_game_sdk/scripts.hpp>

// The plugin's one detour on ReturnSpecificStat (the game's stat dispatcher:
// `ReturnSpecificStat(1, <stat id>, 0)` answers one stat for its `self`),
// shared by skill sliders' Projectile Speed (SkillSlidersMod.hpp adds to stat
// 75 inside the player's own LoadAllModifiers) and the Mark 1 set's half walk
// (ModuleMain.cpp's Mark 1 adapter answers the local player's stat 64 calls
// through Mark1Movement.hpp while the set's bonus is active).
//
// One detour, because HookOneScript puts the inline detour in only while the
// script-table entry is still the game's code: a second install of the same
// script from another function finds this module's hook there and comes up
// table-only, which the game's direct compiled calls bypass (CombatTextHook.hpp
// is the same arrangement for CombatText). So whichever mod asks first
// installs it, and every later caller gets the remembered answer. A script
// that was not found is asked again on the next request, as skill sliders'
// own install did; one that went in, natively or not, is never installed
// twice.
//
// The game's answer comes first; then each consumer that registered runs, in
// a fixed order: skill sliders, then Mark 1. They answer different stat ids,
// so neither sees the other's change. A consumer is registered by its mod on
// its own arming path, so while neither mod is armed nothing is installed, and
// once installed, a consumer whose mod is off returns at once.
//
// It includes nothing that pulls in <windows.h> and stays portable C++,
// because tests/skill_sliders_harness.cpp compiles it with fake game types.
namespace ForgePact::ReturnSpecificStatHook {
// A consumer: the call's self, its arguments and the answer the caller will
// read, which it may replace.
using Consumer = void (*)(CInstance* S, int argc, RValue** A, RValue& result);

inline PFUNC_YYGMLScript original = nullptr;
inline bool installTried = false, installed = false, native = false;
inline Consumer skillSliders = nullptr;   // SkillSlidersMod's Projectile Speed (stat 75)
inline Consumer mark1 = nullptr;          // the Mark 1 set's half walk (stat 64)

inline RValue& Hook(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    RValue& r = original ? original(S, O, R, argc, A) : R;
    if (skillSliders) skillSliders(S, argc, A, r);
    if (mark1) mark1(S, argc, A, r);
    return r;
}

// True when the detour is in and native. The one HookOneScript call for
// ReturnSpecificStat in the player build; hook id `fp_ss_stat`, as skill
// sliders' own was.
inline bool Install() {
    if (!original) {
        installTried = true;
        installed = HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnSpecificStat), "fp_ss_stat",
            (void*)Hook, &original, &native);
    }
    return original && native;
}
} // namespace ForgePact::ReturnSpecificStatHook
