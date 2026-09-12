#pragma once

#include "Common.hpp"

namespace ForgePact {

// Live character/combat stat modifiers ("stat <name> <mult>", "statadd <name>
// <bonus>", "stat list").
//
// Blood Pact's Magic Find / Attack Speed / Cast Rate / Experience gain /
// Movement Speed values do not sit in a store - the player has no such
// variables (measured: `iget magic_find` -> "no such variable"), and the
// runtime's pSt array holds handles, not values. Each stat has its own
// Stat<Name> script instead, and the game re-reads it every time it needs the
// value. So this scales the RETURNED value, not a stored one - one hook site
// affects every caller. A multiplier of 1.0 (or additive 0.0 for
// FasterCastRate) leaves the hook a pure pass-through: vanilla behavior is
// exactly preserved when a stat is off.
//
// Each hook keeps its own plain double rather than a name-keyed map: these
// run on some of the hottest paths in the game (measured in one session:
// StatMovementSpeed 6083 calls, StatTotalDamage-equivalent 2350 calls), so a
// map lookup building a temporary std::string per call is not affordable
// there.
//
// Several Stat* scripts return an ARRAY, not a number (measured 2026-08-28:
// StatMovementSpeed -> [305.108..., 0, 0, 600], where element 0 is the value
// shown on the character screen) - multiplying the whole array's ToDouble()
// produced a meaningless number, which is why Magic Find looked like it did
// nothing and Movement Speed stuttered in an earlier version. The array is
// never mutated in place either: the game may hand back the same array on
// every call, and scaling in place would compound every frame. A copy is
// made and only element 0 is scaled.
class StatsManager {
public:
    static StatsManager& Instance() {
        static StatsManager s_Instance;
        return s_Instance;
    }

    // "stat list" or "stat <name> <mult>" (name may be the full GML script
    // name, e.g. "StatMagicFind", or its short alias, e.g. "magicfind").
    void HandleStatCommand(const std::string& rest) {
        std::string a1, a2; a1 = FirstToken(rest, a2);
        while (!a1.empty() && std::isspace((unsigned char)a1.back())) a1.pop_back();
        while (!a2.empty() && std::isspace((unsigned char)a2.back())) a2.pop_back();

        const Entry* table = Table();

        if (Lower(a1) == "list" || a1.empty()) {
            for (int i = 0; i < kEntryCount; ++i) {
                const Entry& k = table[i];
                char b[160];
                sprintf_s(b, "  %-26s (%-11s) x%.2f  %-12s cagri=%ld", k.name, k.alias ? k.alias : "-",
                          *k.mult, *k.orig ? "kanca kurulu" : "kanca yok", *k.calls);
                Out(b);
            }
            return;
        }

        std::string ara = Lower(a1);
        const Entry* hedef = nullptr;
        for (int i = 0; i < kEntryCount; ++i) {
            std::string tam = Lower(table[i].name);
            if (ara == tam || ara == tam.substr(4) || (table[i].alias && ara == Lower(table[i].alias))) {
                hedef = &table[i]; break;
            }
        }
        if (!hedef) { Out("stat: bilinmeyen ad '" + a1 + "'  (stat list ile bak)"); return; }

        double c = 1.0;
        try { c = std::stod(a2); } catch (...) { Out("stat: carpan sayi olmali"); return; }
        if (c < 0.0) c = 0.0;

        if (c == 1.0 && !*hedef->orig) {
            *hedef->mult = 1.0;
            Out(std::string("stat ") + hedef->name + " -> x1.00 (native, no hook)");
            return;
        }
        if (!*hedef->orig) {
            HookOneScript(hedef->name, hedef->hookId, hedef->hook, hedef->orig);
            if (!*hedef->orig) { Out(std::string("stat: ") + hedef->name + " kancasi kurulamadi"); return; }
        }
        *hedef->mult = c;
        // XP carpani acilinca baloncuk metnini de duzelt (yalnizca gorsel).
        if (c != 1.0 && std::string(hedef->name) == "EnemyCalculateExperience" && !m_OrigCombatText)
            HookOneScript("CombatText", "fp_ctext", (void*)Hook_CombatText, &m_OrigCombatText);
        char b[160];
        sprintf_s(b, "stat %s -> x%.2f", hedef->name, c);
        Out(b);
    }

