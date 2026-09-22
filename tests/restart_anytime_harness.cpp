// Behavioral harness for "Restart zone at any time" (issue #8; `restartanytime`).
//
// The Python runner (test_restart_anytime_behavior.py) injects the REAL
// RestartAnytimeMod.hpp and the REAL HhUsableInstance and
// HookRestartAnytimeSetFocus from ModuleMain.cpp at the PRODUCTION_* markers
// below. Only the game API is replaced; no game process, character or
// installed DLL is touched.
//
// The stand-in runner answers the kinds this runner was measured to produce
// (docs/restart-always-available-research.md, rounds 2 and 3): the button
// UiSetFocus is handed arrives as VALUE_REF, its `manualDisable` and
// `enabled` read back as VALUE_BOOL and its `uiNodeCallstack` as
// VALUE_STRING. A stand-in that could only answer VALUE_OBJECT or a real
// would pass a hook that rejects the one kind the game actually hands it.
//
// Every target/* scenario was run red first against a pass-through body (the
// hook reduced to its trampoline call); the failing assertion each produced
// is recorded beside it.
#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_STRING, VALUE_ARRAY, VALUE_PTR, VALUE_UNDEFINED, VALUE_OBJECT, VALUE_INT32,
       VALUE_INT64 = 10, VALUE_BOOL = 13, VALUE_REF = 15 };

struct FakeInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    FakeInstance* inst = nullptr;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(bool b) : m_Kind(VALUE_BOOL), number(b ? 1.0 : 0.0) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
struct FakeInstance {
    std::map<std::string, RValue> fields;
    bool throwOnSet = false;
};
struct CInstance { int id = 0; };

static std::string Kind(const RValue& v)
{
    switch (v.m_Kind) {
    case VALUE_BOOL: return std::string("bool:") + (v.ToBoolean() ? "true" : "false");
    case VALUE_REAL: return "real:" + std::to_string((int)v.number);
    case VALUE_STRING: return "string:" + v.text;
    case VALUE_UNDEFINED: return "undefined";
    default: return "kind" + std::to_string(v.m_Kind);
    }
}

// ---- the controlled world -------------------------------------------------
struct World {
    long builtins = 0;       // every runtime call the hook made
    long writes = 0;         // variable_instance_set calls that landed
    long origCalls = 0;      // the game's own UiSetFocus
    std::string seenAtOriginal = "n/a";   // the Restart button's gate as the game's body saw it
    std::vector<std::string> log;
};
static World world;
static FakeInstance* g_restartForOriginal = nullptr;

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++world.builtins;
        const std::string fn = name;
        if (fn == "variable_instance_get") {
            if (args.size() < 2 || !args[0].inst) throw std::runtime_error("not an instance");
            auto it = args[0].inst->fields.find(args[1].ToString());
            return it == args[0].inst->fields.end() ? RValue() : it->second;
        }
        if (fn == "variable_instance_set") {
            if (args.size() < 3 || !args[0].inst) throw std::runtime_error("not an instance");
            if (args[0].inst->throwOnSet) throw std::runtime_error("set failed");
            args[0].inst->fields[args[1].ToString()] = args[2];
            ++world.writes;
            return RValue();
        }
        return RValue();
    }
};
static FakeRunner g_Runner;
static FakeRunner* g_Yytk = &g_Runner;

static void Out(const std::string& s) { world.log.push_back(s); }
static std::string Describe(const RValue& v) { return Kind(v); }

namespace HeroSiege { namespace Scripts {
inline constexpr std::string_view gml_Script_UiSetFocus = "gml_Script_UiSetFocus";
}}

using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);
static RValue& FakeOriginal(CInstance*, CInstance*, RValue& result, int, RValue**)
{
    ++world.origCalls;
    if (g_restartForOriginal) world.seenAtOriginal = Kind(g_restartForOriginal->fields["manualDisable"]);
    return result;
}
static PFUNC_YYGMLScript g_OrigUiSetFocus = &FakeOriginal;

// PRODUCTION_HEADER

// PRODUCTION_FUNCTIONS

// ---- fixtures -------------------------------------------------------------
static std::vector<std::unique_ptr<FakeInstance>> g_arena;

// A pause-menu button as the round-2 dump read it: an instance with a real
// `x`, its own `uiNodeCallstack` string and the two bool members.
static FakeInstance* button(const char* callstack, RValue manualDisable)
{
    g_arena.push_back(std::make_unique<FakeInstance>());
    FakeInstance* b = g_arena.back().get();
    b->fields["x"] = RValue(640.0);
    if (callstack) b->fields["uiNodeCallstack"] = RValue(callstack);
    b->fields["manualDisable"] = manualDisable;
    b->fields["enabled"] = RValue(false);
    return b;
}

