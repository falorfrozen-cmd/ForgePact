#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include <hs_game_sdk/item_type.hpp>
#include <hs_game_sdk/scripts.hpp>
#include <ForgePact/CombatTextHook.hpp>

// Included after HookOneScript. Interoperability facts and validation status:
// docs/mining-ore-research.md. No whole-map scan or saved node edits.
//
// Extra rolls (`miningrolls N`, issue #36): after the game's own dig
// completion returns, the same completion runs again on the same node, up to
// N - 1 more times, so each roll pays the node's ore stacks and rolls the dig's
// stat-gated bonus finds again. Before each extra run the node's `hp` goes back
// to 1 and `miningQue` to true (the route Vein Resonance proved live); during
// it mining XP, character and guild XP, quest progress and the floating text
// are silenced, so they count once per node. A run that pays no ore ends the
// loop, and the node is always left with `hp` 0.
namespace ForgePact::MiningOre {
inline constexpr int kMaxMultiplier = 10;
inline constexpr int kMaxRolls = 10;
inline constexpr double kMaxRewardQuantity = 1000000;
// Material definitions, not object indices. No corresponding SDK enum exists.
inline constexpr int kFirstOreBase = 27, kLastOreBase = 32;
inline int multiplier = 1;
inline bool installTried = false, ready = false, unavailable = false;
inline bool loggedReward = false, loggedFailure = false;
// Which of the step/loot pair failed to come up native, for the rolls' line.
inline std::string installFailure;
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

// Extra rolls. Readiness is kept apart from the multiplier's: a rolls-side
// failure never touches `multiplier`, `ready` or `unavailable`.
inline int rolls = 1;
inline bool rollsInstallTried = false, rollsReady = false, rollsUnavailable = false;
inline std::string rollsFailure;
inline bool loggedRollPaid = false, loggedRollUnpaid = false, loggedHpReset = false, loggedRearm = false;
// Kept in both builds: the mod state reports them, so the panel and the live
// checks read what the plugin did, not what it is set to.
inline uint64_t extraRuns = 0, extraRunsUnpaid = 0, silencedCalls = 0;
// Per step call: HookLoot recognised an ore stack (at any factor) inside it.
inline thread_local bool oreRewardSeen = false;
inline thread_local int oreStacksSeen = 0;
inline thread_local bool inExtraRun = false;

// The dig's side effects that must happen once per node: each gets a
// pass-through detour that calls the game outside an extra run and returns
// without calling it inside one. The floating text is the shared CombatText
// detour's job (CombatTextHook.hpp), silenced the same way.
struct SilencedScript {
    const char* name;
    const char* hookId;
    PFUNC_YYGMLScript original;
    bool native;
    uint64_t passed, silenced;
};
inline std::array<SilencedScript, 4> silencedScripts = {{
    { SdkShortScriptName(HeroSiege::Scripts::gml_Script_MiningAdd), "fp_mining_rolls_add", nullptr, false, 0, 0 },
    { SdkShortScriptName(HeroSiege::Scripts::gml_Script_ExperienceUpdate), "fp_mining_rolls_xp", nullptr, false, 0, 0 },
    { SdkShortScriptName(HeroSiege::Scripts::gml_Script_GuildExperienceAdd), "fp_mining_rolls_guild", nullptr, false, 0, 0 },
    { SdkShortScriptName(HeroSiege::Scripts::gml_Script_update_quest), "fp_mining_rolls_quest", nullptr, false, 0, 0 },
}};

// Every silenced call, the four above and the shared CombatText detour's.
inline uint64_t SilencedCalls() { return silencedCalls + CombatText::silencedCalls; }

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

// The adapter does work only while one of its levers is on; otherwise both
// detours are pure pass-through with no builtin call.
inline bool Working() { return multiplier > 1 || rewardMultiplier || rolls > 1; }

#ifndef FORGEPACT_RELEASE
// Research: the node's state before and after each extra run, so a run that
// pays nothing records what was supplied. At most eight a session.
inline constexpr size_t kMaxSnapshots = 8;
inline std::vector<std::string> snapshots;
inline std::string DescribeNodeValue(const RValue& v) {
    if (v.m_Kind == VALUE_UNDEFINED) return "undef";
    if (v.m_Kind == VALUE_BOOL) return v.ToBoolean() ? "true" : "false";
    if (v.m_Kind == VALUE_STRING) return "\"" + v.ToString() + "\"";
    if (v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64 || v.m_Kind == VALUE_REF) {
        char buf[48]; std::snprintf(buf, sizeof buf, "%g", v.ToDouble());
        return std::string(buf) + (v.m_Kind == VALUE_REF ? "r" : "");
    }
    return "k" + std::to_string(v.m_Kind);
}
inline void Snapshot(CInstance* node, const std::string& label) {
    if (!node || snapshots.size() >= kMaxSnapshots) return;
    std::string line = label + ":";
    const RValue self = node->ToRValue();
    for (const char* name : {"hp", "miningQue", "miningActive", "stop", "range", "miningPlayer", "sprite_index"}) {
        try {
            if (!g_Yytk->CallBuiltin("variable_instance_exists", {self, RValue(name)}).ToBoolean()) { line += std::string(" ") + name + "=-"; continue; }
            line += std::string(" ") + name + "=" + DescribeNodeValue(g_Yytk->CallBuiltin("variable_instance_get", {self, RValue(name)}));
        } catch (...) { line += std::string(" ") + name + "=!"; }
    }
    snapshots.push_back(line);
}
#endif

// Sends the node through the game's completion on its next step call, as the
// Vein Resonance route does. False when the node could not be written.
inline bool Rearm(CInstance* node) {
    try {
        const RValue self = node->ToRValue();
        g_Yytk->CallBuiltin("variable_instance_set", {self, RValue("hp"), RValue(1.0)});
        g_Yytk->CallBuiltin("variable_instance_set", {self, RValue("miningQue"), RValue(true)});
        return true;
    } catch (...) {
        if (!loggedRearm) {
            loggedRearm = true;
            Out("miningrolls: could not re-arm the node - extra rolls stopped for this dig");
        }
        return false;
    }
}

// A node must never be left diggable a second time: whatever the extra runs
// did, `hp` ends at 0.
inline void EnsureDepleted(CInstance* node) {
    try {
        const RValue self = node->ToRValue();
        double hp = -1;
        if (WholeNumber(g_Yytk->CallBuiltin("variable_instance_get", {self, RValue("hp")}), hp) && hp == 0) return;
        g_Yytk->CallBuiltin("variable_instance_set", {self, RValue("hp"), RValue(0.0)});
        if (!loggedHpReset) {
            loggedHpReset = true;
            Out("miningrolls: node left diggable, hp reset to 0");
        }
    } catch (...) {}
}

// Up to rolls - 1 more runs of the game's own completion on this node. Each is
// the trampoline call with the original arguments (its own return slot, so the
// caller keeps the original run's result), outside any handler that retries it.
inline void ExtraRolls(CInstance* S, CInstance* O, int argc, RValue** A) {
    const int planned = rolls - 1;
    int paid = 0;
    try {
        for (int k = 1; k <= planned; ++k) {
            oreRewardSeen = false;
            oreStacksSeen = 0;
            if (!Rearm(S)) break;
#ifndef FORGEPACT_RELEASE
            Snapshot(S, "before extra roll " + std::to_string(k));
#endif
            ++extraRuns;
            {
                ScopedValue<bool> extra(inExtraRun, true);
                CombatText::ScopedSilence silence;
                RValue discarded;
                originalStep(S, O, discarded, argc, A);
            }
#ifndef FORGEPACT_RELEASE
            Snapshot(S, "after extra roll " + std::to_string(k));
#endif
            if (!oreRewardSeen) {
                ++extraRunsUnpaid;
                if (!loggedRollUnpaid) {
                    loggedRollUnpaid = true;
                    Out("miningrolls: extra roll paid nothing - stopped after " + std::to_string(paid) + " of "
                        + std::to_string(planned) + " extra rolls");
                }
                break;
            }
            ++paid;
            if (!loggedRollPaid) {
                loggedRollPaid = true;
                Out("miningrolls: first extra roll paid " + std::to_string(oreStacksSeen) + " ore stacks");
            }
        }
    } catch (...) {
        EnsureDepleted(S);
        throw;
    }
    EnsureDepleted(S);
}

inline RValue& HookStep(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (!originalStep) return R;
#ifndef FORGEPACT_RELEASE
    // Every step call the native detour sees, lever on or off: the research
    // build's positive control that the detour reaches the dig at all.
    if (ready) ++stepCalls;
#endif
    if (!Working() || !ready) return originalStep(S, O, R, argc, A);
    if (!stepObserved) stepObserved = true;
    ScopedValue<CInstance*> scope(activeNode, S);
    ScopedValue<bool> seen(oreRewardSeen, false);
    ScopedValue<int> stacks(oreStacksSeen, 0);
    RValue& result = originalStep(S, O, R, argc, A);
    // Only after a step call that paid ore, never from inside an extra run.
    if (rolls > 1 && rollsReady && oreRewardSeen && S && !inExtraRun) ExtraRolls(S, O, argc, A);
    return result;
}

inline RValue& HookLoot(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (!originalLoot) return R;
    if (!Working() || !ready || !S || activeNode != S || inReward)
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
            // The extra rolls' "this step call paid ore" signal, at any factor.
            oreRewardSeen = true;
            ++oreStacksSeen;
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
        // The helmet's pulse and Vein Resonance follow the original run only:
        // one dig still starts at most two veins and one pulse.
        if (rewardDispatched && A[0] && A[1] && !inExtraRun) {
            try { rewardDispatched(S, A[0]->ToDouble(), A[1]->ToDouble()); }
            catch (...) { /* The original reward has already succeeded; never retry it. */ }
        }
    }
    return result;
}

