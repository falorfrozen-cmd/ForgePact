// Behavioral harness for the Special Content eSt gate.
//
// The Python runner injects the REAL GlobalArray, the REAL EstForceApply and
// the REAL room-gated caller below. Only the game API is replaced; no game
// process, character, installed DLL or release asset is touched.
//
// Why a behavioural harness and not a source assertion: the obvious fix for
// "EstForceApply runs 60 times a second" is a flat frame throttle, and it is
// the wrong fix. docs/S10-special-content-notes.md establishes that the game
// refills the whole eSt array at Room Start and that the special-content
// mechanic bodies read global.eSt directly, at step time, during that same
// room-load sequence. A throttled correction would put up to ~250 ms of
// vanilla gate value exactly where the mechanics evaluate - Special Content
// silently producing less, intermittently. So the scenarios here assert the
// timing of the correction, not just how few calls it costs.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING,
       VALUE_UNDEFINED, VALUE_BOOL, VALUE_ARRAY };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
struct CInstance { int id = 0; };
using AurieStatus = int;
static bool AurieSuccess(int s) { return s == 0; }

// ---- the controlled world -------------------------------------------------
// The eleven entries the game assigns at Room Start, measured identical in
// every room (docs/S10-special-content-notes.md). eSt[0] = 35 > 0 is the
// closed gate: no special-content mechanic produces anything.
static const std::vector<double> kVanillaEst = { 35, 50, 3, 7, 12, 20, 4, 9, 15, 2, 6 };