// How this runner hands an instance over: VALUE_REF, not VALUE_OBJECT.
static RValue ref(FakeInstance* i)
{
    RValue v;
    v.m_Kind = VALUE_REF;
    v.inst = i;
    return v;
}

struct Counts { long written, passed, otherNode, unreadable; };
static Counts snapshot()
{
    auto& m = ForgePact::RestartAnytimeMod::Instance();
    return { m.Written(), m.Passed(), m.OtherNode(), m.Unreadable() };
}

static Counts g_before;
static void begin(FakeInstance* restartSeenByOriginal = nullptr)
{
    world = World();
    g_restartForOriginal = restartSeenByOriginal;
    g_before = snapshot();
}

// One UiSetFocus call the way the pause menu makes it: self the pause
// instance, a0 the focused node, a1 the pause instance.
static RValue* g_retAddr = nullptr;
static void focus(std::vector<RValue> args, CInstance* self = nullptr)
{
    std::vector<RValue*> argv;
    for (RValue& a : args) argv.push_back(&a);
    RValue result;
    RValue& ret = HookRestartAnytimeSetFocus(self, nullptr, result, (int)argv.size(), argv.empty() ? nullptr : argv.data());
    g_retAddr = (&ret == &result) ? &result : nullptr;
    if (!g_retAddr) world.log.push_back("<hook returned a reference that is not the original's result>");
}

static void report(const char* label, const std::string& extra = "")
{
    const Counts now = snapshot();
    std::cout << "SCENARIO " << label
              << " origcalls=" << world.origCalls
              << " builtins=" << world.builtins
              << " writes=" << world.writes
              << " written=" << (now.written - g_before.written)
              << " passed=" << (now.passed - g_before.passed)
              << " otherNode=" << (now.otherNode - g_before.otherNode)
              << " unreadable=" << (now.unreadable - g_before.unreadable)
              << " seenAtOriginal=" << world.seenAtOriginal
              << (extra.empty() ? "" : " " + extra) << "\n";
    for (const std::string& line : world.log) std::cout << "LOG " << label << " :: " << line << "\n";
}