template<size_t I> RValue& HookSilenced(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    SilencedScript& script = silencedScripts[I];
    if (inExtraRun) { ++script.silenced; ++silencedCalls; return R; }
    ++script.passed;
    return script.original ? script.original(S, O, R, argc, A) : R;
}
inline constexpr std::array<PFUNC_YYGMLScript, 4> kSilencedHooks = {
    HookSilenced<0>, HookSilenced<1>, HookSilenced<2>, HookSilenced<3> };

inline bool Install() {
    if (installTried) return ready;
    installTried = true;
    bool nativeStep = false, nativeLoot = false;
    const char* stepName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_MiningNodeStepMain);
    const char* lootName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_LootGroundCreate);
    const bool step = HookOneScript(stepName, "fp_mining_step", (PVOID)HookStep, &originalStep, &nativeStep);
    const bool loot = HookOneScript(lootName, "fp_mining_reward", (PVOID)HookLoot, &originalLoot, &nativeLoot);
    ready = step && loot && nativeStep && nativeLoot;
    unavailable = !ready;
    if (!(step && nativeStep)) installFailure = std::string(stepName) + (step ? " came up table-only" : " could not be hooked");
    else if (!(loot && nativeLoot)) installFailure = std::string(lootName) + (loot ? " came up table-only" : " could not be hooked");
    if (!ready) Out("miningore: unavailable - both native hooks are required; ore amount stays x1");
    return ready;
}

