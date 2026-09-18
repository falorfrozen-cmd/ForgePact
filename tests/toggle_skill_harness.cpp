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
//
// T1 (issue #11, Track A): the re-cast guard's real HookTalentUseClass and
// ToggleGuardModel are injected too, compiled as the player build sees them
// (FORGEPACT_RELEASE defined: the research-only entry note and the
// lastProcRet record drop out). The caller's object_index, the double-cast
// object's asset_get_index and the trampoline are the stand-ins.
#define FORGEPACT_RELEASE 1
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
// The values match tests/cpp/stubs/YYToolkit/YYTK_Shared.hpp (VALUE_REF is
// YYToolkit's own 15), so a kind carrying flag bits above the low 28 is
// modelled the way the runner hands one over.
enum { VALUE_REAL = 0, VALUE_STRING = 1, VALUE_ARRAY = 2, VALUE_UNDEFINED = 5, VALUE_OBJECT = 6,
       VALUE_INT32 = 7, VALUE_INT64 = 8, VALUE_BOOL = 13, VALUE_REF = 15 };
static const unsigned kKindFlagBit = 0x80000000u;   // a high flag bit on top of a kind
// A script's `self`/`other`. Only what the guard can learn through a builtin
// is modelled: its object_index as variable_instance_get answers it, or a
// throw. The kind defaults to VALUE_REF because that is what this runner
// returns for object_index (ModuleMain.cpp's N1ObjectIndex and the
// Quest_Act_01_Brick_obj measurement, "kind=15 str=ref object ..."); a
// stand-in that only answered VALUE_REAL let a guard that rejected VALUE_REF
// pass every scenario while it would fail open on every live call.
struct CInstance {
    double objectIndex = -1;
    int objectIndexKind = VALUE_REF;
    bool objectIndexThrows = false;
};
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    bool boolean = false;
    std::string text;
    CInstance* inst = nullptr;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(CInstance* p) : m_Kind(VALUE_REF), inst(p) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return boolean; }
    std::string ToString() const { return text; }
};
static RValue MakeBool(bool b) { RValue r; r.m_Kind = VALUE_BOOL; r.boolean = b; return r; }
static RValue MakeReal(double n) { RValue r; r.m_Kind = VALUE_REAL; r.number = n; return r; }
using PFUNC_YYGMLScript = RValue& (*)(CInstance* Self, CInstance* Other, RValue& Result, int argc, RValue** Args);
// A stand-in for the Windows.h intrinsic the production counters use (same
// pattern as tests/orb_pickup_harness.cpp and tests/headhunter_dispatch_harness.cpp).
static long InterlockedIncrement(volatile long* target) { return ++(*target); }