int main()
{
    auto& mod = ForgePact::RestartAnytimeMod::Instance();
    RValue pause = ref(button(nullptr, RValue(false)));   // stands in for UI_Pause_obj

    // baseline: nothing has been sent, so the mod is off, and off is the
    // vanilla path - the original runs, nothing is read, nothing is written.
    {
        FakeInstance* restart = button("PauseRestart", RValue(true));
        begin(restart);
        focus({ ref(restart), pause });
        report("baseline/off_by_default", std::string("enabled=") + (mod.IsEnabled() ? "1" : "0")
               + " restartValue=" + Kind(restart->fields["manualDisable"]));
    }

    // baseline: on, then off again (`restartanytime 0` keeps the hook and
    // clears the flag) - still the vanilla path.
    {
        mod.SetEnabled(true, true);
        mod.SetEnabled(false, true);
        FakeInstance* restart = button("PauseRestart", RValue(true));
        begin(restart);
        focus({ ref(restart), pause });
        report("baseline/off_calls_original_and_writes_nothing", "restartValue=" + Kind(restart->fields["manualDisable"]));
    }

    mod.SetEnabled(true, true);

    // red (pass-through body): test_on_writes_only_when_arg0_is_the_restart_button
    //   AssertionError: 0 != 1 : writes
    {
        FakeInstance* restart = button("PauseRestart", RValue(true));
        begin(restart);
        focus({ ref(restart), pause });
        report("target/on_writes_only_when_arg0_is_the_restart_button",
               "restartValue=" + Kind(restart->fields["manualDisable"])
               + " enabledValue=" + Kind(restart->fields["enabled"]));
    }

    // Every other node, including the negative control that is shaped exactly
    // like the Restart button but carries another button's key: never written.
    // The call's self being the Restart button changes nothing - identity is
    // a0's own member, never position or self.
    // red (pass-through body): test_on_leaves_every_other_node_untouched
    //   AssertionError: 0 != 6 : otherNode
    {
        FakeInstance* resume = button("PauseResume", RValue(true));
        FakeInstance* keyless = button(nullptr, RValue(true));
        FakeInstance* restart = button("PauseRestart", RValue(true));
        FakeInstance* numberKey = button(nullptr, RValue(true));
        numberKey->fields["uiNodeCallstack"] = RValue(7.0);
        begin();
        focus({ ref(resume), pause });
        focus({ ref(keyless), pause });
        focus({ ref(numberKey), pause });
        focus({ RValue(262264.0), pause });                       // a number, not an instance
        focus({});                                                // no argument at all
        CInstance restartAsSelf;
        focus({ ref(resume), ref(restart) }, &restartAsSelf);     // Restart elsewhere in the call
        report("target/on_leaves_every_other_node_untouched",
               "resumeValue=" + Kind(resume->fields["manualDisable"])
               + " keylessValue=" + Kind(keyless->fields["manualDisable"])
               + " restartValue=" + Kind(restart->fields["manualDisable"]));
    }

    // The Restart button whose gate cannot be read as a bool or number (absent,
    // a string), or whose write throws: passed through untouched, counted.
    // red (pass-through body): test_on_unreadable_member_passes_and_counts
    //   AssertionError: 0 != 3 : unreadable
    {
        FakeInstance* absent = button("PauseRestart", RValue(true));
        absent->fields.erase("manualDisable");
        FakeInstance* text = button("PauseRestart", RValue("true"));
        FakeInstance* throws = button("PauseRestart", RValue(true));
        throws->throwOnSet = true;
        begin();
        focus({ ref(absent), pause });
        focus({ ref(text), pause });
        focus({ ref(throws), pause });
        report("target/on_unreadable_member_passes_and_counts",
               "absentValue=" + Kind(absent->fields["manualDisable"])
               + " textValue=" + Kind(text->fields["manualDisable"]));
    }

    // The original runs exactly once per call, AFTER the write, so the game's
    // own body sees the open gate - and the hook returns the original's result.
    // red (pass-through body): test_on_calls_the_original_exactly_once
    //   AssertionError: 'bool:true' != 'bool:false' (seenAtOriginal: the
    //   game's body saw the closed gate; origcalls=1 held even then)
    {
        FakeInstance* restart = button("PauseRestart", RValue(true));
        begin(restart);
        focus({ ref(restart), pause });
        report("target/on_calls_the_original_exactly_once",
               std::string("sameResult=") + (g_retAddr ? "1" : "0"));
    }

    // The value is written in the kind read at entry: a bool stays a bool
    // (what the game holds), a real stays a real.
    // red (pass-through body): test_on_preserves_the_kind_read_at_entry
    //   AssertionError: 'bool:true' != 'bool:false' (boolWritten; the
    //   scenario printed boolWritten=bool:true realWritten=real:1)
    {
        FakeInstance* asBool = button("PauseRestart", RValue(true));
        FakeInstance* asReal = button("PauseRestart", RValue(1.0));
        begin();
        focus({ ref(asBool), pause });
        focus({ ref(asReal), pause });
        report("target/on_preserves_the_kind_read_at_entry",
               "boolWritten=" + Kind(asBool->fields["manualDisable"])
               + " realWritten=" + Kind(asReal->fields["manualDisable"]));
    }

    // Out of combat the gate is already open: nothing is written.
    // red (pass-through body): test_on_gate_already_open_writes_nothing
    //   AssertionError: 0 != 1 : passed
    {
        FakeInstance* restart = button("PauseRestart", RValue(false));
        begin(restart);
        focus({ ref(restart), pause });
        report("target/on_gate_already_open_writes_nothing",
               "restartValue=" + Kind(restart->fields["manualDisable"]));
    }

    // A blind install (TABLE-ONLY) turns the mod off for the session, and a
    // later `restartanytime 1` cannot turn it back on to report ON and do
    // nothing.
    // red (pass-through body): not red - the pass-through never writes
    //   either; what this scenario pins is the header's own state
    //   (enabled=0 pending=0 after MarkBlind), which the body does not own.
    {
        mod.MarkBlind();
        mod.SetEnabled(true, true);
        FakeInstance* restart = button("PauseRestart", RValue(true));
        begin(restart);
        focus({ ref(restart), pause });
        report("target/blind_install_stays_off",
               std::string("enabled=") + (mod.IsEnabled() ? "1" : "0")
               + " pending=" + (mod.IsPending() ? "1" : "0")
               + " restartValue=" + Kind(restart->fields["manualDisable"]));
    }

    std::cout << "HARNESS DONE\n";
    return 0;
}
