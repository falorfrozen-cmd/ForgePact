#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <hs_game_sdk/scripts.hpp>

// The plugin's one detour on CombatText (the game's floating text), shared by
// the Experience slider (StatsManager.hpp rescales the "N XP" text) and Mining
// Ore Extra Rolls (MiningOreMod.hpp silences it during an extra roll).
//
// One detour, because HookOneScript puts the inline detour in only while the
// script-table entry is still the game's code: a second install of the same
// script from another function finds this module's hook there and comes up
// table-only, which the dig's direct compiled calls bypass. So whichever mod
// asks first installs it, at most once a session, and every later caller gets
// the remembered answer.
//
// Included by StatsManager.hpp, so it precedes MiningOreMod.hpp. It includes
// nothing that pulls in <windows.h> and stays portable C++, because
// tests/mining_ore_harness.cpp compiles it with fake game types.
namespace ForgePact::CombatText {
inline PFUNC_YYGMLScript original = nullptr;
inline bool installTried = false, installed = false, native = false;
// The Experience slider's multiplier; StatsManager writes it wherever it sets
// that slider, back to 1 included. At 1 the text passes through unchanged.
inline double xpMultiplier = 1.0;
// HeroSiege::RewardScope::Active, handed in by StatsManager.hpp (the SDK header
// that defines it includes <windows.h>). Unset reads as "no scope active".
inline bool (*rewardScopeActive)() = nullptr;
// Silence for the calling thread only; a depth, so scopes nest.
inline thread_local int silenceDepth = 0;
inline uint64_t passedCalls = 0, silencedCalls = 0;

class ScopedSilence {
public:
    ScopedSilence() { ++silenceDepth; }
    ~ScopedSilence() { --silenceDepth; }
    ScopedSilence(const ScopedSilence&) = delete;
    ScopedSilence& operator=(const ScopedSilence&) = delete;
};

// Baloncuk metni duzeltmesi: EnemyGiveExperience/ExperienceUpdate zaten
// bicimlenmis "N XP" metni uretiyor, XP carpanindan ONCE. Verilen XP'ye
// dokunulmuyor; bu tamamen gorsel bir duzeltme.
inline RValue& Hook(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (silenceDepth > 0) { ++silencedCalls; return R; }
    ++passedCalls;
    RValue yeni;
    std::vector<RValue*> A2;
    double c = xpMultiplier;
    const bool scopeActive = rewardScopeActive && rewardScopeActive();
    if (!scopeActive && c != 1.0 && A && argc > 0 && A[0] && A[0]->m_Kind == VALUE_STRING) {
        try {
            std::string s = A[0]->ToString();
            static const std::string sonek = " XP";
            if (s.size() > sonek.size() && s.compare(s.size() - sonek.size(), sonek.size(), sonek) == 0) {
                std::string sayi = s.substr(0, s.size() - sonek.size());
                size_t kac = 0;
                double n = std::stod(sayi, &kac);
                if (kac == sayi.size()) {
                    char b[64];
                    std::snprintf(b, sizeof b, "%.0f XP", n * c);
                    yeni = RValue(b);
                    A2.assign(A, A + argc);
                    A2[0] = &yeni;
                }
            }
        } catch (...) {}
    }
    RValue** kullan = A2.empty() ? A : A2.data();
    return original ? original(S, O, R, argc, kullan) : R;
}

// True when the detour is in and native. The one HookOneScript call for
// CombatText in the plugin; hook id `fp_ctext`, as the Experience slider's was.
inline bool Install() {
    if (!installTried) {
        installTried = true;
        installed = HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CombatText), "fp_ctext",
            (PVOID)Hook, &original, &native);
    }
    return installed && native;
}
} // namespace ForgePact::CombatText
