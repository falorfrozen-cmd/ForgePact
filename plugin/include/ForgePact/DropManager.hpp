#pragma once

#include "Common.hpp"
#include "IncidentMonitor.hpp"
#include <hs_game_sdk/reward_scope.hpp>

// The incident monitor's per-mod scope (issue #76): every drop hook's time is
// counted as `drops`, one line at the top of each body. tests/
// drop_gold_harness.cpp splices this header without its #include lines, and
// so without IncidentMonitor.hpp: there the scope compiles to nothing. The
// plugin always has the real one. Every call into the game's original runs
// inside FP_DROP_GAME_ORIGINAL, so `drops` counts only our own code (the
// owner, 2026-10-02); without the header it is the bare call.
#ifdef FORGEPACT_INCIDENT_MONITOR_HPP
#define FP_DROP_INCIDENT_SCOPE() ::ForgePact::Incident::IncidentScope incidentScope(::ForgePact::Incident::Mod::drops)
#define FP_DROP_GAME_ORIGINAL(call) FP_GAME_ORIGINAL(call)
#else
#define FP_DROP_INCIDENT_SCOPE() ((void)0)
#define FP_DROP_GAME_ORIGINAL(call) (call)
#endif

namespace ForgePact {

// Drop-rate multipliers ("dropmult <name> <mult>", "dropstats"): each hook
// calls the game's own drop roll N times instead of once, so every extra copy
// is the game's own dice, not a synthetic roll.
//
// Gold is the exception (#77): "dropmult gold N" multiplies the amount of the
// one coin the game creates, not the number of coins. See Hook_DropGold below.
//
// DropRelic is NOT here even though the panel treats it as one more
// "dropmult" target. Hook_DropRelic used to be a shared chokepoint with
// RelicFilterMod's max-relic exclusion, and splitting a single installed hook
// across two classes was a bigger, riskier step than one module's worth of
// work. Since #125 the relic filter uses GetRelicQuest instead, so moving
// DropRelic here is now only a cleanup, not yet done.
// ModuleMain's InstallDropMultHooks()/SetDropMult() call into this class for
// the 19 hooks below and still install/set DropRelic themselves.
class DropManager {
public:
    static DropManager& Instance() {
        static DropManager s_Instance;
        return s_Instance;
    }

    // Installs all 19 domain hooks (idempotent). Does NOT install DropRelic -
    // see the class comment above.
    void InstallHooks() {
        if (m_HooksInstalled) return;
        m_HooksInstalled = true;
        HookOneScript("DropBossGems",        "bp_dgems",    (void*)Hook_DropBossGems,        &m_Orig_DropBossGems);
        HookOneScript("DropDungeonKeys",     "bp_ddkeys",   (void*)Hook_DropDungeonKeys,     &m_Orig_DropDungeonKeys);
        HookOneScript("DropBossRunes",       "bp_drunes",   (void*)Hook_DropBossRunes,       &m_Orig_DropBossRunes);
        HookOneScript("DropBattleFragments", "bp_dfrag",    (void*)Hook_DropBattleFragments, &m_Orig_DropBattleFragments);
        HookOneScript("DropDimensionalShard","bp_dshard",   (void*)Hook_DropDimensionalShard,&m_Orig_DropDimensionalShard);
        HookOneScript("DropBifrostKey",      "bp_dbifrost", (void*)Hook_DropBifrostKey,      &m_Orig_DropBifrostKey);
        HookOneScript("DropGold",            "bp_dgold",    (void*)Hook_DropGold,            &m_Orig_DropGold);
        HookOneScript("DropItemBoss",        "bp_dibos",    (void*)Hook_DropItemBoss,        &m_Orig_DropItemBoss);
        HookOneScript("DropItem",            "bp_ditem",    (void*)Hook_DropItem,            &m_Orig_DropItem);
        HookOneScript("CreateItemDrop",      "bp_citemd",   (void*)Hook_CreateItemDrop,      &m_Orig_CreateItemDrop);
        HookOneScript("DropItemAngelic",     "bp_dangit",   (void*)Hook_DropItemAngelic,     &m_Orig_DropItemAngelic);
        HookOneScript("DropAngelicKey",      "bp_dangkey",  (void*)Hook_DropAngelicKey,      &m_Orig_DropAngelicKey);
        HookOneScript("DropAngelicCharm",    "bp_dangchm",  (void*)Hook_DropAngelicCharm,    &m_Orig_DropAngelicCharm);
        HookOneScript("DropMonsterGold",     "bp_dmgold",   (void*)Hook_DropMonsterGold,     &m_Orig_DropMonsterGold);
        InstallDropKeysHook();
        HookOneScript("DropChaosKey",        "bp_dckey",    (void*)Hook_DropChaosKey,        &m_Orig_DropChaosKey);
        HookOneScript("DropRubyKey",         "bp_drkey",    (void*)Hook_DropRubyKey,         &m_Orig_DropRubyKey);
        HookOneScript("DropOres",            "bp_dores",    (void*)Hook_DropOres,            &m_Orig_DropOres);
        HookOneScript("DropOreMaterials",    "bp_doremat",  (void*)Hook_DropOreMaterials,    &m_Orig_DropOreMaterials);
    }

