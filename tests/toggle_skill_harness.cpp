// Behavioral harness for the toggle-skill active indicator's read (issue
// #11, Track B).
//
// The Python runner injects the REAL ToggleSkillMod.hpp header, the REAL
// ToggleIndicatorResolveAoeObject/ToggleIndicatorRead below and the real
// scan-cap constant, so no number or branch here restates one from the
// plugin. Only the game API is replaced.
//
// P1b (replan 1): ownership by the AOE's own `isMyClient`, not by comparing
// against the local player - session 3 measured that `Player_obj` has no
// `playerNumber` (docs/toggle-skills-research.md, "Co-op / ownership after
// session 3: isMyClient"). This pins the decision settled on BEFORE any
// drawing code exists: zero instances answers Off without reading any
// instance's own fields; a resolved AOE whose own isMyClient cannot be read
// is unattributed and never lights the indicator; the object itself failing
// to resolve is a different, stronger failure (Unreadable) than "resolved
// but empty" (Off); and the marker-required decision (the Purgatory field,
// "Plain-cast flash (R10) and the Purgatory marker") is a separate pass over
// the same evidence, not baked into the plain read.
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING,
       VALUE_UNDEFINED, VALUE_BOOL, VALUE_ARRAY };
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    bool boolean = false;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return boolean; }
    std::string ToString() const { return text; }
};
static RValue MakeBool(bool b) { RValue r; r.m_Kind = VALUE_BOOL; r.boolean = b; return r; }
static RValue MakeReal(double n) { RValue r; r.m_Kind = VALUE_REAL; r.number = n; return r; }

// A stand-in for HeroSiege::Objects - the harness needs the AOE object named
// by the SDK constant to exist and resolve to a string, not the whole SDK.
namespace HeroSiege { namespace Objects {
enum class GameObject { White_Mage_Soul_Spurn_AOE_obj, UI_Hud_Talent_obj };
inline const char* GetObjectName(GameObject) { return "White_Mage_Soul_Spurn_AOE_obj"; }
}}

// ---- the controlled world -------------------------------------------------
// Every field is read independently, tagged by kind - the same shape
// variable_instance_get actually hands back, never by a game-side identity
// this harness happens to know. isMyClient is read for every scanned
// instance; purgatory is read only for an instance already classified own
// (mineExpected exists only to let a scenario assert the split without
// duplicating the production classification here).
struct AoeInst {
    RValue isMyClient = MakeBool(true);
    bool isMyClientThrows = false;
    RValue purgatory;   // default VALUE_UNDEFINED: marker unreadable
    bool purgatoryThrows = false;
};
static AoeInst OwnMarked(double purgatoryValue = 0.09) {
    AoeInst a; a.isMyClient = MakeBool(true); a.purgatory = MakeReal(purgatoryValue); return a;
}
static AoeInst OwnUnmarked() {
    AoeInst a; a.isMyClient = MakeBool(true); a.purgatory = MakeReal(0.0); return a;
}
static AoeInst OwnMarkerUnreadable() {
    AoeInst a; a.isMyClient = MakeBool(true); a.purgatory = RValue(); return a;   // undefined
}
static AoeInst OwnByNumber(double n = 1.0) {
    AoeInst a; a.isMyClient = MakeReal(n); a.purgatory = MakeReal(0.0); return a;
}
static AoeInst Foreign(double purgatoryValue = 0.0) {
    AoeInst a; a.isMyClient = MakeBool(false); a.purgatory = MakeReal(purgatoryValue); return a;
}
static AoeInst Unattributed() {
    AoeInst a; a.isMyClient = RValue(); return a;   // undefined
}

struct World {
    bool aoeObjectResolves = true;
    std::vector<AoeInst> instances;
    bool instanceNumberThrows = false;   // instance_number itself throws
};
static World world;
static long g_ResolveCalls = 0;   // HhResolveLocalPlayer calls - must stay 0 (read/no_player_lookup)

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string fn = name;
        if (fn == "asset_get_index") {
            return RValue(world.aoeObjectResolves ? 42.0 : -1.0);
        }
        if (fn == "instance_number") {
            if (world.instanceNumberThrows) throw std::runtime_error("instance_number EXCEPTION");
            return RValue((double)world.instances.size());
        }
        if (fn == "instance_find") {
            const int i = (int)args[1].ToDouble();
            if (i < 0 || (size_t)i >= world.instances.size()) return RValue();   // VALUE_UNDEFINED
            RValue r; r.m_Kind = VALUE_REF; r.text = "aoe:" + std::to_string(i);
            return r;
        }
        if (fn == "variable_instance_get") {
            const std::string tag = args[0].text;
            const std::string field = args[1].ToString();
            if (tag.rfind("aoe:", 0) != 0) return RValue();
            const size_t i = (size_t)std::stoi(tag.substr(4));
            if (i >= world.instances.size()) return RValue();
            const AoeInst& a = world.instances[i];
            if (field == "isMyClient") {
                if (a.isMyClientThrows) throw std::runtime_error("isMyClient EXCEPTION");
                return a.isMyClient;
            }
            if (field == "purgatory") {
                if (a.purgatoryThrows) throw std::runtime_error("purgatory EXCEPTION");
                return a.purgatory;
            }
            return RValue();
        }
        return RValue();
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

