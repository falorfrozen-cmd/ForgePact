#pragma once

#include "Common.hpp"

namespace ForgePact {

// Skill sliders (#160): "skillslider <projamount|aoesize|projspeed> <value>"
// and "skillslider list".
//
// Each lever adds to one value the game is already computing for a skill
// cast, after the game's own calculation, so gear and buffs keep stacking
// underneath (docs/skill-sliders-research.md, "What this means for a slider"):
//   projamount  ReturnExtraSpellProjectiles,  +k projectiles per cast; whole,
//               ReturnExtraProjectilesRanged  at most kProjAmountMax. Ranged
//                                             returns the extra only and its
//                                             caller adds it (static reading)
//   aoesize     StatAOESkillSize              +b to element 0 of a copy of its
//                                             array (the skill AoE stat, 554)
//   projspeed   LoadAllModifiers +            +N to stat 75 while the player's
//               ReturnSpecificStat            own LoadAllModifiers is on the
//                                             stack: a projectile's speed
//                                             times 1 + N/100 (Live 2)
//
// Off is vanilla. A value of 0 installs nothing; the first non-zero value
// installs the lever's hooks, and setting 0 later leaves them pure
// pass-throughs (no self check, no count, no write). The game's code calls
// these scripts directly, so only the native detour can add anything: a lever
// arms only when every script it needs got one, and otherwise stays at 0 and
// says so, as `statadd` does.
//
// Only the player's own casts count, decided at the point of use from the
// `self` each hooked call receives (AGENTS.md, "Check a Permission Where It Is
// Used"): Player_obj, and the double-cast proc Universal_Double_Cast_obj that
// repeats the player's cast. Every other caller is meant to get exactly what
// the game returned, counted `other`: by design that covers the mercenary
// (seen live: Mercenary_obj counted other and left unchanged), enemies and
// basic attacks (LoadAllModifiers with self=Projectile_Player_obj; no basic
// attack or enemy call was seen live). A cast's base-6
// ReturnExtraSpellProjectiles call (an item proc, by the research's reading)
// has the same `self` and argument shapes as the skill's own, so it is raised
// too. The counts and the first-call line ship: a player build has no other
// way to show that the mod did something.
class SkillSlidersMod {
public:
    static SkillSlidersMod& Instance() {
        static SkillSlidersMod s_Instance;
        return s_Instance;
    }

    // Slider ceilings. The panel's rows (src/forgepact.py PERCENT_STATS) and
    // the hub model's lever transforms are pinned to these. Measured up to +2
    // projectiles, +50 AoE and +50 % speed; above that the model is linear.
    static constexpr double kProjAmountMax = 5.0;
    static constexpr double kAoeSizeMax = 100.0;
    static constexpr double kProjSpeedMax = 100.0;
    // Projectile speed: no Stat* script and no dispatcher case of its own;
    // LoadAllModifiers reads it through ReturnSpecificStat (stat id argument
    // a1), and a skill object's speed is scaled by 1 + stat/100. The hub's
    // docs/RUNTIME_DATA_MODELS.md § 7.6 records it.
    static constexpr double kProjectileSpeedStatId = 75.0;