// Everything the extra rolls need, once a session: the step/loot pair (whose
// result sets the multiplier's readiness as it always has), the four
// pass-through detours and the shared CombatText detour. All seven must be
// native; the first that is not is named in `rollsFailure`.
inline bool InstallRolls() {
    if (rollsInstallTried) return rollsReady;
    rollsInstallTried = true;
    std::string failure;
    if (!Install()) failure = installFailure.empty() ? "the mining step/loot hooks are unavailable" : installFailure;
    for (size_t i = 0; failure.empty() && i < silencedScripts.size(); ++i) {
        SilencedScript& script = silencedScripts[i];
        const bool ok = HookOneScript(script.name, script.hookId, (PVOID)kSilencedHooks[i], &script.original, &script.native);
        if (!(ok && script.native)) failure = std::string(script.name) + (ok ? " came up table-only" : " could not be hooked");
    }
    if (failure.empty() && !CombatText::Install())
        failure = std::string(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CombatText))
            + (CombatText::installed ? " came up table-only" : " could not be hooked");
    rollsReady = failure.empty();
    rollsUnavailable = !rollsReady;
    rollsFailure = failure;
    return rollsReady;
}

#ifndef FORGEPACT_RELEASE
// ===== Research only: `miningrolls stat`, `stats` and `dig` =====
// `dig` queues one node the way MinerHelmetMod.hpp's Vein Resonance queues a
// vein (that header is included after this one, so it carries its own copy):
// `miningQue` raised and `miningActivateDistance` widened for at most 90
// frames, then released by DigTick. It is what keeps a live check back-end only.
inline constexpr unsigned kDigFrames = 90;
inline constexpr double kDigReach = 4096;
inline constexpr int kDigMaxNodesScanned = 512;
struct QueuedDig { bool active; int64_t room; double id; unsigned frame; double savedDistance; RValue inst; };
inline QueuedDig queuedDig{ false, 0, -1, 0, 16, RValue() };
inline unsigned digFrame = 0;