    // "statadd <name> <bonus>" - only StatFasterCastRate today (see StatEkle's
    // comment on the class: FCR's vanilla base is 0, so it must be added to,
    // not multiplied).
    void HandleStatAddCommand(const std::string& rest) {
        std::string ad, deger; ad = FirstToken(rest, deger);
        while (!ad.empty() && std::isspace((unsigned char)ad.back())) ad.pop_back();
        while (!deger.empty() && std::isspace((unsigned char)deger.back())) deger.pop_back();
        std::string ara = Lower(ad);
        if (ara != "castrate" && ara != "statfastercastrate" && ara != "fastercastrate") {
            Out("statadd: unknown '" + ad + "' (castrate)");
            return;
        }
        double ek = 0.0;
        try { ek = std::stod(deger); } catch (...) { Out("statadd: bonus sayi olmali"); return; }
        if (ek < 0.0) ek = 0.0;
        if (ek == 0.0 && !m_OrigAdd_StatFasterCastRate) {
            m_Add_StatFasterCastRate = 0.0;
            Out("statadd StatFasterCastRate -> +0.00 (native, no hook)");
            return;
        }
        if (!m_OrigAdd_StatFasterCastRate) {
            HookOneScript("StatFasterCastRate", "fp_sta_fcr", (void*)HookAdd_StatFasterCastRate, &m_OrigAdd_StatFasterCastRate);
            if (!m_OrigAdd_StatFasterCastRate) { Out("statadd: StatFasterCastRate kancasi kurulamadi"); return; }
        }
        m_Add_StatFasterCastRate = ek;
        char b[128];
        sprintf_s(b, "statadd StatFasterCastRate -> +%.2f", ek);
        Out(b);
    }

private:
    StatsManager() = default;

    // Vanilya tabani sifir olmayan statlar icin oransal carpim.
    static RValue Scale(RValue& deger, double carpan) {
        if (deger.m_Kind != VALUE_ARRAY)
            return RValue(deger.ToDouble() * carpan);
        int n = (int)g_Yytk->CallBuiltin("array_length", { deger }).ToDouble();
        if (n <= 0) return deger;
        RValue kopya = g_Yytk->CallBuiltin("array_create", { RValue((double)n) });
        for (int i = 0; i < n; i++) {
            RValue e = g_Yytk->CallBuiltin("array_get", { deger, RValue((double)i) });
            if (i == 0) e = RValue(e.ToDouble() * carpan);
            g_Yytk->CallBuiltin("array_set", { kopya, RValue((double)i), e });
        }
        return kopya;
    }