    // "skillslider", "skillslider list" or "skillslider <lever> <value>".
    void HandleCommand(const std::string& rest) {
        std::string name, text; name = FirstToken(rest, text);
        while (!name.empty() && std::isspace((unsigned char)name.back())) name.pop_back();
        while (!text.empty() && std::isspace((unsigned char)text.back())) text.pop_back();
        const std::string key = Lower(name);
        if (key.empty() || key == "list") {
            for (int i = 0; i < kLeverCount; ++i) Out(StatusLine(i));
            return;
        }
        const int lever = FindLever(key);
        if (lever < 0) { Out("skillslider: unknown '" + name + "' (projamount, aoesize, projspeed, list)"); return; }
        double value = 0.0;
        try { value = std::stod(text); } catch (...) { Out("skillslider: the value must be a number"); return; }
        value = Clamp(lever, value);
        const LeverInfo& info = Levers()[lever];
        LeverState& state = m_Levers[lever];
        if (value == 0.0) {
            // Off: an installed hook stays a pass-through, whatever its route.
            state.value = 0.0;
            state.first = kFirstIdle;
            Out(std::string("skillslider ") + info.key + (Hooked(lever) ? " -> +0" : " -> +0 (native, no hook)"));
            return;
        }
        for (int i = 0; i < info.scriptCount; ++i) {
            ScriptHook& hook = m_Hooks[info.scripts[i]];
            if (hook.orig) continue;
            const ScriptInfo& script = Scripts()[info.scripts[i]];
            bool native = false;
            HookOneScript(script.name, script.hookId, script.detour, &hook.orig, &native);
            hook.route = !hook.orig ? kRouteFailed : (native ? kRouteNative : kRouteTableOnly);
        }
        const int blocked = FirstNotNative(lever);
        if (blocked >= 0) {
            state.value = 0.0;
            state.first = kFirstIdle;
            Out(std::string("skillslider: ") + Scripts()[blocked].name + " hook is " + RouteName(m_Hooks[blocked].route)
                + " - the game calls it directly, so the bonus could never apply; not armed");
            return;
        }
        state.value = value;
        state.first = kFirstPending;
        // An object name that did not resolve is asked again, once, per arming.
        if (m_PlayerIndex == kUnresolved) m_PlayerIndex = -1;
        if (m_DoubleCastIndex == kUnresolved) m_DoubleCastIndex = -1;
        char b[96];
        sprintf_s(b, "skillslider %s -> +%g", info.key, value);
        Out(b);
    }

private:
    SkillSlidersMod() = default;

    enum Route : int { kRouteNone = 0, kRouteNative, kRouteTableOnly, kRouteFailed };
    enum First : int { kFirstIdle = 0, kFirstPending, kFirstShown };
    static const char* RouteName(int route) {
        switch (route) {
            case kRouteNative: return "native";
            case kRouteTableOnly: return "TABLE-ONLY";
            case kRouteFailed: return "FAILED";
            default: return "none";
        }
    }

    // ---- the five hooked scripts -------------------------------------------
    enum ScriptId : int { kSpell = 0, kRanged, kAoe, kLoadMods, kSpecificStat, kScriptCount };
    struct ScriptInfo { const char* name; const char* hookId; void* detour; };
    struct ScriptHook { PFUNC_YYGMLScript orig = nullptr; int route = kRouteNone; };

    // ---- the three levers ----------------------------------------------------
    enum LeverId : int { kAmount = 0, kAoeSize, kSpeed, kLeverCount };
    struct LeverInfo { const char* key; int scripts[2]; int scriptCount; double max; bool whole; };
    struct LeverState {
        double value = 0.0;
        int first = kFirstIdle;
        double firstNative = 0.0, firstBoosted = 0.0;
        long own = 0, doubleCast = 0, other = 0;   // calls met while the lever is non-zero
        long skip = 0;                             // in-scope calls whose result AddTo could not change
        int lastOtherIndex = -2;                   // the object `lastOther` names; -1 unreadable
        std::string lastOther = "-";
    };