inline bool ResearchNumber(const RValue& v, double& d) {
    if (v.m_Kind != VALUE_REAL && v.m_Kind != VALUE_INT32 && v.m_Kind != VALUE_INT64
        && v.m_Kind != VALUE_BOOL && v.m_Kind != VALUE_REF) return false;
    d = v.ToDouble();
    return std::isfinite(d);
}
inline double ResearchNodeNumber(const RValue& node, const char* name, double fallback) {
    try {
        if (!g_Yytk->CallBuiltin("variable_instance_exists", {node, RValue(name)}).ToBoolean()) return fallback;
        double d;
        if (ResearchNumber(g_Yytk->CallBuiltin("variable_instance_get", {node, RValue(name)}), d)) return d;
    } catch (...) {}
    return fallback;
}
inline bool ResearchPosition(const RValue& instance, double& x, double& y) {
    return ResearchNumber(g_Yytk->CallBuiltin("variable_instance_get", {instance, RValue("x")}), x)
        && ResearchNumber(g_Yytk->CallBuiltin("variable_instance_get", {instance, RValue("y")}), y);
}
inline double ResearchMiningLevel() {
    try {
        double d;
        if (ResearchNumber(g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_GetMiningLevel.data(), {}), d) && d >= 0) return d;
    } catch (...) {}
    return NAN;
}
// `miningReq` holds a handle into the game's protected store; GPV resolves it.
inline double ResearchNodeRequirement(const RValue& node) {
    try {
        if (!g_Yytk->CallBuiltin("variable_instance_exists", {node, RValue("miningReq")}).ToBoolean()) return NAN;
        RValue handle = g_Yytk->CallBuiltin("variable_instance_get", {node, RValue("miningReq")});
        double d;
        if (ResearchNumber(g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_GPV.data(), {handle}), d) && d >= 0) return d;
    } catch (...) {}
    return NAN;
}

inline void ReleaseDig(bool completed) {
    if (!queuedDig.active) return;
    queuedDig.active = false;
    try {
        if (!g_Yytk->CallBuiltin("instance_exists", {queuedDig.inst}).ToBoolean()) return;
        if (snapshots.size() < kMaxSnapshots) {
            std::string line = std::string(completed ? "dig done" : "dig released") + " node "
                + std::to_string((long long)queuedDig.id) + ":";
            for (const char* name : {"hp", "miningQue", "miningActive", "stop", "range", "miningPlayer", "sprite_index"})
                line += std::string(" ") + name + "=" + DescribeNodeValue(
                    g_Yytk->CallBuiltin("variable_instance_exists", {queuedDig.inst, RValue(name)}).ToBoolean()
                        ? g_Yytk->CallBuiltin("variable_instance_get", {queuedDig.inst, RValue(name)}) : RValue());
            snapshots.push_back(line);
        }
        g_Yytk->CallBuiltin("variable_instance_set", {queuedDig.inst, RValue("miningActivateDistance"), RValue(queuedDig.savedDistance)});
        if (!completed) g_Yytk->CallBuiltin("variable_instance_set", {queuedDig.inst, RValue("miningQue"), RValue(false)});
    } catch (...) {}
    Out(std::string("miningrolls: dig node ") + std::to_string((long long)queuedDig.id)
        + (completed ? " completed; released" : " did not complete; released"));
}

