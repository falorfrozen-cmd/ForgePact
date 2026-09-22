// The Python test inserts the real plugin functions below. Only the game API is
// replaced; no game process, character, installed DLL or release asset is touched.
#include <algorithm>
#include <atomic>
#include <deque>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = -1;
    CInstance* instance = nullptr;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const {
        if (m_Kind == VALUE_OBJECT) throw std::runtime_error("object is not an instance id");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
    CInstance* ToInstance() const { return instance; }
};
struct CInstance {
    int id;
    int object;
    bool alive = true;
    double enemyRarity = -1, x = 0, y = 0;   // what the kill drops read off the enemy
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
using AurieStatus = int;
static bool AurieSuccess(int status) { return status == 0; }
static long InterlockedIncrement(volatile long* value) { return ++*const_cast<long*>(value); }
static long InterlockedExchange(volatile long* value, long next) { const long old = *value; *const_cast<long*>(value) = next; return old; }
static std::vector<CInstance*> instances;
static CInstance* findInstance(int id) {
    for (auto* instance : instances) if (instance->id == id && instance->alive) return instance;
    return nullptr;
}
static int returnedIdKind = VALUE_REAL;
static bool sdkLookupWorks = true;
static bool nativeLookupWorks = true;
static CInstance* nativeLookupOverride = nullptr;
struct FakeRunner {
    CInstance* resolve(const RValue& value) {
        if (value.m_Kind == VALUE_OBJECT) return value.instance && value.instance->alive ? value.instance : nullptr;
        return findInstance(static_cast<int>(value.number));
    }
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        std::string key(name);
        if (key == "@@GetInstance@@") {
            if (!nativeLookupWorks) return RValue();
            return RValue(nativeLookupOverride ? nativeLookupOverride : resolve(args[0]));
        }
        if (key == "asset_get_index") return RValue(args[0].text == "Player_obj" ? 10.0 : -1.0);
        if (key == "object_is_ancestor") return RValue(args[0].number == 11 && args[1].number == 10 ? 1.0 : 0.0);
        auto* instance = resolve(args[0]);
        if (key == "instance_exists") return RValue(instance ? 1.0 : 0.0);
        if (key == "variable_instance_get" && instance) {
            if (args[1].text == "id") {
                RValue id(static_cast<double>(instance->id));
                id.m_Kind = returnedIdKind;
                return id;
            }
            if (args[1].text == "object_index") return RValue(static_cast<double>(instance->object));
        }
        throw std::runtime_error("invalid builtin access");
    }
    int GetInstanceObject(int32_t id, CInstance*& result) {
        result = sdkLookupWorks ? findInstance(id) : nullptr;
        return result ? 0 : -1;
    }
};
static FakeRunner runner;
static FakeRunner* g_Yytk = &runner;
static std::atomic<bool> g_HhEnabled{true};
static std::deque<int> g_HhHandledOrder;
static std::set<int> g_HhHandledIds;
static CInstance* localPlayer = nullptr;
static bool localPlayerAsObject = false;
static bool deliverySucceeds = true;
static bool deliveryThrows = false;
static int attempts = 0;
static int delivered = 0;
static CInstance* lastPlayer = nullptr;
static bool CallerIsEnemyInstance(CInstance* instance) { return instance && instance->alive && instance->object == 20; }
static bool HhResolveLocalPlayer(RValue& result, std::string*) {
    if (!localPlayer || !localPlayer->alive) return false;
    result = localPlayerAsObject ? RValue(localPlayer) : RValue(static_cast<double>(localPlayer->id));
    return true;
}
static bool HhOnKill(CInstance* player, const RValue&) {
    ++attempts;
    lastPlayer = player;
    if (deliveryThrows) throw std::runtime_error("transient game API failure");
    if (deliverySucceeds) ++delivered;
    return deliverySucceeds;
}