// Present only so the read/no_player_lookup scenario has something to count
// - the production read must never call this (Context "Co-op / ownership
// after session 3: isMyClient": "No local-player read at all").
static bool HhResolveLocalPlayer(RValue& out) {
    ++g_ResolveCalls;
    RValue r; r.m_Kind = VALUE_REF; r.text = "player";
    out = r;
    return true;
}

// ---- what the injected production code leans on ---------------------------
// PRODUCTION_CONSTANTS

// PRODUCTION_TOGGLESKILL

// PRODUCTION_FUNCTIONS

// ---- scenarios ------------------------------------------------------------
static int failures = 0;
static void checkState(const std::string& label, ForgePact::ToggleIndicatorState got, ForgePact::ToggleIndicatorState want) {
    const bool ok = got == want;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label
               << " got=" << ForgePact::ToggleIndicatorStateName(got)
               << " want=" << ForgePact::ToggleIndicatorStateName(want) << "\n";
}
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = got == want;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkBool(const std::string& label, bool got, bool want) {
    const bool ok = got == want;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}

static void resetWorld() {
    world = World{};
}

int main() {
    // 1. Zero instances answers Off without reading any instance's own
    //    fields - a real, cheap negative.
    resetWorld();
    world.instances = {};
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/no_aoe_is_off", state, ForgePact::ToggleIndicatorState::Off);
    }

    // 2. The AOE object itself does not resolve by name at all - a
    //    different, stronger failure than "resolved but zero instances".
    resetWorld();
    world.aoeObjectResolves = false;
    world.instances = { OwnMarked() };   // must not matter - object never resolved
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/object_unresolved_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
    }

    // 3. `instance_number` itself throws: a failed read, not a measured
    //    zero. The catch's `d.n = 0` fallback must not be mistaken for a
    //    real, cheap Off - it must count as a failure instead.
    resetWorld();
    world.instances = { OwnMarked() };   // must not matter - the count never completes
    world.instanceNumberThrows = true;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/instance_number_throw_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
        checkBool("read/instance_number_throw_is_unreadable/countReadFailed", d.countReadFailed, true);
    }

    // 4. Own AOE, isMyClient a VALUE_BOOL true: On.
    resetWorld();
    world.instances = { OwnMarked() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/own_bool_true_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/own_bool_true_is_on/mine", d.mine, 1);
    }

    // 5. Own AOE, isMyClient a nonzero real (not a bool): still On - a
    //    numeric kind counts nonzero as true.
    resetWorld();
    world.instances = { OwnByNumber(1.0) };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/own_real_one_is_on", state, ForgePact::ToggleIndicatorState::On);
    }

    // 6. A foreign instance (isMyClient a VALUE_BOOL false) alone: Off, not
    //    On and not Unreadable.
    resetWorld();
    world.instances = { Foreign() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/foreign_bool_false_is_off", state, ForgePact::ToggleIndicatorState::Off);
        checkInt("read/foreign_bool_false_is_off/others", d.others, 1);
    }

    // 7. Own AOE plus a foreign one: still On - a foreign AOE never masks ours.
    resetWorld();
    world.instances = { OwnMarked(), Foreign() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/own_and_foreign_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/own_and_foreign_is_on/mine", d.mine, 1);
        checkInt("read/own_and_foreign_is_on/others", d.others, 1);
    }

    // 8. Two own instances (e.g. a double-cast proc, Track A Q5): still On.
    resetWorld();
    world.instances = { OwnMarked(), OwnMarked() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/two_own_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/two_own_is_on/mine", d.mine, 2);
    }

    // 9. Every AOE present is unattributed (its own isMyClient unreadable):
    //    Unreadable, not Off - a foreign AOE fails toward absent, but an
    //    unreadable one must not be guessed either way.
    resetWorld();
    world.instances = { Unattributed(), Unattributed() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/unattributed_only_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
        checkInt("read/unattributed_only_is_unreadable/unattributed", d.unattributed, 2);
    }

    // 10. The scan is capped: more instances exist than the budget scans,
    //     and the read still decides correctly on what it actually visited.
    resetWorld();
    for (int i = 0; i < kToggleIndicatorScanCap + 6; ++i) world.instances.push_back(OwnMarked());
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);
        checkState("read/scan_is_capped", state, ForgePact::ToggleIndicatorState::On);
        checkBool("read/scan_is_capped/capped", d.capped, true);
        checkInt("read/scan_is_capped/n", d.n, kToggleIndicatorScanCap + 6);
        checkInt("read/scan_is_capped/mine", d.mine, kToggleIndicatorScanCap);
    }

    // 11. No caching across calls: a call reflects the CURRENT world, not a
    //     stale snapshot from an earlier call in the same session.
    resetWorld();
    world.instances = {};
    {
        ForgePact::ToggleIndicatorReadDetail d1;
        auto state1 = ToggleIndicatorRead(&d1, false);
        checkState("read/reread_every_call/first_off", state1, ForgePact::ToggleIndicatorState::Off);
        world.instances = { OwnMarked() };
        ForgePact::ToggleIndicatorReadDetail d2;
        auto state2 = ToggleIndicatorRead(&d2, false);
        checkState("read/reread_every_call", state2, ForgePact::ToggleIndicatorState::On);
    }

    // 12. `spurn as foreign`: treating every own instance as foreign - a
    //     non-mutating negative control, so the real world is never touched.
    resetWorld();
    world.instances = { OwnMarked() };   // the real answer would be On
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, /*treatOwnAsForeign=*/true);
        checkState("read/as_foreign_excludes_own", state, ForgePact::ToggleIndicatorState::Off);
        checkInt("read/as_foreign_excludes_own/others", d.others, 1);
        checkInt("read/as_foreign_excludes_own/mine", d.mine, 0);
    }

    // ---- marker (Purgatory) scenarios: the marker-required decision is a
    // separate pass (ToggleIndicatorModel::Decide(d, true)) over the SAME
    // evidence one ToggleIndicatorRead call gathers -------------------------

    // 14. An own instance whose own purgatory reads numeric > 0: On when the
    //     marker is required.
    resetWorld();
    world.instances = { OwnMarked(0.09) };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        ToggleIndicatorRead(&d, false);
        checkInt("marker/marked_own_on_when_required/markedMine", d.markedMine, 1);
        auto state = ForgePact::ToggleIndicatorModel::Decide(d, /*requireMarker=*/true);
        checkState("marker/marked_own_on_when_required", state, ForgePact::ToggleIndicatorState::On);
    }

    // 15. An own instance whose own purgatory reads numeric <= 0 (readable,
    //     just not positive): Off when the marker is required.
    resetWorld();
    world.instances = { OwnUnmarked() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        ToggleIndicatorRead(&d, false);
        checkInt("marker/unmarked_own_off_when_required/unmarkedMine", d.unmarkedMine, 1);
        auto state = ForgePact::ToggleIndicatorModel::Decide(d, /*requireMarker=*/true);
        checkState("marker/unmarked_own_off_when_required", state, ForgePact::ToggleIndicatorState::Off);
    }

    // 16. The SAME unmarked own instance, marker NOT required: On - the
    //     plain ownership read does not consult purgatory at all.
    resetWorld();
    world.instances = { OwnUnmarked() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, false);   // requireMarker defaults to false in Decide()
        checkState("marker/unmarked_own_on_when_not_required", state, ForgePact::ToggleIndicatorState::On);
    }

    // 17. An own instance whose own purgatory could not be read, and none
    //     marked: Unreadable when the marker is required - never guessed as
    //     Off.
    resetWorld();
    world.instances = { OwnMarkerUnreadable() };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        ToggleIndicatorRead(&d, false);
        checkInt("marker/unreadable_marker_unreadable_when_required/markUnreadableMine", d.markUnreadableMine, 1);
        auto state = ForgePact::ToggleIndicatorModel::Decide(d, /*requireMarker=*/true);
        checkState("marker/unreadable_marker_unreadable_when_required", state, ForgePact::ToggleIndicatorState::Unreadable);
    }

    // 18. A foreign instance's own purgatory is never read for the marker
    //     split - a marked foreign instance must not light the indicator.
    resetWorld();
    world.instances = { Foreign(/*purgatoryValue=*/0.09) };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        ToggleIndicatorRead(&d, false);
        checkInt("marker/foreign_marker_ignored/markedMine", d.markedMine, 0);
        auto state = ForgePact::ToggleIndicatorModel::Decide(d, /*requireMarker=*/true);
        checkState("marker/foreign_marker_ignored", state, ForgePact::ToggleIndicatorState::Off);
    }

    // The read never makes a player-resolving call, in any scenario above -
    // counted here, at the end, so it covers every one of them.
    checkInt("read/no_player_lookup", g_ResolveCalls, 0);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
