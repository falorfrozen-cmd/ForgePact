// Behavioral harness for the toggle-skill active indicator's read (issue
// #11, Track B).
//
// The Python runner injects the REAL ToggleSkillMod.hpp header, the REAL
// ToggleIndicatorResolveAoeObject/ToggleIndicatorRead below and the real
// scan-cap constant, so no number or branch here restates one from the
// plugin. Only the game API is replaced.
//
// This pins the decision the research doc's "Co-op / ownership: the answer"
// and "The read, and exactly what has been proven" sections settled on
// BEFORE any drawing code exists: zero instances answers Off without ever
// resolving the local player; a resolved AOE whose own playerNumber cannot
// be read is unattributed and never lights the indicator; and the object
// itself failing to resolve is a different, stronger failure (Unreadable)
// than "resolved but empty" (Off).
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
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { return number; }
    std::string ToString() const { return text; }
};

// A stand-in for HeroSiege::Objects - the harness needs the AOE object named
// by the SDK constant to exist and resolve to a string, not the whole SDK.
namespace HeroSiege { namespace Objects {
enum class GameObject { White_Mage_Soul_Spurn_AOE_obj, UI_Hud_Talent_obj };
inline const char* GetObjectName(GameObject) { return "White_Mage_Soul_Spurn_AOE_obj"; }
}}

// ---- the controlled world -------------------------------------------------
struct AoeInst { double playerNumber = 0; bool hasPlayerNumber = true; };
struct World {
    bool aoeObjectResolves = true;
    std::vector<AoeInst> instances;
    bool instanceNumberThrows = false;   // instance_number itself throws
    bool playerResolves = true;
    int playerKind = VALUE_REF;      // what this runner really hands back
    bool playerNumberReadable = true;
    double playerNumberValue = 1.0;
};
static World world;
static long g_ResolveCalls = 0;

// A player/instance handle tagged by identity rather than by kind - the
// harness's variable_instance_get looks at the tag, exactly as the real
// runtime looks at which instance id/ref was actually passed, not at the
// RValue's own kind (which this runner is free to hand back as VALUE_REF).
static RValue TagRef(const std::string& tag, int kind) {
    RValue r; r.m_Kind = kind; r.text = tag; return r;
}

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
            return TagRef("aoe:" + std::to_string(i), VALUE_REF);
        }
        if (fn == "variable_instance_get") {
            if (args[1].ToString() != "playerNumber") return RValue();
            const std::string tag = args[0].text;
            if (tag == "player") {
                return world.playerNumberReadable ? RValue(world.playerNumberValue) : RValue();
            }
            if (tag.rfind("aoe:", 0) == 0) {
                const size_t i = (size_t)std::stoi(tag.substr(4));
                if (i < world.instances.size() && world.instances[i].hasPlayerNumber)
                    return RValue(world.instances[i].playerNumber);
                return RValue();   // VALUE_UNDEFINED: unattributed
            }
            return RValue();
        }
        return RValue();
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

// The local player as ModuleMain.cpp's own HhResolveLocalPlayer would hand
// it back - single-arg form, matching how ToggleIndicatorRead calls it.
static bool HhResolveLocalPlayer(RValue& out) {
    ++g_ResolveCalls;
    if (!world.playerResolves) return false;
    out = TagRef("player", world.playerKind);
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
    g_ResolveCalls = 0;
}