    // "dropmult <name> <n>". Does not itself install hooks - ModuleMain's
    // SetDropMult calls InstallDropMultHooks() first (which also handles the
    // shared DropRelic hook), matching this class's own InstallHooks() rule.
    void SetMultiplier(const std::string& name, int n) {
        std::string l = Lower(name);
        if (n < 1) n = 1;
        if (l == "bossgems" || l == "gems") { m_Mult_DropBossGems = n; }
        else if (l == "dungeonkeys" || l == "keys") {
            m_Mult_DropDungeonKeys = n; m_Mult_DropKeys = n;
            m_Mult_DropChaosKey = n; m_Mult_DropRubyKey = n;
        }
        else if (l == "bossrunes" || l == "runes") { m_Mult_DropBossRunes = n; }
        else if (l == "battlefragments" || l == "frags") { m_Mult_DropBattleFragments = n; }
        else if (l == "dimshard" || l == "shards") { m_Mult_DropDimensionalShard = n; }
        else if (l == "bifrost") { m_Mult_DropBifrostKey = n; }
        else if (l == "gold") { m_Mult_DropGold = n; m_Mult_DropMonsterGold = n; }
        else if (l == "bossitem" || l == "itemboss") { m_Mult_DropItemBoss = n; }
        else if (l == "item") { m_Mult_DropItem = n; }
        else if (l == "createitem") { m_Mult_CreateItemDrop = n; }
        else if (l == "angelic" || l == "angelicitem") { m_Mult_DropItemAngelic = n; }
        else if (l == "angelickey") { m_Mult_DropAngelicKey = n; }
        else if (l == "angeliccharm") { m_Mult_DropAngelicCharm = n; }
        else if (l == "ore" || l == "ores") { m_Mult_DropOres = n; m_Mult_DropOreMaterials = n; }
        else {
            // "relic" is intentionally absent from this list - ModuleMain's
            // SetDropMult still owns g_mult_DropRelic directly.
            Out("dropmult: unknown '" + name + "' (relic|gems|keys|runes|frags|shards|bifrost|gold|bossitem|item|createitem|angelic|angelickey|angeliccharm|ore)");
            return;
        }
        Out("dropmult " + name + " -> " + std::to_string(n));
    }

