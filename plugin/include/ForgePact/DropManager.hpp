#pragma once

#include "Common.hpp"

namespace ForgePact {

// Drop-rate multipliers ("dropmult <name> <mult>", "dropstats"): each hook
// calls the game's own drop roll N times instead of once, so every extra copy
// is the game's own dice, not a synthetic roll.
//
// DropRelic is deliberately NOT here even though the panel treats it as one
// more "dropmult" target: Hook_DropRelic is a shared chokepoint also used by
// RelicFilterMod's max-relic exclusion, and splitting a single installed hook
// across two classes is a bigger, riskier step than one module's worth of
// work (the same reasoning that kept it out of RelicFilterMod earlier).
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

private:
    DropManager() = default;
    bool m_HooksInstalled{ false };

#define FP_DROP_HOOK(NAME) \
    PFUNC_YYGMLScript m_Orig_##NAME{ nullptr }; \
    volatile long m_Cnt_##NAME{ 0 }; \
    int m_Mult_##NAME{ 1 }; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        auto& mgr = Instance(); \
        BP_DIAG_INCREMENT(mgr.m_Cnt_##NAME); \
        for (int i = 1; i < mgr.m_Mult_##NAME; i++) { RValue t; if (mgr.m_Orig_##NAME) mgr.m_Orig_##NAME(S, O, t, argc, A); } \
        RValue& _res = mgr.m_Orig_##NAME ? mgr.m_Orig_##NAME(S, O, R, argc, A) : R; \
        BP_LOGDROP(#NAME, _res, argc, A); \
        return _res; \
    }

    FP_DROP_HOOK(DropBossGems)
    FP_DROP_HOOK(DropDungeonKeys)
    FP_DROP_HOOK(DropBossRunes)
    FP_DROP_HOOK(DropBattleFragments)
    FP_DROP_HOOK(DropDimensionalShard)
    FP_DROP_HOOK(DropBifrostKey)
    FP_DROP_HOOK(DropGold)
    FP_DROP_HOOK(DropItemBoss)
    FP_DROP_HOOK(DropItem)
    FP_DROP_HOOK(CreateItemDrop)
    FP_DROP_HOOK(DropItemAngelic)
    FP_DROP_HOOK(DropAngelicKey)
    FP_DROP_HOOK(DropAngelicCharm)
    FP_DROP_HOOK(DropMonsterGold)
    FP_DROP_HOOK(DropChaosKey)
    FP_DROP_HOOK(DropRubyKey)
    FP_DROP_HOOK(DropOres)
    FP_DROP_HOOK(DropOreMaterials)
#undef FP_DROP_HOOK

    // DropKeys carries real traffic (measured: DropGold 4 calls, DropDungeonKeys
    // never called - keys actually come from here) and gets an extra research
    // diagnostic in non-release builds: which key was chosen and in which
    // room, logged to bp_ipc/keychoice.txt. The multiply/log core is identical
    // either way.
    PFUNC_YYGMLScript m_Orig_DropKeys{ nullptr };
    volatile long m_Cnt_DropKeys{ 0 };
    int m_Mult_DropKeys{ 1 };
    static RValue& Hook_DropKeys(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
        auto& mgr = Instance();
        BP_DIAG_INCREMENT(mgr.m_Cnt_DropKeys);
        for (int i = 1; i < mgr.m_Mult_DropKeys; i++) { RValue t; if (mgr.m_Orig_DropKeys) mgr.m_Orig_DropKeys(S, O, t, argc, A); }
        RValue& _res = mgr.m_Orig_DropKeys ? mgr.m_Orig_DropKeys(S, O, R, argc, A) : R;
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
            RValue rm = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") });
            std::ofstream f(IPC_DIR + "\\keychoice.txt", std::ios::app);
            f << "DropKeys -> " << ad << "   room=" << (int)rm.ToDouble() << "\n";
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