int main() {
    // 1. Zero instances answers Off without ever resolving the local player -
    //    a real, cheap negative that must not pay for a player lookup.
    resetWorld();
    world.instances = {};
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/no_aoe_is_off_without_resolving_player", state, ForgePact::ToggleIndicatorState::Off);
        checkInt("read/no_aoe_is_off_without_resolving_player/resolve_calls", g_ResolveCalls, 0);
    }

    // 2. One AOE whose playerNumber matches the local player: On.
    resetWorld();
    world.instances = { { 1.0, true } };
    world.playerNumberValue = 1.0;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/own_aoe_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/own_aoe_is_on/mine", d.mine, 1);
    }

    // 3. This runner hands back VALUE_REF for the local player, not
    //    VALUE_OBJECT (Known Limitations item 7) - the read must not gate on
    //    the player RValue's own kind.
    resetWorld();
    world.instances = { { 1.0, true } };
    world.playerNumberValue = 1.0;
    world.playerKind = VALUE_REF;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/valueref_player_is_on", state, ForgePact::ToggleIndicatorState::On);
    }

    // 4. A foreign player's AOE alone: Off, not On and not Unreadable.
    resetWorld();
    world.instances = { { 2.0, true } };
    world.playerNumberValue = 1.0;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/foreign_aoe_is_off", state, ForgePact::ToggleIndicatorState::Off);
        checkInt("read/foreign_aoe_is_off/others", d.others, 1);
    }

    // 5. Own AOE plus a foreign one: still On - a foreign AOE never masks ours.
    resetWorld();
    world.instances = { { 1.0, true }, { 2.0, true } };
    world.playerNumberValue = 1.0;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/own_and_foreign_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/own_and_foreign_is_on/mine", d.mine, 1);
        checkInt("read/own_and_foreign_is_on/others", d.others, 1);
    }

    // 6. Two own instances (e.g. a double-cast proc, Track A Q5): still On.
    resetWorld();
    world.instances = { { 1.0, true }, { 1.0, true } };
    world.playerNumberValue = 1.0;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/two_own_is_on", state, ForgePact::ToggleIndicatorState::On);
        checkInt("read/two_own_is_on/mine", d.mine, 2);
    }

    // 7. Every AOE present is unattributed (its own playerNumber unreadable):
    //    Unreadable, not Off - a foreign AOE fails toward absent, but an
    //    unreadable one must not be guessed either way.
    resetWorld();
    world.instances = { { 0.0, false }, { 0.0, false } };
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/unattributed_only_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
        checkInt("read/unattributed_only_is_unreadable/unattributed", d.unattributed, 2);
    }

    // 8. Instances exist but the local player cannot be resolved at all.
    resetWorld();
    world.instances = { { 1.0, true } };
    world.playerResolves = false;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/no_local_player_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
    }

    // 9. The player resolves but its own playerNumber cannot be read.
    resetWorld();
    world.instances = { { 1.0, true } };
    world.playerResolves = true;
    world.playerNumberReadable = false;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/local_number_unreadable_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
    }

    // 10. The AOE object itself does not resolve by name at all - a
    //     different, stronger failure than "resolved but zero instances".
    resetWorld();
    world.aoeObjectResolves = false;
    world.instances = { { 1.0, true } };   // must not matter - object never resolved
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/object_unresolved_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
        checkInt("read/object_unresolved_is_unreadable/resolve_calls", g_ResolveCalls, 0);
    }

    // 11. The scan is capped: more instances exist than the budget scans, and
    //     the read still decides correctly on what it actually visited.
    resetWorld();
    for (int i = 0; i < kToggleIndicatorScanCap + 6; ++i) world.instances.push_back({ 1.0, true });
    world.playerNumberValue = 1.0;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/scan_is_capped", state, ForgePact::ToggleIndicatorState::On);
        checkBool("read/scan_is_capped/capped", d.capped, true);
        checkInt("read/scan_is_capped/n", d.n, kToggleIndicatorScanCap + 6);
        checkInt("read/scan_is_capped/mine", d.mine, kToggleIndicatorScanCap);
    }

    // 12. No caching across calls: a call reflects the CURRENT world, not a
    //     stale snapshot from an earlier call in the same session.
    resetWorld();
    world.instances = {};
    {
        ForgePact::ToggleIndicatorReadDetail d1;
        auto state1 = ToggleIndicatorRead(&d1, nullptr);
        checkState("read/reread_every_call/first_off", state1, ForgePact::ToggleIndicatorState::Off);
        world.instances = { { 1.0, true } };
        world.playerNumberValue = 1.0;
        ForgePact::ToggleIndicatorReadDetail d2;
        auto state2 = ToggleIndicatorRead(&d2, nullptr);
        checkState("read/reread_every_call", state2, ForgePact::ToggleIndicatorState::On);
    }

    // 13. `spurn as <n>`: overriding the local number excludes the real own
    //     AOE from "mine" and counts it as foreign instead - a non-mutating
    //     negative control, so the real player number is never consulted.
    resetWorld();
    world.instances = { { 1.0, true } };
    world.playerNumberValue = 1.0;   // the real answer would be On
    {
        const double overrideNumber = 2.0;
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, &overrideNumber);
        checkState("read/override_number_excludes_own", state, ForgePact::ToggleIndicatorState::Off);
        checkInt("read/override_number_excludes_own/others", d.others, 1);
        checkInt("read/override_number_excludes_own/mine", d.mine, 0);
        checkInt("read/override_number_excludes_own/resolve_calls", g_ResolveCalls, 0);
    }

    // 14. `instance_number` itself throws: a failed read, not a measured
    //     zero. The catch's `d.n = 0` fallback must not be mistaken for a
    //     real, cheap Off - it must count as a failure instead.
    resetWorld();
    world.instances = { { 1.0, true } };   // must not matter - the count never completes
    world.instanceNumberThrows = true;
    {
        ForgePact::ToggleIndicatorReadDetail d;
        auto state = ToggleIndicatorRead(&d, nullptr);
        checkState("read/instance_number_throw_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
        checkBool("read/instance_number_throw_is_unreadable/countReadFailed", d.countReadFailed, true);
        checkInt("read/instance_number_throw_is_unreadable/resolve_calls", g_ResolveCalls, 0);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