// Called every frame from the plugin's frame callback (research build only).
inline void DigTick() {
    ++digFrame;
    if (!queuedDig.active) return;
    if (CurrentRoomKey() != queuedDig.room) { queuedDig.active = false; return; }
    const double hp = ResearchNodeNumber(queuedDig.inst, "hp", 1);
    if (hp == 0) { ReleaseDig(true); return; }
    if (digFrame - queuedDig.frame >= kDigFrames) ReleaseDig(false);
}

inline void Dig() {
    // The pass-through detours' own counters are the positive control for
    // the silenced counts, so the research dig installs them even at rolls 1.
    const bool counters = InstallRolls();
    if (queuedDig.active) { Out("miningrolls: dig - node " + std::to_string((long long)queuedDig.id) + " is still queued"); return; }
    try {
        RValue player;
        if (!HhResolveLocalPlayer(player)) { Out("miningrolls: dig - local player unavailable; nothing queued"); return; }
        double px, py;
        if (!ResearchPosition(player, px, py)) { Out("miningrolls: dig - player position unreadable; nothing queued"); return; }
        const double level = ResearchMiningLevel();
        if (!std::isfinite(level)) { Out("miningrolls: dig - mining level unreadable; nothing queued"); return; }
        const RValue object = g_Yytk->CallBuiltin("asset_get_index", {RValue("Mining_Node_obj")});
        if (object.ToDouble() < 0) { Out("miningrolls: dig - Mining_Node_obj unknown; nothing queued"); return; }
        int count = static_cast<int>(g_Yytk->CallBuiltin("instance_number", {object}).ToDouble());
        if (count > kDigMaxNodesScanned) count = kDigMaxNodesScanned;
        double bestDistance = kDigReach + 1, bestId = -1;
        RValue best;
        int seen = 0, depleted = 0, busy = 0, tooHard = 0, tooFar = 0;
        for (int i = 0; i < count; ++i) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", {object, RValue((double)i)});
            if (!g_Yytk->CallBuiltin("instance_exists", {inst}).ToBoolean()) continue;
            double id = -1;
            if (!ResearchNumber(g_Yytk->CallBuiltin("variable_instance_get", {inst, RValue("id")}), id) || id <= 0) continue;
            ++seen;
            double nx, ny;
            if (!ResearchPosition(inst, nx, ny)) continue;
            const double distance = std::hypot(nx - px, ny - py);
            if (distance > kDigReach) { ++tooFar; continue; }
            if (ResearchNodeNumber(inst, "hp", 0) <= 0) { ++depleted; continue; }
            if (ResearchNodeNumber(inst, "miningActive", 1) != 0 || ResearchNodeNumber(inst, "miningQue", 1) != 0) { ++busy; continue; }
            const double requirement = ResearchNodeRequirement(inst);
            if (!std::isfinite(requirement) || requirement > level) { ++tooHard; continue; }
            if (distance < bestDistance) { bestDistance = distance; bestId = id; best = inst; }
        }
        if (bestId < 0) {
            Out("miningrolls: dig - no node queued (nodes=" + std::to_string(seen) + " farther than 4096 px="
                + std::to_string(tooFar) + " depleted=" + std::to_string(depleted) + " busy=" + std::to_string(busy)
                + " above mining level " + std::to_string((int)level) + "=" + std::to_string(tooHard) + ")");
            return;
        }
        const double saved = ResearchNodeNumber(best, "miningActivateDistance", 16);
        g_Yytk->CallBuiltin("variable_instance_set", {best, RValue("miningActivateDistance"), RValue(kDigReach)});
        g_Yytk->CallBuiltin("variable_instance_set", {best, RValue("miningQue"), RValue(true)});
        queuedDig = { true, CurrentRoomKey(), bestId, digFrame, saved, best };
        Out("miningrolls: dig queued node " + std::to_string((long long)bestId) + " at "
            + std::to_string((int)bestDistance) + " px (rolls=" + std::to_string(rolls)
            + (counters ? "" : "; pass-through counters unavailable - " + rollsFailure) + ")");
    } catch (...) { Out("miningrolls: dig failed - nothing queued"); }
}