    // Narrow setters for the Companion mechanic's own bundle of "beneficial
    // buffs" (3x item/boss-item/gold, 2x boss gems/dungeon keys - relic is
    // set directly by ModuleMain's CompSetBuffs, same as everywhere else in
    // this class). Deliberately NOT routed through SetMultiplier(name, n):
    // SetMultiplier("gold", n) also sets MonsterGold, which CompSetBuffs has
    // never touched, and reusing it here would have silently widened what
    // Companion mode boosts. No install call either - CompSetBuffs never
    // triggered one, so this preserves that (relies on hooks already being
    // installed some other way, exactly as before).
    void SetDropItemMultRaw(int n) { m_Mult_DropItem = n; }
    void SetDropItemBossMultRaw(int n) { m_Mult_DropItemBoss = n; }
    void SetDropGoldMultRaw(int n) { m_Mult_DropGold = n; }
    void SetDropBossGemsMultRaw(int n) { m_Mult_DropBossGems = n; }
    void SetDropDungeonKeysMultRaw(int n) { m_Mult_DropDungeonKeys = n; }

    // "dropstats": the 16 counters the original status line prints (Angelic
    // items/keys/charms are tracked but not shown here, matching the
    // pre-migration behaviour exactly).
    void PrintStats() {
        char b[400];
        sprintf_s(b, "          BossGems c=%ld x%d | DungeonKeys c=%ld x%d | BossRunes c=%ld x%d",
            m_Cnt_DropBossGems, m_Mult_DropBossGems, m_Cnt_DropDungeonKeys, m_Mult_DropDungeonKeys,
            m_Cnt_DropBossRunes, m_Mult_DropBossRunes);
        Out(b);
        sprintf_s(b, "          BattleFrag c=%ld x%d | DimShard c=%ld x%d | Bifrost c=%ld x%d | Gold c=%ld x%d",
            m_Cnt_DropBattleFragments, m_Mult_DropBattleFragments, m_Cnt_DropDimensionalShard, m_Mult_DropDimensionalShard,
            m_Cnt_DropBifrostKey, m_Mult_DropBifrostKey, m_Cnt_DropGold, m_Mult_DropGold);
        Out(b);
        sprintf_s(b, "          ItemBoss c=%ld x%d | Item c=%ld x%d | CreateItemDrop c=%ld x%d",
            m_Cnt_DropItemBoss, m_Mult_DropItemBoss, m_Cnt_DropItem, m_Mult_DropItem,
            m_Cnt_CreateItemDrop, m_Mult_CreateItemDrop);
        Out(b);
        sprintf_s(b, "          Ores c=%ld x%d | OreMaterials c=%ld x%d",
            m_Cnt_DropOres, m_Mult_DropOres, m_Cnt_DropOreMaterials, m_Mult_DropOreMaterials);
        Out(b);
        // Asil trafigin gectigi yollar - DropGold/DropDungeonKeys neredeyse hic
        // cagrilmiyor, gercek altin ve anahtarlar buradan geliyor.
        sprintf_s(b, "          MonsterGold c=%ld x%d | Keys c=%ld x%d | ChaosKey c=%ld x%d | RubyKey c=%ld x%d",
            m_Cnt_DropMonsterGold, m_Mult_DropMonsterGold, m_Cnt_DropKeys, m_Mult_DropKeys,
            m_Cnt_DropChaosKey, m_Mult_DropChaosKey, m_Cnt_DropRubyKey, m_Mult_DropRubyKey);
        Out(b);
    }