struct World {
    bool estExists = true;
    std::vector<double> est = kVanillaEst;
    int64_t room = 100;
    bool roomReadable = true;
    // Every runtime call the eSt path can make, counted by name.
    long globalExists = 0, globalGet = 0, arrayLength = 0, arrayGet = 0, arraySet = 0;
    void resetCounts() { globalExists = arrayGet = arraySet = globalGet = arrayLength = 0; }
    // Simulated Room Start: the game reassigns all eleven entries.
    void roomStart(int64_t newRoom) { est = kVanillaEst; room = newRoom; }
};
static World world;
static CInstance g_GlobalInstance;
static RValue g_RoomMember;

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string key(name);
        if (key == "variable_global_exists") {
            ++world.globalExists;
            return RValue(world.estExists && args[0].ToString() == "eSt" ? 1.0 : 0.0);
        }
        if (key == "variable_global_get") {
            ++world.globalGet;
            if (!world.estExists || args[0].ToString() != "eSt") return RValue();
            RValue arr; arr.m_Kind = VALUE_ARRAY; return arr;
        }
        if (key == "array_length") { ++world.arrayLength; return RValue((double)world.est.size()); }
        if (key == "array_get") {
            ++world.arrayGet;
            const int i = (int)args[1].ToDouble();
            return RValue(i >= 0 && i < (int)world.est.size() ? world.est[i] : 0.0);
        }
        if (key == "array_set") {
            ++world.arraySet;
            const int i = (int)args[1].ToDouble();
            if (i >= 0 && i < (int)world.est.size()) world.est[i] = args[2].ToDouble();
            return RValue();
        }
        return RValue();
    }
    AurieStatus GetGlobalInstance(CInstance** out) { *out = &g_GlobalInstance; return 0; }
    AurieStatus GetInstanceMember(RValue, const char* name, RValue*& out) {
        if (std::string(name) == "room" && world.roomReadable) {
            g_RoomMember = RValue((double)world.room);
            out = &g_RoomMember;
            return 0;
        }
        out = nullptr;
        return 1;
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

// ---- what the injected production code leans on ---------------------------
// Test-controlled input, not production text: which entries are forced, and
// how many corrections have been written.
static std::map<int, double> g_EstForce;
static uint64_t g_EstForceWrites = 0;

// PRODUCTION_EST

// ---- scenarios ------------------------------------------------------------
static int failures = 0;
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = (got == want);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkAtMost(const std::string& label, long long got, long long limit) {
    const bool ok = (got <= limit);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " limit=" << limit << "\n";
}

// The pre-change shape, kept runnable so the before/after numbers are measured
// against each other rather than quoted from a comment.
static void PreChangeTick(uint32_t) { EstForceApply(); }

static void resetAll(int64_t room) {
    world = World{};
    world.room = room;
    world.resetCounts();
    g_EstForceWrites = 0;
    g_EstLastRoom = INT64_MIN;
}

int main() {
    // --- 1. baseline: nothing forced costs nothing --------------------------
    resetAll(100);
    g_EstForce.clear();
    for (uint32_t f = 1; f <= 60; ++f) EstForceTick(f);
    checkInt("all_off/variable_global_get", world.globalGet, 0);
    checkInt("all_off/array_get", world.arrayGet, 0);
    checkInt("all_off/array_set", world.arraySet, 0);

    // --- 2. baseline: the gate is opened, and stays open --------------------
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };          // the shared gate: eSt[0] <= 0 opens it
    EstForceTick(1);
    checkInt("opened/est0", (long long)world.est[0], -1);
    checkInt("opened/writes", (long long)g_EstForceWrites, 1);

    // --- 3. baseline: a Room Start overwrite is corrected on the SAME frame --
    // This is the whole point of the design. The mechanic bodies read
    // global.eSt at step time during the room-load sequence, so a correction
    // one tick later is a correction after the read that mattered.
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };
    EstForceTick(1);                        // settle in room 100
    g_EstForceWrites = 0;
    world.roomStart(200);                   // the game refills eSt and the room changes
    EstForceTick(2);                        // frame 2 is NOT a multiple of the poll period
    checkInt("room_start/corrected_same_frame", (long long)g_EstForceWrites, 1);
    checkInt("room_start/est0", (long long)world.est[0], -1);

    // --- 4. baseline: an overwrite with NO room change is still corrected ----
    // Nobody has proven Room Start is the only time the game writes eSt, so
    // the safety re-poll stays. An unproven negative is not a licence.
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };
    EstForceTick(1);
    g_EstForceWrites = 0;
    world.est = kVanillaEst;                // overwritten by something unknown
    long long correctedAt = -1;
    for (uint32_t f = 2; f <= 1 + 15 * 2; ++f) {
        EstForceTick(f);
        if (g_EstForceWrites && correctedAt < 0) correctedAt = f;
    }
    checkInt("no_room_change/corrected", (long long)g_EstForceWrites, 1);
    checkAtMost("no_room_change/frames_to_correct", correctedAt - 1, 15);

    // --- 5. target: 60 idle frames, before vs after -------------------------
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };
    EstForceTick(1);                        // the gate is already correct after this
    world.resetCounts();
    for (uint32_t f = 2; f <= 61; ++f) PreChangeTick(f);
    const long before = world.arrayGet;
    world.resetCounts();
    for (uint32_t f = 2; f <= 61; ++f) EstForceTick(f);
    const long after = world.arrayGet;
    checkInt("idle60/array_get_before", before, 60);
    checkAtMost("idle60/array_get_after", after, 4);
    checkInt("idle60/no_redundant_writes", (long long)world.arraySet, 0);

    // --- 6. target: an unreadable room key must not SUPPRESS the correction --
    // A sentinel meaning "unknown" must never decide that nothing needs doing.
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };
    EstForceTick(1);
    g_EstForceWrites = 0;
    world.roomReadable = false;             // room member unreadable, as during a load
    world.est = kVanillaEst;
    for (uint32_t f = 2; f <= 1 + 15; ++f) EstForceTick(f);
    checkInt("unreadable_room/still_corrected", (long long)g_EstForceWrites, 1);
    checkInt("unreadable_room/est0", (long long)world.est[0], -1);

    // --- 7. target: several forced entries, all corrected on a room change ---
    resetAll(100);
    g_EstForce = { { 0, -1.0 }, { 2, 99.0 } };
    EstForceTick(1);
    g_EstForceWrites = 0;
    world.roomStart(300);
    EstForceTick(2);
    checkInt("multi/writes", (long long)g_EstForceWrites, 2);
    checkInt("multi/est0", (long long)world.est[0], -1);
    checkInt("multi/est2", (long long)world.est[2], 99);

    // --- 8. target: a missing global is survived, not crashed on ------------
    resetAll(100);
    g_EstForce = { { 0, -1.0 } };
    world.estExists = false;
    for (uint32_t f = 1; f <= 30; ++f) EstForceTick(f);
    checkInt("no_global/writes", (long long)g_EstForceWrites, 0);
    checkInt("no_global/array_get", world.arrayGet, 0);

    // --- 9. target: changing WHAT is forced re-opens the gate next frame -----
    // The room gate remembers the room the last apply ran in. Turning Special
    // Content off and on again WITHOUT leaving that room therefore looked like
    // no change at all, and the correction waited for the next kEstPollFrames
    // re-poll - ~250 ms of vanilla gate immediately after a user action, in a
    // feature whose consumers read eSt at step time. The pre-change per-frame
    // write had no such gap. Dropping the remembered key inside the mutators
    // makes the following frame look like a room change, which restores it.
    resetAll(100);
    EstForceSet(0, -1.0);
    EstForceTick(1);                        // settles in room 100
    EstForceClear();                        // the user turns Special Content off
    EstForceTick(2);                        // all-off: returns without a read
    EstForceSet(0, -1.0);                   // ...and back on, same room
    world.est = kVanillaEst;
    g_EstForceWrites = 0;
    EstForceTick(3);                        // frame 3 is not a multiple of the period
    checkInt("toggle_off_on/corrected_next_frame", (long long)g_EstForceWrites, 1);
    checkInt("toggle_off_on/est0", (long long)world.est[0], -1);

    // ...and the same when one slot is dropped while others stay forced.
    resetAll(100);
    EstForceSet(0, -1.0);
    EstForceSet(2, 99.0);
    EstForceTick(1);
    EstForceErase(2);                       // `estfree 2`, same room
    world.est = kVanillaEst;
    g_EstForceWrites = 0;
    EstForceTick(2);
    checkInt("toggle_erase/corrected_next_frame", (long long)g_EstForceWrites, 1);
    checkInt("toggle_erase/est0", (long long)world.est[0], -1);
    checkInt("toggle_erase/est2_left_vanilla", (long long)world.est[2], (long long)kVanillaEst[2]);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