#define PERF_SCOPE(counter) ((void)0)
using KillHook = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);
static int originalCalls = 0;
static bool originalRemovesEnemy = false;
static RValue& originalKill(CInstance* self, CInstance*, RValue& result, int, RValue**) {
    ++originalCalls;
    if (originalRemovesEnemy) self->alive = false;
    return result;
}
static KillHook g_Orig_EnemyDestroyKillProc = originalKill;
using PVOID = void*;
static KillHook g_Orig_HhDeathEffects = originalKill;
static volatile long g_HhDeathScriptCalls = 0;
static volatile long g_HhAltTrigger = 0;
static int statusReports = 0;
static double nowMs = 0;
static double HhNowMs() { return nowMs; }
static void HeadhunterStatus(bool includeMap) { if (includeMap) throw std::runtime_error("automatic log included full mapping"); ++statusReports; }
static volatile long g_HhHookCalls = 0, g_HhLastArgc = -1;
// The kill drops (SignatureDropOnKill, AngelicDropOnKill) are the real functions. Only their
// game boundary is stubbed: the field read, the dice, the pool and the two spawns. Each spawn
// records the self it was handed, whether that instance was still alive, and whether the
// original kill proc had already run - the order the live crash report is about.
static int outLines = 0;
static bool outThrows = false;
static void Out(const std::string&) { ++outLines; if (outThrows) throw std::runtime_error("log write failed"); }
static int readsAfterOriginal = 0;
static double HhReadNumber(const RValue& value, const char* field, double fallback) {
    CInstance* instance = value.instance;
    if (!instance) return fallback;
    if (originalCalls > 0) ++readsAfterOriginal;
    const std::string name(field);
    if (name == "enemyRarity") return instance->enemyRarity;
    if (name == "x") return instance->x;
    if (name == "y") return instance->y;
    return fallback;
}
static std::mt19937& TyRng() { static std::mt19937 rng{ 7 }; return rng; }
static bool TyRoll(double pct) { return pct >= 100.0; }   // deterministic: 100 hits, anything less misses
static volatile long g_KillsSeen = 0;
static double g_AngelicDropOneIn = 0.0;
static volatile long g_AngelicDropRolls = 0, g_AngelicDropHits = 0, g_AngelicDropFails = 0;
// Old (pre-#63) signature-drop globals, kept so the old source's extracted SignatureDropOnKill
// still has something to read; new (#63) global alongside it - the union the test design calls
// for, so a baseline scenario's dropsCertain() can drive either compiled body.
static double g_SigDropPct = 0.0, g_SigDropAncientPct = 0.0;
static long g_SigDropPity = 0, g_SigDropSinceLast = 0;
static long g_SigDropRolls = 0, g_SigDropHits = 0, g_SigDropFails = 0;
static int g_SigDropNext = 0;
static int g_SigDropForce = -1;   // #63: -1 off (default), 0 force Tyrant's Crown, 1 force Headhunter
// signature: -1 = an ordinary unique; 0/1 = Tyrant's Crown / Headhunter (#63). Default member
// initializer so the existing {3,1,15,"test unique",true} initializers still compile.
struct AngelicCandidate { int type, sub, b; std::string name; bool angelic; int signature = -1; };
static std::vector<AngelicCandidate> g_AngelicPool;
static void BuildAngelicPool(bool) { if (g_AngelicPool.empty()) g_AngelicPool.push_back({ 3, 1, 15, "test unique", true }); }
struct SpawnRecord { CInstance* self; bool selfAlive; int originalCallsBefore; double x, y; int which = -1; };
static std::vector<SpawnRecord> angelicSpawns, sigSpawns;
static bool spawnThrows = false;
static bool SpawnSignatureItem(int which, double x, double y, CInstance* ctx) {
    sigSpawns.push_back({ ctx, ctx && ctx->alive, originalCalls, x, y, which });
    return true;
}
static bool SpawnAngelicItem(const AngelicCandidate&, double x, double y, CInstance* ctx) {
    angelicSpawns.push_back({ ctx, ctx && ctx->alive, originalCalls, x, y });
    if (spawnThrows) throw std::runtime_error("spawn threw");
    return true;
}
static bool g_HhHookInstalled = false;
static void* g_OrigICD = nullptr;
static void* g_OrigICL = nullptr;
static bool primaryAvailable = true, fallbackAvailable = true;
static int createInstallCalls = 0;
static bool deathScriptAvailable = true;
static bool HookOneScript(const char*, const char*, PVOID, KillHook* original) {
    *original = deathScriptAvailable ? originalKill : nullptr;
    return deathScriptAvailable;
}
static void InstallHeadhunterHook() { g_HhHookInstalled = primaryAvailable; }
static void InstallCreateHooks() {
    ++createInstallCalls;
    g_OrigICD = fallbackAvailable ? &runner : nullptr;
    g_OrigICL = fallbackAvailable ? &runner : nullptr;
}