    // How many DropGold calls reached the hook with a multiplier above 1 and
    // an amount it could not scale (no argument at the index, not a number,
    // or not finite); each one kept the game's own amount.
    long GoldUnscaledCount() const { return m_Cnt_GoldUnscaled; }

#ifndef FORGEPACT_RELEASE
    // Research build only (docs/angelic-roll-hook-research.md): the saved
    // original of five of the hooks below, by the name each was installed
    // under, so a research instrument can tell a trampoline (the hook is
    // native) from the game's own function (it fell back to table-only).
    // angelicprobe only reads it; gambaprobe (docs/gamba-machine-research.md)
    // splices its own detour into DropItem's slot when it holds a trampoline,
    // so the hook below calls the probe, which calls the trampoline. nullptr
    // for any other name.
    PFUNC_YYGMLScript* ResearchHeldOriginal(std::string_view shortName) {
        if (shortName == "DropItem")         return &m_Orig_DropItem;
        if (shortName == "DropItemBoss")     return &m_Orig_DropItemBoss;
        if (shortName == "DropItemAngelic")  return &m_Orig_DropItemAngelic;
        if (shortName == "DropAngelicKey")   return &m_Orig_DropAngelicKey;
        if (shortName == "DropAngelicCharm") return &m_Orig_DropAngelicCharm;
        return nullptr;
    }
#endif

private:
    DropManager() = default;
    bool m_HooksInstalled{ false };

#define FP_DROP_HOOK(NAME) \
    PFUNC_YYGMLScript m_Orig_##NAME{ nullptr }; \
    volatile long m_Cnt_##NAME{ 0 }; \
    int m_Mult_##NAME{ 1 }; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        FP_DROP_INCIDENT_SCOPE(); \
        BP_ANGELIC_PROBE_SCOPE(#NAME, S, argc, A); \
        auto& mgr = Instance(); \
        BP_DIAG_INCREMENT(mgr.m_Cnt_##NAME); \
        for (int i = 1, n = HeroSiege::RewardScope::Active() ? 1 : mgr.m_Mult_##NAME; i < n; i++) { RValue t; if (mgr.m_Orig_##NAME) FP_DROP_GAME_ORIGINAL(mgr.m_Orig_##NAME(S, O, t, argc, A)); } \
        RValue& _res = mgr.m_Orig_##NAME ? FP_DROP_GAME_ORIGINAL(mgr.m_Orig_##NAME(S, O, R, argc, A)) : R; \
        BP_LOGDROP(#NAME, _res, argc, A); \
        return _res; \
    }

    FP_DROP_HOOK(DropBossGems)
    FP_DROP_HOOK(DropDungeonKeys)
    FP_DROP_HOOK(DropBossRunes)
    FP_DROP_HOOK(DropBattleFragments)
    FP_DROP_HOOK(DropDimensionalShard)
    FP_DROP_HOOK(DropBifrostKey)
    FP_DROP_HOOK(DropItemBoss)
    FP_DROP_HOOK(DropItem)
    FP_DROP_HOOK(CreateItemDrop)
    FP_DROP_HOOK(DropItemAngelic)
    FP_DROP_HOOK(DropAngelicKey)
    FP_DROP_HOOK(DropAngelicCharm)
    FP_DROP_HOOK(DropChaosKey)
    FP_DROP_HOOK(DropRubyKey)
    FP_DROP_HOOK(DropOres)
    FP_DROP_HOOK(DropOreMaterials)
#undef FP_DROP_HOOK

    // Gold (#77): an amount multiplier, not a count one. DropMonsterGold
    // works out an amount and calls DropGold once, directly (the inline
    // detour sees that call); DropGold creates one coin carrying the amount it
    // was handed. Run through FP_DROP_HOOK, "dropmult gold 100" ran the
    // DropMonsterGold original 100 times and each reached a DropGold hook
    // that ran its own original 100 times: 10,000 coins for one monster's
    // gold, measured in Live 1 (2026-09-27) with an 8.4 s stall at spawn and
    // another at pickup. So both originals run exactly once, and DropGold's
    // amount argument is multiplied instead. SetMultiplier("gold") still sets
    // m_Mult_DropMonsterGold, which only "dropstats" reports.
    //
    // DropGold's argument 4 is the coin's amount: measured 2026-09-27
    // (Live 1, `goldtrace`), it was the one argument that varied per coin
    // (51, 59, 31, 29) while the others stayed constant.
    static constexpr int kDropGoldAmountArg = 4;

    PFUNC_YYGMLScript m_Orig_DropGold{ nullptr };
    volatile long m_Cnt_DropGold{ 0 };
    int m_Mult_DropGold{ 1 };
    long m_Cnt_GoldUnscaled{ 0 };
    bool m_GoldScaledLogged{ false };
    bool m_GoldUnscaledLogged{ false };

    // Per-coin record, the positive control on argument 4 being the amount
    // the game credits: the first kGoldCoinLogCount coins after each change
    // of the gold multiplier (and the first ones of a session, at x1) each log
    // the argument-4 value DropGold was handed and the value passed on, so a
    // gold reading taken before and after picking one coin up can be held
    // against its own line (x1: the delta should equal it; x100: about 100x).
    // Keyed on the configured multiplier, not the one in effect, so AFK
    // FARM's reward scope switching x100 to x1 and back does not re-arm it.
    // 0 is "never armed": the multiplier is never below 1.
    static constexpr long kGoldCoinLogCount = 8;
    int m_GoldCoinLogMult{ 0 };
    long m_GoldCoinLogged{ 0 };

    static std::string GoldNum(double v) {
        return (std::fabs(v) < 9.0e15 && v == std::floor(v))
            ? std::to_string((long long)v) : std::to_string(v);
    }
    static std::string GoldValueText(const RValue* v) {
        if (!v) return "missing";
        if (v->m_Kind == VALUE_REAL || v->m_Kind == VALUE_INT32 || v->m_Kind == VALUE_INT64) return GoldNum(v->ToDouble());
        return "kind " + std::to_string((int)v->m_Kind);
    }
    static const RValue* GoldArg(int argc, RValue** A, int i) { return (A && argc > i) ? A[i] : nullptr; }
    // `passed` is what the original is handed at argument 4: the scaled copy,
    // or the caller's own value when the hook left it alone.
    static void LogGoldCoin(DropManager& mgr, int mult, int argc, RValue** A, const RValue* passed) {
        if (mgr.m_GoldCoinLogMult != mgr.m_Mult_DropGold) {
            mgr.m_GoldCoinLogMult = mgr.m_Mult_DropGold;
            mgr.m_GoldCoinLogged = 0;
        }
        if (mgr.m_GoldCoinLogged >= kGoldCoinLogCount) return;
        ++mgr.m_GoldCoinLogged;
        Out("dropmult gold coin " + std::to_string(mgr.m_GoldCoinLogged) + "/" + std::to_string(kGoldCoinLogCount)
            + " at x" + std::to_string(mult)
            + (mult != mgr.m_Mult_DropGold ? " (reward scope; set x" + std::to_string(mgr.m_Mult_DropGold) + ")" : "")
            + ": argument " + std::to_string(kDropGoldAmountArg) + " "
            + GoldValueText(GoldArg(argc, A, kDropGoldAmountArg)) + " -> " + GoldValueText(passed)
            + " (arguments 1,2: " + GoldValueText(GoldArg(argc, A, 1)) + ", " + GoldValueText(GoldArg(argc, A, 2)) + ")");
    }

    // Gold breadcrumbs (ForgePact #173): a point immediately before and after
    // each call into the DropGold and DropMonsterGold originals, outside the
    // game-original guard, so a game that dies inside one leaves an `enter`
    // with no `done`. The including file supplies the sink before including
    // this header:
    //   FP_GOLD_CRUMB_SINK(const char* script, bool done, long ordinal,
    //                      int mult, const RValue* handed, const RValue* passed)
    // `ordinal` counts the session's calls of that hook (enter and done share
    // it), `mult` is the multiplier in force (1 inside a reward scope), and
    // for a DropGold enter `handed`/`passed` are argument 4 as the hook got it
    // and as it hands it on (nullptr when missing, and for every other point).
    // Only the research build defines a sink (crashwatch). With none, each
    // point is ((void)0) and its arguments are never evaluated, so the player
    // build compiles to what it did before the points existed.
#ifdef FP_GOLD_CRUMB_SINK
#define FP_GOLD_CRUMB_ORDINAL() static long goldCrumbCalls = 0; const long goldCrumb = ++goldCrumbCalls
#define FP_GOLD_CRUMB_ENTER(script, mult, handed, passed) FP_GOLD_CRUMB_SINK(script, false, goldCrumb, mult, handed, passed)
#define FP_GOLD_CRUMB_DONE(script, mult) FP_GOLD_CRUMB_SINK(script, true, goldCrumb, mult, nullptr, nullptr)
#else
#define FP_GOLD_CRUMB_ORDINAL() ((void)0)
#define FP_GOLD_CRUMB_ENTER(script, mult, handed, passed) ((void)0)
#define FP_GOLD_CRUMB_DONE(script, mult) ((void)0)
#endif

    static RValue& Hook_DropGold(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        FP_DROP_INCIDENT_SCOPE();
        auto& mgr = Instance();
        BP_DIAG_INCREMENT(mgr.m_Cnt_DropGold);
        FP_GOLD_CRUMB_ORDINAL();
        const int mult = HeroSiege::RewardScope::Active() ? 1 : mgr.m_Mult_DropGold;
        const RValue* amount = (A && argc > kDropGoldAmountArg) ? A[kDropGoldAmountArg] : nullptr;
        if (mult <= 1 || !mgr.m_Orig_DropGold) {
            if (mgr.m_Orig_DropGold) {
                LogGoldCoin(mgr, mult, argc, A, amount);
                FP_GOLD_CRUMB_ENTER("DropGold", mult, amount, amount);
            }
            RValue& _res = mgr.m_Orig_DropGold ? FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropGold(S, O, R, argc, A)) : R;
            if (mgr.m_Orig_DropGold) FP_GOLD_CRUMB_DONE("DropGold", mult);
            BP_LOGDROP("DropGold", _res, argc, A);
            return _res;
        }
        const bool numeric = amount && (amount->m_Kind == VALUE_REAL
                                        || amount->m_Kind == VALUE_INT32
                                        || amount->m_Kind == VALUE_INT64);
        const double value = numeric ? amount->ToDouble() : 0.0;
        if (!numeric || !std::isfinite(value)) {
            // Refused, and said once: the coin keeps the game's own amount.
            ++mgr.m_Cnt_GoldUnscaled;
            if (!mgr.m_GoldUnscaledLogged) {
                mgr.m_GoldUnscaledLogged = true;
                Out("dropmult gold: x" + std::to_string(mult) + " not applied - DropGold's amount (argument "
                    + std::to_string(kDropGoldAmountArg) + ") was "
                    + (!amount ? std::string("missing") : "not a finite number (kind " + std::to_string((int)amount->m_Kind) + ")")
                    + "; the coin keeps the game's amount");
            }
            LogGoldCoin(mgr, mult, argc, A, amount);
            FP_GOLD_CRUMB_ENTER("DropGold", mult, amount, amount);
            RValue& _res = FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropGold(S, O, R, argc, A));
            FP_GOLD_CRUMB_DONE("DropGold", mult);
            BP_LOGDROP("DropGold", _res, argc, A);
            return _res;
        }
        // A copy of the argument array with the amount slot replaced; the
        // caller's own RValue is never written.
        RValue scaled(value * (double)mult);
        std::vector<RValue*> args(A, A + argc);
        args[kDropGoldAmountArg] = &scaled;
        if (!mgr.m_GoldScaledLogged) {
            // The line carries the first coin's numbers so a report (or a live
            // capture) can hold them against the gold the game credits at
            // pickup: this hook only proves what DropGold was handed, not that
            // DropGold credits argument 4 unchanged. The per-coin lines
            // (LogGoldCoin) are what a pickup's gold delta is paired with.
            mgr.m_GoldScaledLogged = true;
            Out("dropmult gold: x" + std::to_string(mult) + " applied to the coin's amount (one coin per drop): first coin "
                + GoldNum(value) + " -> " + GoldNum(scaled.ToDouble()));
        }
        LogGoldCoin(mgr, mult, argc, A, &scaled);
        FP_GOLD_CRUMB_ENTER("DropGold", mult, amount, &scaled);
        RValue& _res = FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropGold(S, O, R, argc, args.data()));
        FP_GOLD_CRUMB_DONE("DropGold", mult);
        BP_LOGDROP("DropGold", _res, argc, args.data());
        return _res;
    }