    // Faster Cast Rate'in vanilya tabani 0'dir. Sifiri carpmak her zaman sifir
    // verdigi icin bu statta yuzde puani EKLEMEK gerekir (+50 -> FCR 0'dan 50'ye).
    static RValue Add(RValue& deger, double ek) {
        if (deger.m_Kind != VALUE_ARRAY)
            return RValue(deger.ToDouble() + ek);
        int n = (int)g_Yytk->CallBuiltin("array_length", { deger }).ToDouble();
        if (n <= 0) return deger;
        RValue kopya = g_Yytk->CallBuiltin("array_create", { RValue((double)n) });
        for (int i = 0; i < n; i++) {
            RValue e = g_Yytk->CallBuiltin("array_get", { deger, RValue((double)i) });
            if (i == 0) e = RValue(e.ToDouble() + ek);
            g_Yytk->CallBuiltin("array_set", { kopya, RValue((double)i), e });
        }
        return kopya;
    }

#define FP_STAT_HOOK(NAME) \
    PFUNC_YYGMLScript m_Orig_##NAME{ nullptr }; \
    volatile long m_Calls_##NAME{ 0 }; \
    double m_Mult_##NAME{ 1.0 }; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        auto& mgr = Instance(); \
        BP_DIAG_INCREMENT(mgr.m_Calls_##NAME); \
        RValue& _r = mgr.m_Orig_##NAME ? mgr.m_Orig_##NAME(S, O, R, argc, A) : R; \
        if (mgr.m_Mult_##NAME != 1.0) { try { _r = Scale(_r, mgr.m_Mult_##NAME); } catch (...) {} } \
        return _r; \
    }

    FP_STAT_HOOK(StatMagicFind)
    // The aggregate StatAttackSpeed result is the value consumed by the live
    // attack-timing path. MainHand/OffHand are detail helpers used by the stat
    // query/UI path and can remain completely idle after the character loads.
    FP_STAT_HOOK(StatAttackSpeed)
    FP_STAT_HOOK(StatExperienceGain)
    FP_STAT_HOOK(StatMovementSpeed)
    FP_STAT_HOOK(StatExtraGold)
    FP_STAT_HOOK(StatLifeReplenish)
    FP_STAT_HOOK(StatManaReplenish)
    FP_STAT_HOOK(StatDefense)
    FP_STAT_HOOK(StatCritDamage)
    FP_STAT_HOOK(StatCritRate)
    FP_STAT_HOOK(StatSpellCritDamage)
    FP_STAT_HOOK(StatSpellCritRate)
    // Gercek son vurus hasari. StatTotalDamage yalnizca karakter istatistigi
    // hesap/arayuz yoludur; onu carpmak dusmana giden hasari degistirmedi.
    // Canli olcumde CalculateEndDamage her vurus icin nihai sayiyi dondurdu ve
    // ayni sayi hemen RunDamageSync'e girdi - carpan tam burada uygulanir.
    FP_STAT_HOOK(CalculateEndDamage)
    // XP carpani. Hedef EnemyCalculateExperience'in DONUS degeri: fonksiyonun
    // icindeki sabiti degil, tanimi geregi hesaplanan deneyimi carpiyoruz.
    FP_STAT_HOOK(EnemyCalculateExperience)
#undef FP_STAT_HOOK

    PFUNC_YYGMLScript m_OrigAdd_StatFasterCastRate{ nullptr };
    volatile long m_CallsAdd_StatFasterCastRate{ 0 };
    double m_Add_StatFasterCastRate{ 0.0 };
    static RValue& HookAdd_StatFasterCastRate(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        auto& mgr = Instance();
        BP_DIAG_INCREMENT(mgr.m_CallsAdd_StatFasterCastRate);
        RValue& _r = mgr.m_OrigAdd_StatFasterCastRate ? mgr.m_OrigAdd_StatFasterCastRate(S, O, R, argc, A) : R;
        if (mgr.m_Add_StatFasterCastRate != 0.0) { try { _r = Add(_r, mgr.m_Add_StatFasterCastRate); } catch (...) {} }
        return _r;
    }

    // Baloncuk metni duzeltmesi: EnemyGiveExperience/ExperienceUpdate zaten
    // bicimlenmis "N XP" metni uretiyor, XP carpanindan ONCE. Verilen XP'ye
    // dokunulmuyor; bu tamamen gorsel bir duzeltme.
    PFUNC_YYGMLScript m_OrigCombatText{ nullptr };
    static RValue& Hook_CombatText(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        auto& mgr = Instance();
        RValue yeni;
        std::vector<RValue*> A2;
        double c = mgr.m_Mult_EnemyCalculateExperience;
        if (c != 1.0 && A && argc > 0 && A[0] && A[0]->m_Kind == VALUE_STRING) {
            try {
                std::string s = A[0]->ToString();
                static const std::string sonek = " XP";
                if (s.size() > sonek.size() && s.compare(s.size() - sonek.size(), sonek.size(), sonek) == 0) {
                    std::string sayi = s.substr(0, s.size() - sonek.size());
                    size_t kac = 0;
                    double n = std::stod(sayi, &kac);
                    if (kac == sayi.size()) {
                        char b[64];
                        sprintf_s(b, "%.0f XP", n * c);
                        yeni = RValue(b);
                        A2.assign(A, A + argc);
                        A2[0] = &yeni;
                    }
                }
            } catch (...) {}
        }
        RValue** kullan = A2.empty() ? A : A2.data();
        return mgr.m_OrigCombatText ? mgr.m_OrigCombatText(S, O, R, argc, kullan) : R;
    }

    struct Entry { const char* name; const char* hookId; void* hook; PFUNC_YYGMLScript* orig; volatile long* calls; double* mult; const char* alias; };
    static constexpr int kEntryCount = 14;
    const Entry* Table() {
        static const Entry table[kEntryCount] = {
            { "StatMagicFind",        "fp_st_mf",  (void*)Hook_StatMagicFind,        &m_Orig_StatMagicFind,        &m_Calls_StatMagicFind,        &m_Mult_StatMagicFind,        "magicfind" },
            { "StatAttackSpeed",      "fp_st_as",  (void*)Hook_StatAttackSpeed,      &m_Orig_StatAttackSpeed,      &m_Calls_StatAttackSpeed,      &m_Mult_StatAttackSpeed,      "attackspeed" },
            { "StatExperienceGain",   "fp_st_xp",  (void*)Hook_StatExperienceGain,   &m_Orig_StatExperienceGain,   &m_Calls_StatExperienceGain,   &m_Mult_StatExperienceGain,   "expgain" },
            { "StatMovementSpeed",    "fp_st_ms",  (void*)Hook_StatMovementSpeed,    &m_Orig_StatMovementSpeed,    &m_Calls_StatMovementSpeed,    &m_Mult_StatMovementSpeed,    "movespeed" },
            { "CalculateEndDamage",   "fp_st_td",  (void*)Hook_CalculateEndDamage,   &m_Orig_CalculateEndDamage,   &m_Calls_CalculateEndDamage,   &m_Mult_CalculateEndDamage,   "damage" },
            { "StatExtraGold",        "fp_st_eg",  (void*)Hook_StatExtraGold,        &m_Orig_StatExtraGold,        &m_Calls_StatExtraGold,        &m_Mult_StatExtraGold,        "extragold" },
            { "StatLifeReplenish",    "fp_st_lr",  (void*)Hook_StatLifeReplenish,    &m_Orig_StatLifeReplenish,    &m_Calls_StatLifeReplenish,    &m_Mult_StatLifeReplenish,    "lifereplenish" },
            { "StatManaReplenish",    "fp_st_mr",  (void*)Hook_StatManaReplenish,    &m_Orig_StatManaReplenish,    &m_Calls_StatManaReplenish,    &m_Mult_StatManaReplenish,    "manareplenish" },
            { "StatDefense",          "fp_st_def", (void*)Hook_StatDefense,          &m_Orig_StatDefense,          &m_Calls_StatDefense,          &m_Mult_StatDefense,          "defense" },
            { "StatCritDamage",       "fp_st_cd",  (void*)Hook_StatCritDamage,       &m_Orig_StatCritDamage,       &m_Calls_StatCritDamage,       &m_Mult_StatCritDamage,       "critdamage" },
            { "StatCritRate",         "fp_st_cr",  (void*)Hook_StatCritRate,         &m_Orig_StatCritRate,         &m_Calls_StatCritRate,         &m_Mult_StatCritRate,         "critchance" },
            { "StatSpellCritDamage",  "fp_st_scd", (void*)Hook_StatSpellCritDamage,  &m_Orig_StatSpellCritDamage,  &m_Calls_StatSpellCritDamage,  &m_Mult_StatSpellCritDamage,  "spellcritdamage" },
            { "StatSpellCritRate",    "fp_st_scr", (void*)Hook_StatSpellCritRate,    &m_Orig_StatSpellCritRate,    &m_Calls_StatSpellCritRate,    &m_Mult_StatSpellCritRate,    "spellcritchance" },
            { "EnemyCalculateExperience", "fp_st_xpc", (void*)Hook_EnemyCalculateExperience, &m_Orig_EnemyCalculateExperience, &m_Calls_EnemyCalculateExperience, &m_Mult_EnemyCalculateExperience, "exp" },
        };
        return table;
    }
};

} // namespace ForgePact