    static const ScriptInfo* Scripts() {
        static const ScriptInfo scripts[kScriptCount] = {
            { SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnExtraSpellProjectiles), "fp_ss_amount", (void*)HookSpell },
            { SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnExtraProjectilesRanged), "fp_ss_ranged", (void*)HookRanged },
            { SdkShortScriptName(HeroSiege::Scripts::gml_Script_StatAOESkillSize), "fp_ss_aoe", (void*)HookAoe },
            { SdkShortScriptName(HeroSiege::Scripts::gml_Script_LoadAllModifiers), "fp_ss_mods", (void*)HookLoadAllModifiers },
            { SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnSpecificStat), "fp_ss_stat", (void*)HookReturnSpecificStat },
        };
        return scripts;
    }
    static const LeverInfo* Levers() {
        static const LeverInfo levers[kLeverCount] = {
            { "projamount", { kSpell, kRanged }, 2, kProjAmountMax, true },
            { "aoesize", { kAoe, kAoe }, 1, kAoeSizeMax, false },
            { "projspeed", { kLoadMods, kSpecificStat }, 2, kProjSpeedMax, false },
        };
        return levers;
    }

    ScriptHook m_Hooks[kScriptCount];
    LeverState m_Levers[kLeverCount];
    // An object index by name: -1 not asked yet, kUnresolved asked this
    // arming without an answer. Neither can equal a real index (0 or more),
    // so an unresolved name never matches an unreadable self.
    static constexpr int kUnresolved = -2;
    int m_PlayerIndex = -1;       // Player_obj's object index, once resolved
    int m_DoubleCastIndex = -1;   // Universal_Double_Cast_obj's

    // Whether the innermost LoadAllModifiers on this thread's stack belongs to
    // the player (Projectile Speed only). Saved and restored around each call,
    // so a mercenary's LoadAllModifiers nested inside the player's is left
    // alone and the player's applies again after it returns.
    static inline thread_local bool t_SpeedScope = false;
    struct SpeedScope {
        bool saved;
        explicit SpeedScope(bool inScope) : saved(t_SpeedScope) { t_SpeedScope = inScope; }
        ~SpeedScope() { t_SpeedScope = saved; }
        SpeedScope(const SpeedScope&) = delete;
        SpeedScope& operator=(const SpeedScope&) = delete;
    };

    static RValue& HookSpell(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        return Instance().AddAfter(kSpell, kAmount, S, O, R, argc, A);
    }
    static RValue& HookRanged(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        return Instance().AddAfter(kRanged, kAmount, S, O, R, argc, A);
    }
    static RValue& HookAoe(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        return Instance().AddAfter(kAoe, kAoeSize, S, O, R, argc, A);
    }
    // Records, for the duration of the original call, whether its `self` is
    // in scope. Counted per call, while the lever is non-zero.
    static RValue& HookLoadAllModifiers(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        SkillSlidersMod& mod = Instance();
        const PFUNC_YYGMLScript orig = mod.m_Hooks[kLoadMods].orig;
        if (!orig) return R;
        LeverState& state = mod.m_Levers[kSpeed];
        if (state.value == 0.0) return orig(S, O, R, argc, A);
        bool inScope = false;
        try { inScope = mod.InScope(state, S); } catch (...) { inScope = false; }
        SpeedScope scope(inScope);
        return orig(S, O, R, argc, A);
    }
    // Adds to stat 75 only inside the player's LoadAllModifiers; anywhere
    // else this is one thread-local read and the call.
    static RValue& HookReturnSpecificStat(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        SkillSlidersMod& mod = Instance();
        const PFUNC_YYGMLScript orig = mod.m_Hooks[kSpecificStat].orig;
        RValue& r = orig ? orig(S, O, R, argc, A) : R;
        if (!t_SpeedScope) return r;
        LeverState& state = mod.m_Levers[kSpeed];
        const double value = state.value;
        if (value == 0.0) return r;
        double id = -1.0;
        if (!StatIdArg(argc, A, id) || id != kProjectileSpeedStatId) return r;
        try { mod.Boost(r, value, state, kSpecificStat); } catch (...) {}
        return r;
    }

    // The amount and AoE hooks: the original first, then, for a self in
    // scope, the lever's value added to what it returned.
    RValue& AddAfter(int script, int lever, CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        const PFUNC_YYGMLScript orig = m_Hooks[script].orig;
        RValue& r = orig ? orig(S, O, R, argc, A) : R;
        LeverState& state = m_Levers[lever];
        const double value = state.value;
        if (value == 0.0) return r;
        try {
            if (InScope(state, S)) Boost(r, value, state, script);
        } catch (...) {}
        return r;
    }

    // Adds `value` to the result and, on the first boosted call after
    // arming, prints what the game returned and what it now returns - once,
    // so a hot path logs nothing further. A result AddTo cannot change
    // reaches the game as it was and is counted `skip=`, so a boost that
    // silently stopped applying shows in the status line.
    void Boost(RValue& r, double value, LeverState& state, int script) {
        double native = 0.0;
        bool added = false;
        try { added = AddTo(r, value, native); } catch (...) { added = false; }
        if (!added) { ++state.skip; return; }
        if (state.first != kFirstPending) return;
        state.first = kFirstShown;
        state.firstNative = native;
        state.firstBoosted = native + value;
        char b[160];
        sprintf_s(b, "skillslider %s: first boosted call %g -> %g", Scripts()[script].name, native, native + value);
        Out(b);
    }

    // The kinds the runtime produces for a number, and for an instance's
    // `object_index`, which this runner may hand back as a reference with a
    // flag bit above the kind. RValue::ToDouble() is the runner's REAL_RValue,
    // which raises the runner's own error on any other kind instead of
    // throwing, so the kind is asked before any conversion.
    static uint32_t KindOf(const RValue& v) { return static_cast<uint32_t>(v.m_Kind) & 0x0FFFFFFFU; }
    static bool IsNumber(const RValue& v) {
        const uint32_t kind = KindOf(v);
        return kind == VALUE_REAL || kind == VALUE_INT32 || kind == VALUE_INT64;
    }

    // A number gets `value` added; an array gets it added to element 0 of a
    // copy, because the game may hand the same array back on every call and
    // adding in place would compound. Anything else is left exactly as the
    // game returned it. `native` receives what the game returned.
    static bool AddTo(RValue& r, double value, double& native) {
        if (IsNumber(r)) {
            native = r.ToDouble();
            r = RValue(native + value);
            return true;
        }
        if (KindOf(r) != VALUE_ARRAY) return false;
        const int n = (int)g_Yytk->CallBuiltin("array_length", { r }).ToDouble();
        if (n <= 0) return false;
        const RValue head = g_Yytk->CallBuiltin("array_get", { r, RValue(0.0) });
        if (!IsNumber(head)) return false;
        native = head.ToDouble();
        RValue copy = g_Yytk->CallBuiltin("array_create", { RValue((double)n) });
        g_Yytk->CallBuiltin("array_set", { copy, RValue(0.0), RValue(native + value) });
        for (int i = 1; i < n; i++)
            g_Yytk->CallBuiltin("array_set", { copy, RValue((double)i), g_Yytk->CallBuiltin("array_get", { r, RValue((double)i) }) });
        r = copy;
        return true;
    }

    // The stat-id argument of a ReturnSpecificStat query: a1, numeric, in the
    // shape (1, <id>, 0) the game passes.
    static bool StatIdArg(int argc, RValue** A, double& id) {
        if (argc < 2 || !A || !A[1] || !IsNumber(*A[1])) return false;
        id = A[1]->ToDouble();
        return true;
    }

    // `self`'s object_index, read through variable_instance_get (the struct
    // read is research-only); -1 for a null self or one with no readable
    // object_index. The self's own kind is never asked: the runner hands an
    // instance over as VALUE_OBJECT or VALUE_REF alike.
    static int SelfObjectIndex(CInstance* S) {
        if (!S) return -1;
        try {
            const RValue index = g_Yytk->CallBuiltin("variable_instance_get", { RValue(S), RValue("object_index") });
            const uint32_t kind = KindOf(index);
            if (kind != VALUE_REAL && kind != VALUE_INT32 && kind != VALUE_INT64 && kind != VALUE_REF) return -1;
            const double n = index.ToDouble();
            if (!std::isfinite(n) || n < 0.0 || n > 2147483647.0) return -1;
            return (int)n;
        } catch (...) { return -1; }
    }

    // An object's index by its SDK name, which asset_get_index may answer as
    // a number or a reference. Asked at most once per arming until it
    // resolves; a name that does not resolve says so once, and the calls
    // whose self is that object then count as `other`.
    static void Resolve(int& index, HeroSiege::Objects::GameObject object) {
        if (index >= 0 || index == kUnresolved) return;
        const std::string name(HeroSiege::Objects::GetObjectName(object));
        int resolved = -1;
        try {
            const RValue v = g_Yytk->CallBuiltin("asset_get_index", { RValue(name) });
            const double n = IsNumber(v) || KindOf(v) == VALUE_REF ? v.ToDouble() : -1.0;
            resolved = std::isfinite(n) && n >= 0.0 && n <= 2147483647.0 ? (int)n : -1;
        } catch (...) { resolved = -1; }
        if (resolved >= 0) { index = resolved; return; }
        index = kUnresolved;
        Out("skillslider: cannot resolve " + name + " by name; its calls count as other");
    }

    // Is this call the player's own cast? Player_obj has no child objects and
    // Mercenary_obj's parent is Enemy_Aggroable_obj (SDK parent table), so an
    // exact index match is the whole test. Only a readable index (0 or more)
    // is compared, so an unreadable self never matches an unresolved name.
    bool InScope(LeverState& state, CInstance* S) {
        const int index = SelfObjectIndex(S);
        if (index >= 0) {
            Resolve(m_PlayerIndex, HeroSiege::Objects::GameObject::Player_obj);
            Resolve(m_DoubleCastIndex, HeroSiege::Objects::GameObject::Universal_Double_Cast_obj);
            if (index == m_PlayerIndex) { ++state.own; return true; }
            if (index == m_DoubleCastIndex) { ++state.doubleCast; return true; }
        }
        ++state.other;
        NoteOther(state, index);
        return false;
    }

    // `last-other=`: the object's name, asked only when it changes; `?` when
    // the self was null or its object_index unreadable.
    static void NoteOther(LeverState& state, int index) {
        if (index == state.lastOtherIndex) return;
        state.lastOtherIndex = index;
        if (index < 0) { state.lastOther = "?"; return; }
        state.lastOther = "#" + std::to_string(index);
        try {
            const RValue name = g_Yytk->CallBuiltin("object_get_name", { RValue((double)index) });
            if (KindOf(name) == VALUE_STRING) {
                const std::string text = name.ToString();
                if (!text.empty()) state.lastOther = text;
            }
        } catch (...) {}
    }

    int FindLever(const std::string& lowered) const {
        for (int i = 0; i < kLeverCount; ++i)
            if (lowered == Levers()[i].key) return i;
        return -1;
    }

    // Not finite or negative is 0; Projectile Amount is whole; each is capped
    // at its ceiling.
    static double Clamp(int lever, double v) {
        const LeverInfo& info = Levers()[lever];
        if (!std::isfinite(v) || v < 0.0) v = 0.0;
        if (info.whole) v = std::floor(v + 0.5);
        if (v > info.max) v = info.max;
        return v;
    }

    bool Hooked(int lever) const {
        const LeverInfo& info = Levers()[lever];
        for (int i = 0; i < info.scriptCount; ++i)
            if (m_Hooks[info.scripts[i]].orig) return true;
        return false;
    }
    // The first of the lever's scripts without a native detour, or -1.
    int FirstNotNative(int lever) const {
        const LeverInfo& info = Levers()[lever];
        for (int i = 0; i < info.scriptCount; ++i)
            if (m_Hooks[info.scripts[i]].route != kRouteNative) return info.scripts[i];
        return -1;
    }

    // skillslider <lever> +<v> hook=<h> first=<f> own=<n> double=<n> other=<n> skip=<n> last-other=<name>
    std::string StatusLine(int lever) const {
        const LeverInfo& info = Levers()[lever];
        const LeverState& state = m_Levers[lever];
        bool attempted = false;
        for (int i = 0; i < info.scriptCount; ++i)
            if (m_Hooks[info.scripts[i]].route != kRouteNone) attempted = true;
        std::string hook = "none";
        if (attempted) {
            const int blocked = FirstNotNative(lever);
            hook = blocked < 0 ? std::string("native")
                               : std::string(Scripts()[blocked].name) + ":" + RouteName(m_Hooks[blocked].route);
        }
        std::string first = "-";
        if (state.first == kFirstPending) first = "waiting";
        else if (state.first == kFirstShown) {
            char f[96];
            sprintf_s(f, "%g->%g", state.firstNative, state.firstBoosted);
            first = f;
        }
        char b[160];
        sprintf_s(b, "skillslider %s +%g hook=", info.key, state.value);
        return std::string(b) + hook + " first=" + first + " own=" + std::to_string(state.own)
             + " double=" + std::to_string(state.doubleCast) + " other=" + std::to_string(state.other)
             + " skip=" + std::to_string(state.skip) + " last-other=" + state.lastOther;
    }
};

} // namespace ForgePact