    PFUNC_YYGMLScript m_Orig_DropMonsterGold{ nullptr };
    volatile long m_Cnt_DropMonsterGold{ 0 };
    int m_Mult_DropMonsterGold{ 1 };
    static RValue& Hook_DropMonsterGold(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        FP_DROP_INCIDENT_SCOPE();
        auto& mgr = Instance();
        BP_DIAG_INCREMENT(mgr.m_Cnt_DropMonsterGold);
        FP_GOLD_CRUMB_ORDINAL();
        // Once, whatever the multiplier: its one coin is scaled in DropGold.
        if (mgr.m_Orig_DropMonsterGold)
            FP_GOLD_CRUMB_ENTER("DropMonsterGold", HeroSiege::RewardScope::Active() ? 1 : mgr.m_Mult_DropGold, nullptr, nullptr);
        RValue& _res = mgr.m_Orig_DropMonsterGold ? FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropMonsterGold(S, O, R, argc, A)) : R;
        if (mgr.m_Orig_DropMonsterGold)
            FP_GOLD_CRUMB_DONE("DropMonsterGold", HeroSiege::RewardScope::Active() ? 1 : mgr.m_Mult_DropGold);
        BP_LOGDROP("DropMonsterGold", _res, argc, A);
        return _res;
    }