// A stand-in for HeroSiege::Objects - the harness needs the AOE and the slot
// object named by the SDK constants to exist and resolve to a string, not
// the whole SDK.
namespace HeroSiege { namespace Objects {
enum class GameObject { White_Mage_Soul_Spurn_AOE_obj, UI_Hud_Talent_obj, Universal_Double_Cast_obj };
inline const char* GetObjectName(GameObject g) {
    if (g == GameObject::Universal_Double_Cast_obj) return "Universal_Double_Cast_obj";
    return g == GameObject::UI_Hud_Talent_obj ? "UI_Hud_Talent_obj" : "White_Mage_Soul_Spurn_AOE_obj";
}
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

// P2 (the shipped indicator): UI_Hud_Talent_obj instance 0's own `row0`
// array, the object ToggleIndicatorFindSlot walks (session 3's R5, "Slot
// geometry fields"). One element per hotbar slot; the element whose own
// talentId reads 240 (Soul Spurn) carries the rectangle.
struct HudRow0Elem {
    double talentId = 0, navBboxX = 0, navBboxY = 0, navBboxWidth = 0, navBboxHeight = 0;
};

struct World {
    bool aoeObjectResolves = true;
    std::vector<AoeInst> instances;
    bool instanceNumberThrows = false;   // instance_number itself throws
    // P2 slot lookup:
    bool hudTalentObjectResolves = true;
    bool hudTalentInstanceExists = true;
    bool row0IsArray = true;   // false -> variable_instance_get("row0") answers non-array (noRow0)
    std::vector<HudRow0Elem> row0 = { { 240.0, 100.0, 200.0, 50.0, 60.0 } };
    // Follow-up: a throwing draw_rectangle stub, to prove the catch after the
    // outline loop counts the exception rather than swallowing it uncounted.
    bool drawRectangleThrows = false;
    // T1 guard: whether the double-cast object's name resolves.
    bool doubleCastObjectResolves = true;
};
static World world;
static long g_DcResolveCalls = 0;      // asset_get_index("Universal_Double_Cast_obj") calls
static long g_InstanceEnumCalls = 0;   // instance_number/instance_find calls - guard_on/state_not_consulted
static long g_TrampCalls = 0;          // the TalentUseClass trampoline stand-in
static const double kDcObjIdx = 5318.0, kPlayerObjIdx = 7.0;   // what the stand-in runner answers
static long g_ResolveCalls = 0;    // HhResolveLocalPlayer calls - must stay 0 (read/no_player_lookup)
static long g_AnyCallCount = 0;    // every CallBuiltin call, of any name - indicator_off/no_runtime_calls
static int g_RectangleDraws = 0;   // draw_rectangle calls this draw
static double g_LastSetColour = -1, g_LastSetAlpha = -1;
static const double kAoeObjIdx = 42.0, kHudObjIdx = 99.0;
static const double kPrevColour = 555.0, kPrevAlpha = 0.66;   // what draw_get_colour/draw_get_alpha answer

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++g_AnyCallCount;
        const std::string fn = name;
        if (fn == "asset_get_index") {
            const std::string want = args[0].ToString();
            if (want == "UI_Hud_Talent_obj") return RValue(world.hudTalentObjectResolves ? kHudObjIdx : -1.0);
            if (want == "Universal_Double_Cast_obj") {
                ++g_DcResolveCalls;
                return RValue(world.doubleCastObjectResolves ? kDcObjIdx : -1.0);
            }
            return RValue(world.aoeObjectResolves ? kAoeObjIdx : -1.0);
        }
        if (fn == "instance_number") {
            ++g_InstanceEnumCalls;
            if (world.instanceNumberThrows) throw std::runtime_error("instance_number EXCEPTION");
            return RValue((double)world.instances.size());
        }
        if (fn == "instance_find") {
            ++g_InstanceEnumCalls;
            const double obj = args[0].ToDouble();
            const int i = (int)args[1].ToDouble();
            if (obj == kHudObjIdx) {
                if (!world.hudTalentInstanceExists || i != 0) return RValue();   // VALUE_UNDEFINED
                RValue r; r.m_Kind = VALUE_REF; r.text = "hud:0";
                return r;
            }
            if (i < 0 || (size_t)i >= world.instances.size()) return RValue();   // VALUE_UNDEFINED
            RValue r; r.m_Kind = VALUE_REF; r.text = "aoe:" + std::to_string(i);
            return r;
        }
        if (fn == "variable_instance_get") {
            if (args[0].inst) {   // a script's self, handed over as RValue(S)
                const CInstance* self = args[0].inst;
                if (args[1].ToString() != "object_index") return RValue();
                if (self->objectIndexThrows) throw std::runtime_error("object_index EXCEPTION");
                RValue r; r.m_Kind = self->objectIndexKind; r.number = self->objectIndex;
                return r;
            }
            const std::string tag = args[0].text;
            const std::string field = args[1].ToString();
            if (tag == "hud:0") {
                if (field != "row0") return RValue();
                if (!world.row0IsArray) return RValue();   // VALUE_UNDEFINED, not an array
                RValue r; r.m_Kind = VALUE_ARRAY; r.text = "row0";
                return r;
            }
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
        if (fn == "array_length") {
            if (args[0].text == "row0") return RValue((double)world.row0.size());
            return RValue(0.0);
        }
        if (fn == "array_get") {
            if (args[0].text == "row0") {
                const int i = (int)args[1].ToDouble();
                if (i < 0 || (size_t)i >= world.row0.size()) return RValue();
                RValue r; r.m_Kind = VALUE_OBJECT; r.text = "row0elem:" + std::to_string(i);
                return r;
            }
            return RValue();
        }
        if (fn == "variable_struct_get") {
            const std::string tag = args[0].text;
            const std::string field = args[1].ToString();
            if (tag.rfind("row0elem:", 0) == 0) {
                const size_t i = (size_t)std::stoi(tag.substr(9));
                if (i >= world.row0.size()) return RValue();
                const HudRow0Elem& e = world.row0[i];
                if (field == "talentId") return RValue(e.talentId);
                if (field == "navBboxX") return RValue(e.navBboxX);
                if (field == "navBboxY") return RValue(e.navBboxY);
                if (field == "navBboxWidth") return RValue(e.navBboxWidth);
                if (field == "navBboxHeight") return RValue(e.navBboxHeight);
            }
            return RValue();
        }
        if (fn == "draw_get_colour") return RValue(kPrevColour);
        if (fn == "draw_get_alpha") return RValue(kPrevAlpha);
        if (fn == "make_colour_rgb") return RValue(123456.0);
        if (fn == "draw_set_colour") { g_LastSetColour = args[0].ToDouble(); return RValue(); }
        if (fn == "draw_set_alpha") { g_LastSetAlpha = args[0].ToDouble(); return RValue(); }
        if (fn == "draw_rectangle") {
            if (world.drawRectangleThrows) throw std::runtime_error("draw_rectangle EXCEPTION");
            ++g_RectangleDraws; return RValue();
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
static void checkNear(const std::string& label, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-6;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}

static void resetWorld() {
    world = World{};
}

// T1: the TalentUseClass original, as the trampoline HookOneScript hands the
// hook. It counts, and writes a return value the way a real call would, so a
// refused call's untouched result is distinguishable; a refused call must
// never reach it.
static RValue& FakeTalentUseClassOriginal(CInstance*, CInstance*, RValue& R, int, RValue**) {
    ++g_TrampCalls;
    R = MakeReal(777);
    return R;
}
struct GuardCall { long tramp; bool returnedResult; };
// One TalentUseClass call with session 1's eight-argument shape.
static GuardCall CallGuard(CInstance* self, double talent, bool a4) {
    RValue a0 = MakeReal(talent), a1 = MakeReal(0), a2 = MakeReal(0), a3 = MakeReal(0);
    RValue a4v = MakeBool(a4), a5 = MakeReal(0), a6 = MakeReal(-1), a7 = MakeReal(-1);
    RValue* args[8] = { &a0, &a1, &a2, &a3, &a4v, &a5, &a6, &a7 };
    RValue result = MakeReal(-12345);
    const long before = g_TrampCalls;
    RValue& r = HookTalentUseClass(self, nullptr, result, 8, args);
    return { g_TrampCalls - before, &r == &result && r.number == -12345 };
}
static void resetGuard(bool enabled) {
    world = World{};
    g_OrigTalentUseClass = &FakeTalentUseClassOriginal;
    ForgePact::ToggleGuardMod::Instance().SetEnabled(enabled, /*alreadyHooked=*/true);
    g_TgdRefused = 0; g_TgdPassed = 0; g_TgdProcSeen = 0; g_TgdSelfUnreadable = 0; g_TgdObjUnresolved = 0;
    g_ToggleGuardDcObjIdx.store(-1);
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

    // ---- the shipped indicator (P2): ToggleIndicatorDraw() -----------------
    // requireMarker is always true here - session 4 measured the plain-cast
    // flash (D-R2), matching the shipped call.

    // 19. OFF: the very first statement is an atomic load and return - no
    //     runtime call happens at all.
    resetWorld();
    g_ToggleBorderOn.store(false);
    world.instances = { OwnMarked() };   // must not matter - never read while off
    {
        long before = g_AnyCallCount;
        ToggleIndicatorDraw();
        checkInt("indicator_off/no_runtime_calls", g_AnyCallCount - before, 0);
    }

    // 20. ON, own+marked, slot found: outlines the slot exactly once (three
    //     nested passes, for thickness).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        g_TibDrawn = 0; g_RectangleDraws = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/own_on_outlines_slot", g_TibDrawn, 1);
        checkInt("indicator_on/own_on_outlines_slot/rectangles", g_RectangleDraws, 3);
    }

    // 21. ON, no AOE at all: Off, draws nothing.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = {};
    {
        g_TibDrawn = 0; g_RectangleDraws = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/off_draws_nothing", g_TibDrawn, 0);
        checkInt("indicator_on/off_draws_nothing/rectangles", g_RectangleDraws, 0);
    }

    // 22. ON, every AOE's isMyClient reads bool:false ("foreign only"): Off,
    //     draws nothing.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { Foreign(0.09) };
    {
        g_TibDrawn = 0; g_RectangleDraws = 0; g_TibForeign = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/foreign_only_draws_nothing", g_TibDrawn, 0);
        checkInt("indicator_on/foreign_only_draws_nothing/counter", g_TibForeign, 1);
    }

    // 23. ON, the AOE object itself never resolves (Unreadable): draws
    //     nothing, and counts it.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.aoeObjectResolves = false;
    {
        g_TibDrawn = 0; g_RectangleDraws = 0; g_TibUnreadable = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/unreadable_draws_nothing_and_counts", g_TibDrawn, 0);
        checkInt("indicator_on/unreadable_draws_nothing_and_counts/counter", g_TibUnreadable, 1);
    }

    // 24. ON, own+marked, but the slot never resolves (no element carries
    //     talentId 240): draws nothing, and counts it separately from
    //     Unreadable - via noTalent, the follow-up's three-way split of the
    //     old single noSlot counter.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.row0.clear();
    {
        g_TibDrawn = 0; g_RectangleDraws = 0; g_TibNoTalent = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/slot_not_found_draws_nothing_and_counts", g_TibDrawn, 0);
        checkInt("indicator_on/slot_not_found_draws_nothing_and_counts/counter", g_TibNoTalent, 1);
    }

    // 25. ON, own but unmarked (purgatory readable, <= 0): draws nothing -
    //     the plain-cast flash session 4 measured (D-R2).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnUnmarked() };
    {
        g_TibDrawn = 0; g_RectangleDraws = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/unmarked_own_draws_nothing", g_TibDrawn, 0);
    }

    // 26. No caching: a later draw in the SAME session picks up a state
    //     change with no restart.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = {};
    {
        g_TibDrawn = 0; g_RectangleDraws = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/state_reread_every_draw/first_off", g_TibDrawn, 0);
        world.instances = { OwnMarked(0.09) };
        ToggleIndicatorDraw();
        checkInt("indicator_on/state_reread_every_draw", g_TibDrawn, 1);
    }

    // 27. Colour and alpha are saved before the first draw_set_ and restored
    //     to their PRE-draw values after the last draw - the same pattern
    //     TgProbeDrawMark uses.
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        g_LastSetColour = -999; g_LastSetAlpha = -999;
        ToggleIndicatorDraw();
        checkNear("indicator_on/draw_colour_and_alpha_restored/colour", g_LastSetColour, kPrevColour);
        checkNear("indicator_on/draw_colour_and_alpha_restored/alpha", g_LastSetAlpha, kPrevAlpha);
    }

    // 28. Follow-up: a throwing draw_rectangle stub. The catch after the
    //     outline loop must count the exception rather than swallow it
    //     uncounted, and drawn must stay 0 (the draw did not complete).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.drawRectangleThrows = true;
    {
        g_TibDrawn = 0; g_TibDrawExc = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/draw_exception_counts/drawn", g_TibDrawn, 0);
        checkInt("indicator_on/draw_exception_counts", g_TibDrawExc, 1);
    }

    // 29. Follow-up: ToggleIndicatorFindSlot's noSlot split into three
    //     counters that mean something different, each failure mode leaving
    //     the other two untouched. No HUD object -> noHud; row0 not an
    //     array -> noRow0; no element with talentId 240 -> noTalent (the
    //     same case scenario 24 above exercises, reused here for the split).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.hudTalentObjectResolves = false;
    {
        g_TibNoHud = 0; g_TibNoRow0 = 0; g_TibNoTalent = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/slot_failures_are_split/noHud", g_TibNoHud, 1);
        checkInt("indicator_on/slot_failures_are_split/noHud/others_zero", g_TibNoRow0 + g_TibNoTalent, 0);
    }
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.row0IsArray = false;
    {
        g_TibNoHud = 0; g_TibNoRow0 = 0; g_TibNoTalent = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/slot_failures_are_split/noRow0", g_TibNoRow0, 1);
        checkInt("indicator_on/slot_failures_are_split/noRow0/others_zero", g_TibNoHud + g_TibNoTalent, 0);
    }
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.row0.clear();
    {
        g_TibNoHud = 0; g_TibNoRow0 = 0; g_TibNoTalent = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/slot_failures_are_split/noTalent", g_TibNoTalent, 1);
        checkInt("indicator_on/slot_failures_are_split", g_TibNoHud + g_TibNoRow0, 0);
    }

    // ---- T1: the re-cast guard, HookTalentUseClass ---------------------------
    // Session 1's measured call shapes (docs/toggle-skills-research.md, Q1/Q5):
    // a player cast is self=Player_obj a0=240 a4=true, chained by a0=243
    // a4=false; the double-cast proc is self=Universal_Double_Cast_obj a0=240
    // a4=false with world x,y in a6/a7.
    CInstance dcSelf; dcSelf.objectIndex = kDcObjIdx;
    CInstance playerSelf; playerSelf.objectIndex = kPlayerObjIdx;

    // 30. Baseline: the guard is off (the default). A proc of Soul Spurn goes
    //     straight to the original, and the hook makes no runtime call at all.
    resetGuard(false);
    {
        const long calls = g_AnyCallCount;
        GuardCall c = CallGuard(&dcSelf, 240.0, false);
        checkInt("guard_off/proc_passes_and_no_runtime_call/runtime_calls", g_AnyCallCount - calls, 0);
        checkInt("guard_off/proc_passes_and_no_runtime_call", c.tramp, 1);
    }

    // 31. Target: guard on, the double-cast object re-casting Soul Spurn is
    //     refused - the original is not called and the result is handed back
    //     untouched.
    resetGuard(true);
    {
        GuardCall c = CallGuard(&dcSelf, 240.0, false);
        checkBool("guard_on/proc_of_guarded_talent_refused/result_untouched", c.returnedResult, true);
        checkInt("guard_on/proc_of_guarded_talent_refused/refused", g_TgdRefused, 1);
        checkInt("guard_on/proc_of_guarded_talent_refused", c.tramp, 0);
    }

    // 32. A proc of another talent (Healing Zone, 252) passes, and is still
    //     seen as a proc.
    resetGuard(true);
    {
        GuardCall c = CallGuard(&dcSelf, 252.0, false);
        checkInt("guard_on/proc_of_other_talent_passes/procSeen", g_TgdProcSeen, 1);
        checkInt("guard_on/proc_of_other_talent_passes/refused", g_TgdRefused, 0);
        checkInt("guard_on/proc_of_other_talent_passes", c.tramp, 1);
    }

    // 33. The player's own cast of Soul Spurn passes.
    resetGuard(true);
    {
        GuardCall c = CallGuard(&playerSelf, 240.0, true);
        checkInt("guard_on/player_cast_passes/refused", g_TgdRefused, 0);
        checkInt("guard_on/player_cast_passes", c.tramp, 1);
    }

    // 34. The chained follow-up cast (243, a4 false) from the player passes.
    resetGuard(true);
    {
        GuardCall c = CallGuard(&playerSelf, 243.0, false);
        checkInt("guard_on/player_chain_passes", c.tramp, 1);
    }

    // 35. The caller's object_index cannot be read: fail open, and count it.
    resetGuard(true);
    {
        CInstance broken; broken.objectIndex = kDcObjIdx; broken.objectIndexThrows = true;
        GuardCall c = CallGuard(&broken, 240.0, false);
        checkInt("guard_on/self_unreadable_passes_and_counts/selfUnreadable", g_TgdSelfUnreadable, 1);
        checkInt("guard_on/self_unreadable_passes_and_counts", c.tramp, 1);
    }

    // 36. The double-cast object's name does not resolve: fail open, count
    //     it, and never cache the negative - a later call resolves again and
    //     then refuses.
    resetGuard(true);
    world.doubleCastObjectResolves = false;
    {
        const long resolvesBefore = g_DcResolveCalls;
        GuardCall c = CallGuard(&dcSelf, 240.0, false);
        checkInt("guard_on/double_cast_object_unresolved_passes_and_counts/objUnresolved", g_TgdObjUnresolved, 1);
        checkInt("guard_on/double_cast_object_unresolved_passes_and_counts/trampoline", c.tramp, 1);
        world.doubleCastObjectResolves = true;
        GuardCall again = CallGuard(&dcSelf, 240.0, false);
        checkInt("guard_on/double_cast_object_unresolved_passes_and_counts/re_resolved", g_DcResolveCalls - resolvesBefore, 2);
        checkInt("guard_on/double_cast_object_unresolved_passes_and_counts", again.tramp, 0);
        // Once resolved, the index is cached: a third call resolves nothing.
        CallGuard(&dcSelf, 240.0, false);
        checkInt("guard_on/double_cast_object_unresolved_passes_and_counts/cached", g_DcResolveCalls - resolvesBefore, 2);
    }

    // 37. D-N1: the guard never reads the toggle's state - no instance_number
    //     or instance_find call on any path, refused or passed.
    resetGuard(true);
    world.instances = { OwnMarked(0.09) };   // an ON toggle, which must not be consulted
    {
        const long enumBefore = g_InstanceEnumCalls;
        CallGuard(&dcSelf, 240.0, false);
        CallGuard(&playerSelf, 240.0, true);
        CallGuard(&dcSelf, 252.0, false);
        checkInt("guard_on/state_not_consulted", g_InstanceEnumCalls - enumBefore, 0);
    }

    // 38. The counters over one mixed sequence: a refused proc, the player's
    //     cast and its chain, a Healing Zone proc.
    resetGuard(true);
    {
        CallGuard(&dcSelf, 240.0, false);
        CallGuard(&playerSelf, 240.0, true);
        CallGuard(&playerSelf, 243.0, false);
        CallGuard(&dcSelf, 252.0, false);
        checkInt("guard_on/counters/refused", g_TgdRefused, 1);
        checkInt("guard_on/counters/passed", g_TgdPassed, 3);
        checkInt("guard_on/counters/procSeen", g_TgdProcSeen, 2);
        checkInt("guard_on/counters", g_TgdSelfUnreadable + g_TgdObjUnresolved, 0);
    }

    // 39. The caller's object_index in every numeric kind the runner can hand
    //     back - VALUE_REF (what this runner returns), the three plain number
    //     kinds, and VALUE_REF with a flag bit set above the kind - identifies
    //     the double-cast object, and its Soul Spurn re-cast is refused.
    {
        struct KindCase { const char* name; int kind; };
        const KindCase kinds[] = {
            { "ref", VALUE_REF }, { "real", VALUE_REAL }, { "int32", VALUE_INT32 },
            { "int64", VALUE_INT64 }, { "ref_flagged", (int)(VALUE_REF | kKindFlagBit) },
        };
        for (const KindCase& k : kinds) {
            resetGuard(true);
            CInstance self; self.objectIndex = kDcObjIdx; self.objectIndexKind = k.kind;
            GuardCall c = CallGuard(&self, 240.0, false);
            const std::string label = std::string("guard_on/self_object_index_kinds/") + k.name;
            checkInt(label + "/selfUnreadable", g_TgdSelfUnreadable, 0);
            checkInt(label + "/refused", g_TgdRefused, 1);
            checkInt(label, c.tramp, 0);
        }
    }

    // 40. Negative control for 39: an object_index that is not an index at
    //     all (undefined, a string, a bool carrying the same number) is
    //     unreadable - counted, and the call passes. Widening the accepted
    //     kinds must not become accepting anything.
    {
        struct KindCase { const char* name; int kind; };
        const KindCase kinds[] = {
            { "undefined", VALUE_UNDEFINED }, { "string", VALUE_STRING }, { "bool", VALUE_BOOL },
        };
        for (const KindCase& k : kinds) {
            resetGuard(true);
            CInstance self; self.objectIndex = kDcObjIdx; self.objectIndexKind = k.kind;
            GuardCall c = CallGuard(&self, 240.0, false);
            const std::string label = std::string("guard_on/self_object_index_not_an_index_passes/") + k.name;
            checkInt(label + "/selfUnreadable", g_TgdSelfUnreadable, 1);
            checkInt(label + "/refused", g_TgdRefused, 0);
            checkInt(label, c.tramp, 1);
        }
    }

    // The read never makes a player-resolving call, in any scenario above -
    // counted here, at the end, so it covers every one of them.
    checkInt("read/no_player_lookup", g_ResolveCalls, 0);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