// PRODUCTION_FUNCTIONS

static void reset() {
    returnedIdKind = VALUE_REAL;
    sdkLookupWorks = true; nativeLookupWorks = true; nativeLookupOverride = nullptr;
    g_HhEnabled = true;
    g_HhHandledIds.clear(); g_HhHandledOrder.clear();
    localPlayer = nullptr; localPlayerAsObject = false;
    deliverySucceeds = true; deliveryThrows = false;
    attempts = 0; delivered = 0; lastPlayer = nullptr;
    originalCalls = 0; originalRemovesEnemy = false;
    g_HhHookInstalled = false; g_OrigICD = nullptr; g_OrigICL = nullptr;
    primaryAvailable = true; fallbackAvailable = true; createInstallCalls = 0;
    deathScriptAvailable = true; g_Orig_HhDeathEffects = originalKill; g_HhDeathScriptCalls = 0;
    g_HhHookCalls = 0; g_HhAltTrigger = 0; statusReports = 0; nowMs = 0;
    outLines = 0; outThrows = false; readsAfterOriginal = 0; spawnThrows = false;
    g_KillsSeen = 0; g_AngelicDropOneIn = 0.0; g_AngelicDropRolls = 0; g_AngelicDropHits = 0; g_AngelicDropFails = 0;
    g_SigDropPct = 0.0; g_SigDropAncientPct = 0.0; g_SigDropPity = 0; g_SigDropSinceLast = 0;
    g_SigDropRolls = 0; g_SigDropHits = 0; g_SigDropFails = 0; g_SigDropNext = 0; g_SigDropForce = -1;
    g_AngelicPool.clear(); angelicSpawns.clear(); sigSpawns.clear();
}
// Both kill drops on at a certain hit: 1-in-1 angelic, 100 pct signature (old source) or forced
// every kill (#63 source) - drives whichever body got compiled in.
static void dropsCertain() { g_AngelicDropOneIn = 1.0; g_SigDropPct = 100.0; g_SigDropAncientPct = 100.0; g_SigDropForce = 0; }
static void killEnemy(CInstance& enemy, CInstance& player) {
    RValue result, killer(&player); RValue* arguments[] = {nullptr,nullptr,&killer};
    Hook_EnemyDestroyKillProc(&enemy,nullptr,result,3,arguments);
}
static void requireSpawnedLive(const std::vector<SpawnRecord>& spawns, CInstance* enemy, const char* what) {
    if (spawns.size() != 1) throw std::runtime_error(std::string(what) + ": expected one spawn, got " + std::to_string(spawns.size()));
    const SpawnRecord& s = spawns[0];
    if (s.self != enemy) throw std::runtime_error(std::string(what) + ": spawned with a self other than the dying enemy");
    if (s.originalCallsBefore != 0) throw std::runtime_error(std::string(what) + ": spawned after the original kill proc had run");
    if (!s.selfAlive) throw std::runtime_error(std::string(what) + ": spawned with an enemy the original had already cleaned up");
}
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    CInstance enemy{100,20}, player{200,10}, projectile{300,30}, childPlayer{201,11};
    instances = {&enemy,&player,&projectile,&childPlayer};
    reset();
    const std::string test = argv[1];
    try {
        if (test == "missing_player_retry") {
            HhSteal(&enemy, nullptr, nullptr);
            localPlayer = &player;
            HhSteal(&enemy, nullptr, nullptr);
            require(delivered == 1, "first event without a player consumed the later valid event");
        } else if (test == "failed_buff_retry") {
            deliverySucceeds = false; HhSteal(&enemy, &player, nullptr);
            deliverySucceeds = true; HhSteal(&enemy, &player, nullptr);
            require(delivered == 1 && attempts == 2, "failed buff application permanently consumed the kill");
        } else if (test == "exception_retry") {
            deliveryThrows = true; HhSteal(&enemy, &player, nullptr);
            deliveryThrows = false; HhSteal(&enemy, &player, nullptr);
            require(delivered == 1, "exception permanently consumed the kill");
        } else if (test == "projectile_context") {
            localPlayer = &player;
            HhSteal(&enemy, &projectile, &projectile);
            require(delivered == 1 && lastPlayer == &player, "projectile accepted as the buff player");
        } else if (test == "object_player_reference") {
            localPlayer = &player; localPlayerAsObject = true;
            HhSteal(&enemy, nullptr, nullptr);
            require(delivered == 1, "local player object coerced to a numeric instance id");
        } else if (test == "typed_instance_id_killer") {
            returnedIdKind = VALUE_REF;
            RValue result, killer(static_cast<double>(player.id));
            killer.m_Kind = VALUE_REF;
            RValue* arguments[] = {nullptr,nullptr,&killer};
            Hook_EnemyDestroyKillProc(&enemy,nullptr,result,3,arguments);
            require(delivered == 1 && lastPlayer == &player && originalCalls == 1,
                "runner's typed instance id rejected before buff delivery");
        } else if (test == "typed_instance_id_local_fallback") {
            returnedIdKind = VALUE_REF;
            localPlayer = &player; localPlayerAsObject = true;
            RValue result;
            Hook_HhDeathEffects(&enemy,nullptr,result,0,nullptr);
            require(delivered == 1 && lastPlayer == &player && originalCalls == 1,
                "typed id from local-player lookup silenced all death fallbacks");
        } else if (test == "native_lookup_with_broken_sdk_room") {
            sdkLookupWorks = false; returnedIdKind = VALUE_REF;
            localPlayer = &player;
            RValue result;
            Hook_HhDeathEffects(&enemy,nullptr,result,0,nullptr);
            require(delivered == 1 && lastPlayer == &player && originalCalls == 1,
                "valid player was lost because YYTK's room-list layout is stale");
        } else if (test == "native_numeric_lookup_with_broken_sdk_room") {
            sdkLookupWorks = false;
            RValue result, killer(static_cast<double>(player.id));
            RValue* arguments[] = {nullptr,nullptr,&killer};
            Hook_EnemyDestroyKillProc(&enemy,nullptr,result,3,arguments);
            require(delivered == 1 && lastPlayer == &player && originalCalls == 1,
                "numeric killer still used YYTK's broken room traversal");
        } else if (test == "native_lookup_unavailable") {
            nativeLookupWorks = false; localPlayer = &player;
            HhSteal(&enemy,nullptr,nullptr);
            require(delivered == 0 && g_HhHandledIds.empty(),
                "missing native resolver consumed or delivered a kill");
        } else if (test == "native_lookup_wrong_identity") {
            nativeLookupOverride = &childPlayer;
            require(HhResolveInstance(RValue(static_cast<double>(player.id))) == nullptr,
                "native resolver returned a different instance id without rejection");
        } else if (test == "deduplicate_success") {
            HhSteal(&enemy, &player, nullptr);
            HhSteal(&enemy, nullptr, &player);
            require(delivered == 1 && attempts == 1, "same kill delivered twice");
        } else if (test == "reject_non_enemy") {
            HhSteal(&projectile, &player, nullptr);
            require(delivered == 0, "non-monster event granted a buff");
        } else if (test == "disabled") {
            g_HhEnabled = false; HhSteal(&enemy, &player, nullptr);
            require(delivered == 0 && g_HhHandledIds.empty(), "disabled mechanic consumed an event");
        } else if (test == "player_subclass") {
            HhSteal(&enemy, &childPlayer, nullptr);
            require(delivered == 1 && lastPlayer == &childPlayer, "valid player subclass rejected");
        } else if (test == "bounded_cache") {
            for (int i=0; i<1000; ++i) { enemy.id=1000+i; HhSteal(&enemy,&player,nullptr); }
            require(delivered == 1000 && g_HhHandledIds.size() <= 256 && g_HhHandledOrder.size() <= 256, "unbounded kill cache");
        } else if (test == "capture_before_cleanup") {
            originalRemovesEnemy = true;
            RValue result, killer(&player); RValue* arguments[] = {nullptr,nullptr,&killer};
            Hook_EnemyDestroyKillProc(&enemy,nullptr,result,3,arguments);
            require(delivered == 1 && originalCalls == 1 && !enemy.alive, "enemy was read after original cleanup or original was skipped");
        } else if (test == "player_self_call_shape") {
            RValue result, victim(&enemy); RValue* arguments[] = {nullptr,nullptr,&victim};
            Hook_EnemyDestroyKillProc(&player,nullptr,result,3,arguments);
            require(delivered == 1 && lastPlayer == &player && originalCalls == 1, "player-self kill dispatch lost the victim");
        } else if (test == "kill_without_arguments") {
            localPlayer = &player;
            RValue result;
            Hook_EnemyDestroyKillProc(&enemy,nullptr,result,0,nullptr);
            require(delivered == 1 && originalCalls == 1, "missing arguments silenced the kill hook");
        } else if (test == "death_without_visual_effect") {
            localPlayer = &player;
            RValue result;
            Hook_HhDeathEffects(&enemy,nullptr,result,0,nullptr);
            require(delivered == 1 && originalCalls == 1, "death script required the visual object or primary kill hook");
        } else if (test == "both_death_paths") {
            localPlayer = &player;
            RValue result;
            Hook_HhDeathEffects(&enemy,nullptr,result,0,nullptr);
            Hook_EnemyDestroyKillProc(&enemy,nullptr,result,0,nullptr);
            require(delivered == 1 && originalCalls == 2, "death script and kill proc applied duplicate buffs");
        } else if (test == "automatic_combat_log") {
            HeadhunterActivityTick();
            require(statusReports == 0, "startup noise logged as combat");
            g_HhHookCalls = 1; HeadhunterActivityTick();
            require(statusReports == 1, "combat required manual settings reapply to appear in log");
            g_HhHookCalls = 2; nowMs = 1000; HeadhunterActivityTick();
            require(statusReports == 1, "combat log rate limit failed");
            nowMs = 30000; HeadhunterActivityTick();
            require(statusReports == 2, "new combat summary never flushed");
            nowMs = 60000; HeadhunterActivityTick();
            require(statusReports == 2, "unchanged counters repeatedly logged");
        } else if (test == "disabled_combat_log") {
            g_HhEnabled = false; g_HhHookCalls = 4; HeadhunterActivityTick();
            require(statusReports == 0, "disabled mechanic emitted combat summaries");
        // ---- kill drops: baseline (holds before and after the spawn moved) ----
        } else if (test == "drops_off_no_spawn") {
            enemy.enemyRarity = 4; g_AngelicPool.push_back({ 3, 1, 15, "test unique", true });
            killEnemy(enemy, player);
            require(angelicSpawns.empty() && sigSpawns.empty(), "a drop that is off still spawned");
            require(g_AngelicDropRolls == 0 && g_SigDropRolls == 0, "a drop that is off still rolled");
            require(originalCalls == 1, "original kill proc not called exactly once");
        } else if (test == "drop_skips_non_monster") {
            dropsCertain(); enemy.enemyRarity = 0;
            killEnemy(enemy, player);
            RValue result, victim(&enemy); RValue* arguments[] = {nullptr,nullptr,&victim};
            Hook_EnemyDestroyKillProc(&player,nullptr,result,3,arguments);   // player-self shape: still no drop
            require(angelicSpawns.empty() && sigSpawns.empty(), "a non-monster (enemyRarity < 1) got a drop");
            require(g_AngelicDropRolls == 0 && g_SigDropRolls == 0, "a non-monster was rolled");
            require(originalCalls == 2, "original kill proc not called exactly once per kill");
        } else if (test == "drop_hit_at_enemy_position") {
            dropsCertain(); enemy.enemyRarity = 1; enemy.x = 320.5; enemy.y = 144.25;
            killEnemy(enemy, player);
            require(angelicSpawns.size() == 1 && sigSpawns.size() == 1, "a certain hit did not spawn both drops");
            require(angelicSpawns[0].x == 320.5 && angelicSpawns[0].y == 144.25, "angelic drop not at the enemy's x,y");
            require(sigSpawns[0].x == 320.5 && sigSpawns[0].y == 144.25, "signature drop not at the enemy's x,y");
            require(angelicSpawns[0].self == &enemy && sigSpawns[0].self == &enemy, "drop spawned with a self other than the enemy");
            require(g_AngelicDropHits == 1 && g_AngelicDropRolls == 1 && g_SigDropRolls == 1, "drop counters wrong");
            require(originalCalls == 1, "original kill proc not called exactly once");
        // ---- kill drops: target (spawn while the enemy is still live, before the original) ----
        } else if (test == "angelic_spawns_before_cleanup") {
            originalRemovesEnemy = true; g_AngelicDropOneIn = 1.0; enemy.enemyRarity = 1;
            killEnemy(enemy, player);
            requireSpawnedLive(angelicSpawns, &enemy, "angelic");
            require(originalCalls == 1 && !enemy.alive, "original kill proc not called exactly once");
        } else if (test == "sigdrop_spawns_before_cleanup") {
            originalRemovesEnemy = true; g_SigDropPct = 100.0; g_SigDropAncientPct = 100.0; g_SigDropForce = 0; enemy.enemyRarity = 4;
            killEnemy(enemy, player);
            requireSpawnedLive(sigSpawns, &enemy, "sigdrop");
            require(originalCalls == 1 && !enemy.alive, "original kill proc not called exactly once");
        } else if (test == "drops_read_nothing_after_original") {
            originalRemovesEnemy = true; dropsCertain(); enemy.enemyRarity = 2;
            killEnemy(enemy, player);
            require(readsAfterOriginal == 0, "a kill drop read the enemy after the original kill proc had run");
            require(angelicSpawns.size() == 1 && sigSpawns.size() == 1 && originalCalls == 1, "drops or original missing");
        } else if (test == "drop_throw_still_calls_original") {
            g_AngelicDropOneIn = 1.0; enemy.enemyRarity = 1; spawnThrows = true; outThrows = true;
            bool escaped = false;
            try { killEnemy(enemy, player); } catch (...) { escaped = true; }
            require(!escaped, "an exception from a kill drop escaped the kill hook");
            require(originalCalls == 1, "a throwing kill drop skipped or repeated the original kill proc");
#ifdef HAS_APPENDSIGNATURECANDIDATES
        } else if (test == "signature_pool_append") {
            std::vector<AngelicCandidate> empty;
            AppendSignatureCandidates(empty);
            require(empty.empty(), "signature candidates were appended to an empty pool");
            std::vector<AngelicCandidate> pool{ {3,1,15,"test unique",true} };
            AppendSignatureCandidates(pool);
            require(pool.size() == 3, "signature candidates were not appended to a non-empty pool");
            int crowns = 0, belts = 0;
            for (const auto& c : pool) { if (c.signature == 0) ++crowns; else if (c.signature == 1) ++belts; }
            require(crowns == 1 && belts == 1, "expected exactly one crown and one belt signature entry");
#endif
        } else if (test == "angelic_pick_crown_spawns_signature") {
            g_AngelicDropOneIn = 1.0; enemy.enemyRarity = 1;
            g_AngelicPool.push_back({ 0, 0, 0, "Tyrant's Crown", true, 0 });
            killEnemy(enemy, player);
            require(sigSpawns.size() == 1 && sigSpawns[0].which == 0, "picking the crown signature entry did not spawn Tyrant's Crown");
            require(angelicSpawns.empty(), "a signature-only pool also spawned through SpawnAngelicItem");
            require(sigSpawns[0].self == &enemy && sigSpawns[0].originalCallsBefore == 0 && sigSpawns[0].selfAlive,
                "signature pick spawned after the original, or with the wrong self, or a cleaned-up enemy");
            require(g_AngelicDropHits == 1, "the Angelic hit counter was not credited for a signature pick");
            require(originalCalls == 1, "original kill proc not called exactly once");
        } else if (test == "angelic_pick_belt_spawns_signature") {
            g_AngelicDropOneIn = 1.0; enemy.enemyRarity = 1;
            g_AngelicPool.push_back({ 0, 0, 0, "Headhunter", true, 1 });
            killEnemy(enemy, player);
            require(sigSpawns.size() == 1 && sigSpawns[0].which == 1, "picking the belt signature entry did not spawn Headhunter");
            require(angelicSpawns.empty(), "a signature-only pool also spawned through SpawnAngelicItem");
            require(sigSpawns[0].self == &enemy && sigSpawns[0].originalCallsBefore == 0 && sigSpawns[0].selfAlive,
                "signature pick spawned after the original, or with the wrong self, or a cleaned-up enemy");
            require(g_AngelicDropHits == 1, "the Angelic hit counter was not credited for a signature pick");
            require(originalCalls == 1, "original kill proc not called exactly once");
        } else if (test == "signature_equal_share") {
            g_AngelicDropOneIn = 1.0; enemy.enemyRarity = 1;
            g_AngelicPool.push_back({ 3, 1, 15, "test unique", true });
            g_AngelicPool.push_back({ 0, 0, 0, "Tyrant's Crown", true, 0 });
            g_AngelicPool.push_back({ 0, 0, 0, "Headhunter", true, 1 });
            for (int i = 0; i < 3000; ++i) killEnemy(enemy, player);
            int crowns = 0, belts = 0;
            for (const auto& s : sigSpawns) { if (s.which == 0) ++crowns; else if (s.which == 1) ++belts; }
            require(angelicSpawns.size() >= 800 && angelicSpawns.size() <= 1200, "the ordinary unique did not get its equal share of the pool");
            require(crowns >= 800 && crowns <= 1200, "Tyrant's Crown did not get its equal share of the pool");
            require(belts >= 800 && belts <= 1200, "Headhunter did not get its equal share of the pool");
            require(angelicSpawns.size() + (size_t)crowns + (size_t)belts == 3000, "some kills spawned nothing, or spawned more than once");
        } else if (test == "sigdrop_force_belt") {
            enemy.enemyRarity = 1; g_SigDropForce = 1;
            SignatureDropOnKill(&enemy);
            require(sigSpawns.size() == 1 && sigSpawns[0].which == 1, "sigdrop belt did not force a Headhunter spawn on the kill");
        } else if (test == "sigdrop_force_no_alternation") {
            enemy.enemyRarity = 1; g_SigDropForce = 0;
            SignatureDropOnKill(&enemy);
            SignatureDropOnKill(&enemy);
            require(sigSpawns.size() == 2 && sigSpawns[0].which == 0 && sigSpawns[1].which == 0,
                "sigdrop crown alternated to the belt instead of forcing the same item on every kill");
#ifdef HAS_ENABLE_HEADHUNTER
        } else if (test == "standalone_fallback_install") {
            EnableHeadhunter();
            require(g_HhEnabled && createInstallCalls == 1 && g_OrigICD && g_OrigICL, "Headhunter relied on another feature to install the fallback");
        } else if (test == "fallback_without_primary") {
            primaryAvailable = false;
            EnableHeadhunter();
            require(g_HhEnabled && !g_HhHookInstalled && g_OrigICD, "usable fallback disabled when primary hook failed");
        } else if (test == "no_trigger_available") {
            primaryAvailable = false; fallbackAvailable = false; deathScriptAvailable = false; g_Orig_HhDeathEffects = nullptr;
            EnableHeadhunter();
            require(!g_HhEnabled, "mechanic advertised as enabled without any trigger");
        } else if (test == "death_script_only") {
            primaryAvailable = false; fallbackAvailable = false; g_Orig_HhDeathEffects = nullptr;
            EnableHeadhunter();
            require(g_HhEnabled && g_Orig_HhDeathEffects, "death script could not act as the sole available trigger");
#endif
        } else return 2;
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << test << ": " << error.what() << '\n';
        return 1;
    }
}