    // DropKeys carries real traffic (measured: DropGold 4 calls, DropDungeonKeys
    // never called - keys actually come from here) and gets an extra research
    // diagnostic in non-release builds: which key was chosen and in which
    // room, logged to bp_ipc/keychoice.txt. The multiply/log core is identical
    // either way.
    PFUNC_YYGMLScript m_Orig_DropKeys{ nullptr };
    volatile long m_Cnt_DropKeys{ 0 };
    int m_Mult_DropKeys{ 1 };
    static RValue& Hook_DropKeys(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        FP_DROP_INCIDENT_SCOPE();
        auto& mgr = Instance();
        BP_DIAG_INCREMENT(mgr.m_Cnt_DropKeys);
        for (int i = 1, n = HeroSiege::RewardScope::Active() ? 1 : mgr.m_Mult_DropKeys; i < n; i++) { RValue t; if (mgr.m_Orig_DropKeys) FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropKeys(S, O, t, argc, A)); }
        RValue& _res = mgr.m_Orig_DropKeys ? FP_DROP_GAME_ORIGINAL(mgr.m_Orig_DropKeys(S, O, R, argc, A)) : R;
        BP_LOGDROP("DropKeys", _res, argc, A);
#ifndef FORGEPACT_RELEASE
        // Hangi anahtar secildi, neden - kullanici gozlemi: yalnizca Chaos/Basic/Crystal dusuyor.
        try {
            std::string ad = "?";
            if (_res.m_Kind == VALUE_OBJECT) {
                RValue info = g_Yytk->CallBuiltin("variable_struct_get", { _res, RValue("itemInfoStruct") });
                if (info.m_Kind == VALUE_OBJECT) {
                    RValue nm = g_Yytk->CallBuiltin("variable_struct_get", { info, RValue("28") });
                    ad = nm.ToString();
                }
            }
            // `room` is a GameMaker built-in, not a global: variable_global_get
            // answered undefined, and converting it raised a runner error on
            // every DropKeys call (#144). Read it as the built-in, by name.
            std::string room = "(unreadable)";
            RValue rm;
            if (AurieSuccess(g_Yytk->GetBuiltin("room", nullptr, NULL_INDEX, rm)))
                room = g_Yytk->CallBuiltin("room_get_name", { rm }).ToString();
            std::ofstream f(IPC_DIR + "\\keychoice.txt", std::ios::app);
            f << "DropKeys -> " << ad << "   room=" << room << "\n";
            f.flush();
        } catch (...) {}
#endif
        return _res;
    }
    void InstallDropKeysHook() {
        HookOneScript("DropKeys", "bp_dkeys", (void*)Hook_DropKeys, &m_Orig_DropKeys);
    }
};

} // namespace ForgePact