// Ten stat queries the dig's bonus finds and the node's composition read,
// through the game's own ReturnSpecificStat, in the query shape
// hs_game_sdk/reward_stats.hpp records, with the local player as self.
inline void ReadStats() {
    RValue player;
    CInstance* self = HhResolveLocalPlayer(player) ? HhResolveInstance(player) : nullptr;
    if (!self) { Out("miningrolls: stats - local player unavailable"); return; }
    for (int id : {692, 693, 694, 695, 696, 697, 698, 699, 700, 703}) {
        std::string value = "!";
        try {
            RValue result;
            if (AurieSuccess(g_Yytk->CallGameScriptEx(result, HeroSiege::Scripts::gml_Script_ReturnSpecificStat.data(),
                    self, self, { RValue(1.0), RValue((double)id), RValue(0.0), RValue(), RValue() })))
                value = DescribeNodeValue(result);
        } catch (...) {}
        Out("miningrolls: id=" + std::to_string(id) + " value=" + value);
    }
}

inline void RollsStat() {
    uint64_t xpPassed = 0, xpSilenced = 0;
    // Mining XP, character XP and guild XP; quests are listed on their own.
    for (size_t i = 0; i < 3; ++i) { xpPassed += silencedScripts[i].passed; xpSilenced += silencedScripts[i].silenced; }
    std::string line = "miningrolls: rolls=" + std::to_string(rolls) + " rollsReady=" + std::to_string(rollsReady)
        + " steps=" + std::to_string(stepCalls) + " extraRuns=" + std::to_string(extraRuns)
        + " extraRunsUnpaid=" + std::to_string(extraRunsUnpaid) + " xpPassed=" + std::to_string(xpPassed)
        + " xpSilenced=" + std::to_string(xpSilenced);
    for (const auto& script : silencedScripts)
        line += std::string(" ") + script.name + "=" + std::to_string(script.passed) + "/" + std::to_string(script.silenced);
    line += " CombatText=" + std::to_string(CombatText::passedCalls) + "/" + std::to_string(CombatText::silencedCalls);
    if (rollsUnavailable) line += " unavailable=\"" + rollsFailure + "\"";
    Out(line);
    for (const auto& snapshot : snapshots) Out("miningrolls: node " + snapshot);
}
#endif

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

// `miningrolls N`: 1 is vanilla and installs nothing; above 1 the extra rolls
// arm only if all seven detours they need are native.
inline void RollsCommand(const std::string& text) {
#ifndef FORGEPACT_RELEASE
    if (text == "stat") { RollsStat(); return; }
    if (text == "stats") { ReadStats(); return; }
    if (text == "dig") { Dig(); return; }
#endif
    std::istringstream input(text);
    int value = 0;
    std::string extra;
    if (!(input >> value) || (input >> extra) || value < 1 || value > kMaxRolls) {
        Out("miningrolls: use a whole-number roll count from 1 to 10"); return;
    }
    if (value > 1 && !InstallRolls()) {
        rolls = 1;
        Out("miningrolls: unavailable - " + rollsFailure + "; each dig pays out once, the ore multiplier is unaffected");
        return;
    }
    rolls = value;
    Out("miningrolls: x" + std::to_string(value)
        + (value == 1 ? " (vanilla)" : " (each dig rolled " + std::to_string(value) + " times)"));
}
} // namespace ForgePact::MiningOre
