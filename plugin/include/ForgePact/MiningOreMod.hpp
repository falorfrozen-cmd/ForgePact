#pragma once
#include <array>
#include <cmath>
#include <sstream>
#include <hs_game_sdk/item_type.hpp>
#include <hs_game_sdk/scripts.hpp>

// Included after HookOneScript. Interoperability facts and validation status:
// docs/mining-ore-research.md. No whole-map scan, saved node edits or extra rolls.
namespace ForgePact::MiningOre {
inline constexpr int kMaxMultiplier = 10;
inline constexpr double kMaxRewardQuantity = 1000000;
// Material definitions, not object indices. No corresponding SDK enum exists.
inline constexpr int kFirstOreBase = 27, kLastOreBase = 32;
inline int multiplier = 1;
inline bool installTried = false, ready = false, unavailable = false;
inline bool loggedReward = false, loggedFailure = false;
// Readiness and actual execution are different states. These first-use flags
// let the existing mod-state writer distinguish a blind hook from a refusal.
inline bool stepObserved = false, oreObserved = false;
inline PFUNC_YYGMLScript originalStep = nullptr, originalLoot = nullptr;
inline thread_local CInstance* activeNode = nullptr;
inline thread_local bool inReward = false;
// Optional item mechanic: resolved at the actual ore reward, never cached as
// permission between frames. A worn helmet replaces, rather than stacks, xN.
inline int (*rewardMultiplier)(CInstance*) = nullptr;
inline void (*rewardDispatched)(CInstance*, double, double) = nullptr;
#ifndef FORGEPACT_RELEASE
inline uint64_t stepCalls = 0, lootCalls = 0, changedRewards = 0;
#endif

template<class T> class ScopedValue {
    T& slot;
    T previous;
public:
    ScopedValue(T& target, T value) : slot(target), previous(target) { slot = value; }
    ~ScopedValue() { slot = previous; }
    ScopedValue(const ScopedValue&) = delete;
    ScopedValue& operator=(const ScopedValue&) = delete;
};

inline bool WholeNumber(const RValue& value, double& out) {
    if (value.m_Kind != VALUE_REAL && value.m_Kind != VALUE_INT32 && value.m_Kind != VALUE_INT64) return false;
    out = value.ToDouble();
    return std::isfinite(out) && std::floor(out) == out;
}

inline void RefuseOnce() {
    if (!loggedFailure) {
        loggedFailure = true;
        Out("miningore: could not validate/copy this reward; the game's original reward was left unchanged");
    }
}

inline RValue& HookStep(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (!originalStep) return R;
    if ((multiplier <= 1 && !rewardMultiplier) || !ready) return originalStep(S, O, R, argc, A);
    if (!stepObserved) stepObserved = true;
#ifndef FORGEPACT_RELEASE
    ++stepCalls;
#endif
    ScopedValue<CInstance*> scope(activeNode, S);
    return originalStep(S, O, R, argc, A);
}

inline RValue& HookLoot(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (!originalLoot) return R;
    if ((multiplier <= 1 && !rewardMultiplier) || !ready || !S || activeNode != S || inReward)
        return originalLoot(S, O, R, argc, A);
    // Bound the copied pointer vector; never mutate the caller's args/struct.
    if (!A || argc < 4 || argc > 16 || !A[2] || !A[3])
        return originalLoot(S, O, R, argc, A);
#ifndef FORGEPACT_RELEASE
    ++lootCalls;
#endif
    ScopedValue<bool> rewardScope(inReward, true);
    RValue clone;
    std::array<RValue*, 16> forwarded{};
    bool changed = false;
    double quantity = 1, scaled = 1;
    int factor = multiplier;
    try {
        double type = -1, base = -1;
        const RValue& params = *A[3];
        if (WholeNumber(*A[2], type) && type == static_cast<int>(HeroSiege::Items::ItemType::Material)
            && params.m_Kind == VALUE_OBJECT && params.m_Object
            && g_Yytk->CallBuiltin("variable_struct_exists", { params, RValue("b") }).ToBoolean()
            && WholeNumber(g_Yytk->CallBuiltin("variable_struct_get", { params, RValue("b") }), base)
            && base >= kFirstOreBase && base <= kLastOreBase) {
            if (!oreObserved) oreObserved = true;
            factor = rewardMultiplier ? rewardMultiplier(S) : multiplier;
            if (factor < 1 || factor > kMaxMultiplier) factor = 1;
            bool valid = true;
            if (g_Yytk->CallBuiltin("variable_struct_exists", { params, RValue("o") }).ToBoolean())
                valid = WholeNumber(g_Yytk->CallBuiltin("variable_struct_get", { params, RValue("o") }), quantity);
            if (factor > 1 && valid && quantity >= 1 && quantity <= kMaxRewardQuantity / factor) {
                scaled = quantity * factor;
                // Shallow copy preserves every existing parameter. Only o changes.
                clone = g_Yytk->CallBuiltin("variable_clone", { params, RValue(0.0) });
                if (clone.m_Kind == VALUE_OBJECT && clone.m_Object && clone.m_Object != params.m_Object) {
                    g_Yytk->CallBuiltin("variable_struct_set", { clone, RValue("o"), RValue(scaled) });
                    double readback = 0;
                    if (WholeNumber(g_Yytk->CallBuiltin("variable_struct_get", { clone, RValue("o") }), readback)
                        && readback == scaled) {
                        for (int i = 0; i < argc; ++i) forwarded[i] = A[i];
                        forwarded[3] = &clone;
                        changed = true;
                    }
                }
            }
            if (factor > 1 && !changed) RefuseOnce();
        }
    } catch (...) { RefuseOnce(); }
    // The native call is OUTSIDE the catch. Never retry a partially-run reward.
    RValue& result = originalLoot(S, O, R, argc, changed ? forwarded.data() : A);
    if (changed) {
#ifndef FORGEPACT_RELEASE
        ++changedRewards;
#endif
        if (!loggedReward) {
            loggedReward = true;
            Out("miningore: first reward dispatched " + std::to_string((int)quantity)
                + " -> " + std::to_string((int)scaled) + " (one native drop call)");
        }
        if (rewardDispatched && A[0] && A[1]) {
            try { rewardDispatched(S, A[0]->ToDouble(), A[1]->ToDouble()); }
            catch (...) { /* The original reward has already succeeded; never retry it. */ }
        }
    }
    return result;
}

inline bool Install() {
    if (installTried) return ready;
    installTried = true;
    bool nativeStep = false, nativeLoot = false;
    const bool step = HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_MiningNodeStepMain),
        "fp_mining_step", (PVOID)HookStep, &originalStep, &nativeStep);
    const bool loot = HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LootGroundCreate),
        "fp_mining_reward", (PVOID)HookLoot, &originalLoot, &nativeLoot);
    ready = step && loot && nativeStep && nativeLoot;
    unavailable = !ready;
    if (!ready) Out("miningore: unavailable - both native hooks are required; ore amount stays x1");
    return ready;
}

inline void Command(const std::string& text) {
#ifndef FORGEPACT_RELEASE
    if (text == "stat") {
        Out("miningore: steps=" + std::to_string(stepCalls) + " scopedLoot=" + std::to_string(lootCalls)
            + " changed=" + std::to_string(changedRewards) + " multiplier=" + std::to_string(multiplier)
            + " nativeReady=" + std::to_string(ready));
        return;
    }
#endif
    std::istringstream input(text);
    int value = 0;
    std::string extra;
    if (!(input >> value) || (input >> extra) || value < 1 || value > kMaxMultiplier) {
        Out("miningore: use a whole-number multiplier from 1 to 10"); return;
    }
    if (value > 1 && !Install()) { multiplier = 1; return; }
    multiplier = value;
    loggedReward = loggedFailure = false;
    Out("miningore: x" + std::to_string(value) + (value == 1 ? " (vanilla)" : " (mining ore rewards only)"));
}
} // namespace ForgePact::MiningOre
