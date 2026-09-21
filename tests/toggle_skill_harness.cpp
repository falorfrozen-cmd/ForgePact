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
#include <map>
#include <set>
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
// The six other candidate rows' objects exist so the research table's seed
// rows (`tgprobe tgl`, spliced below) compile against their real enumerators,
// and the three controller objects phase S's shipped table names (session 6
// rejected the predicted damage objects for those rows) so the shipped table
// compiles against theirs.
namespace HeroSiege { namespace Objects {
enum class GameObject { White_Mage_Soul_Spurn_AOE_obj, UI_Hud_Talent_obj, Universal_Double_Cast_obj,
                        Exo_Lunar_Orbit_obj, Plague_Doctor_Crematus_obj, Shield_Lancer_Counter_World_obj,
                        Butcher_Submerged_Knives_obj, Prophet_Maelstrom_obj, Butcher_Blender_obj,
                        Exo_Lunar_Orbit_Crescent_Moon_obj, Plague_Doctor_Crematus_Controller_obj,
                        Butcher_Submerged_Knives_Knifehoarder_obj,
                        // Session 8 (`tgprobe sweep`): the six parents the
                        // sweep enumerates, so its root table compiles
                        // against their real enumerators and resolves by name.
                        Player_Damage_Parent_obj, Skill_Controller_obj, Player_Buff_Parent_obj,
                        Player_Curse_Parent_obj, Player_Sentry_Parent_obj, Player_Ability_Parent_obj,
                        // Session 8's ship set: the two countdown rows whose
                        // objects no toggle-table row names, so the
                        // countdown's own table compiles against them.
                        White_Mage_Healing_Zone_obj, Samurai_Blade_Barrier_obj,
                        // Issue #55 follow-up (D-S4): two synthetic objects for
                        // the rule map's own stand-in generated table below -
                        // this harness does not carry the real 700+-entry
                        // hs-game-sdk table, the same reason it does not carry
                        // the real HeroSiege::Objects::GameObject enum.
                        Rule_Alpha_obj, Rule_Beta_obj };
inline const char* GetObjectName(GameObject g) {
    switch (g) {
    case GameObject::Rule_Alpha_obj: return "Rule_Alpha_obj";
    case GameObject::Rule_Beta_obj: return "Rule_Beta_obj";
    case GameObject::White_Mage_Healing_Zone_obj: return "White_Mage_Healing_Zone_obj";
    case GameObject::Samurai_Blade_Barrier_obj: return "Samurai_Blade_Barrier_obj";
    case GameObject::Player_Damage_Parent_obj: return "Player_Damage_Parent_obj";
    case GameObject::Skill_Controller_obj: return "Skill_Controller_obj";
    case GameObject::Player_Buff_Parent_obj: return "Player_Buff_Parent_obj";
    case GameObject::Player_Curse_Parent_obj: return "Player_Curse_Parent_obj";
    case GameObject::Player_Sentry_Parent_obj: return "Player_Sentry_Parent_obj";
    case GameObject::Player_Ability_Parent_obj: return "Player_Ability_Parent_obj";
    case GameObject::Universal_Double_Cast_obj: return "Universal_Double_Cast_obj";
    case GameObject::UI_Hud_Talent_obj: return "UI_Hud_Talent_obj";
    case GameObject::Exo_Lunar_Orbit_obj: return "Exo_Lunar_Orbit_obj";
    case GameObject::Plague_Doctor_Crematus_obj: return "Plague_Doctor_Crematus_obj";
    case GameObject::Shield_Lancer_Counter_World_obj: return "Shield_Lancer_Counter_World_obj";
    case GameObject::Butcher_Submerged_Knives_obj: return "Butcher_Submerged_Knives_obj";
    case GameObject::Prophet_Maelstrom_obj: return "Prophet_Maelstrom_obj";
    case GameObject::Butcher_Blender_obj: return "Butcher_Blender_obj";
    case GameObject::Exo_Lunar_Orbit_Crescent_Moon_obj: return "Exo_Lunar_Orbit_Crescent_Moon_obj";
    case GameObject::Plague_Doctor_Crematus_Controller_obj: return "Plague_Doctor_Crematus_Controller_obj";
    case GameObject::Butcher_Submerged_Knives_Knifehoarder_obj: return "Butcher_Submerged_Knives_Knifehoarder_obj";
    default: return "White_Mage_Soul_Spurn_AOE_obj";
    }
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
    // R (the research table's timer sampler): default VALUE_UNDEFINED, the
    // timer unreadable - never a number the sampler could mistake for one.
    RValue destroyTimer;
    bool destroyTimerThrows = false;
    // S (the shipped table): a row's discriminator names whatever field
    // session 6 measured for it, e.g. `crematus`'s `skillContamination`, so
    // an instance can carry any field by name rather than only the three the
    // Soul Spurn row happens to use. A name listed in `throws` throws instead.
    std::map<std::string, RValue> extra;
    std::set<std::string> throws;
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
static AoeInst WithTimer(AoeInst a, const RValue& timer) {
    a.destroyTimer = timer; return a;
}
// Session 8 (`tgprobe sweep`): an instance found under a parent carries its
// own object_index, answered as VALUE_REF - what this runner returns for
// object_index (see CInstance above) - so a sweep that only accepted plain
// numbers would find nothing here, exactly as it would live.
static RValue MakeRef(double n) { RValue r; r.m_Kind = VALUE_REF; r.number = n; return r; }
static AoeInst OfObject(AoeInst a, double objectIndex) {
    a.extra["object_index"] = MakeRef(objectIndex); return a;
}

// P2 (the shipped indicator): UI_Hud_Talent_obj instance 0's own `row0`
// array, the object ToggleIndicatorFindSlot walks (session 3's R5, "Slot
// geometry fields"). One element per hotbar slot; the element whose own
// talentId reads 240 (Soul Spurn) carries the rectangle.
struct HudRow0Elem {
    double talentId = 0, navBboxX = 0, navBboxY = 0, navBboxWidth = 0, navBboxHeight = 0;
};

// S (the shipped guard's sub-talent gate): `global.subTalentMap` as session 6
// measured it - an array whose index 1 holds one struct per talent, keyed
// `t<talentId>`, whose `s<NN>` keys are the sub-talent levels (`0.000000`
// with the key present when the sub-talent is respecced out, never absent).
// Every failure shape the hook must fail open on is a flag here, so each one
// gets its own scenario rather than being inferred from one of them.
struct SubTalentWorld {
    bool globalExists = true;
    bool isArray = true;
    bool getThrows = false;
    int length = 6;
    // index -> talent id -> slot number -> value. A missing talent id answers
    // `t<id>` undefined; a missing slot answers `s<NN>` undefined.
    std::map<int, std::map<int, std::map<int, RValue>>> levels;
    // One entry of the array being unusable is not the same as the whole read
    // failing: `entryNotStruct` makes that index answer a number instead of a
    // struct, and `entryThrows` makes reading `t<id>` off it throw. Both are
    // per index, because `getThrows` above throws for the whole global and so
    // cannot tell "this slot is junk" from "nothing is readable".
    std::set<int> entryNotStruct;
    std::set<int> entryThrows;
};

struct World {
    bool aoeObjectResolves = true;
    std::vector<AoeInst> instances;
    bool instanceNumberThrows = false;   // instance_number itself throws
    // S: a scenario that needs two table rows to see DIFFERENT instances gives
    // an object name its own index here and its own instance list below. A
    // name that is not listed keeps answering kAoeObjIdx and `instances`, so
    // every scenario written before the table is untouched.
    std::map<std::string, double> objIndexByName;
    std::map<double, std::vector<AoeInst>> instancesByIndex;
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
    SubTalentWorld sub;
    // Issue #55 (skilltimer): throwing stand-ins for the two draw calls that
    // are not already covered by drawRectangleThrows above, and whether
    // asset_get_index("__newfont6") resolves.
    bool drawRectangleColourThrows = false;
    bool drawLineThrows = false;
    bool fontResolves = true;
};
static World world;
static long g_DcResolveCalls = 0;      // asset_get_index("Universal_Double_Cast_obj") calls
static long g_InstanceEnumCalls = 0;   // instance_number/instance_find calls - guard_on/state_not_consulted
static long g_TrampCalls = 0;          // the TalentUseClass trampoline stand-in
static const double kDcObjIdx = 5318.0, kPlayerObjIdx = 7.0;   // what the stand-in runner answers
static long g_ResolveCalls = 0;    // HhResolveLocalPlayer calls - must stay 0 (read/no_player_lookup)
static long g_AnyCallCount = 0;    // every CallBuiltin call, of any name - indicator_off/no_runtime_calls
static long g_NamesCalls = 0;      // variable_instance_get_names calls - the `tgl fields` snapshot
// Session 8: every asset_get_index call by the name it asked for, so a
// scenario can tell WHICH objects a draw resolved - e.g. that a countdown row
// with no toggle twin never resolved a toggle-table row's object at all.
static std::map<std::string, long> g_AssetLookups;
static int g_RectangleDraws = 0;   // draw_rectangle calls this draw
static double g_LastSetColour = -1, g_LastSetAlpha = -1;
static const double kAoeObjIdx = 42.0, kHudObjIdx = 99.0;
static const double kPrevColour = 555.0, kPrevAlpha = 0.66;   // what draw_get_colour/draw_get_alpha answer
// S (D-U13's banded marker): what was actually drawn, not merely how many
// calls were made - the band rectangles' own arguments in order, the alpha set
// before each of them, and the colour triple the one make_colour_rgb call was
// given. A scenario that only counted calls could not tell a 10-band ramp in
// deepred from ten identical gold rectangles.
struct DrawnRect { double x1 = 0, y1 = 0, x2 = 0, y2 = 0; };
static std::vector<DrawnRect> g_DrawnRects;
static std::vector<double> g_SetAlphas;
static double g_ColourR = -1, g_ColourG = -1, g_ColourB = -1;
static long g_MakeColourCalls = 0;
static long g_SlotLookups = 0;     // ToggleIndicatorFindSlot reaching the row0 array
static void resetDrawRecord() {
    g_DrawnRects.clear(); g_SetAlphas.clear();
    g_ColourR = g_ColourG = g_ColourB = -1; g_MakeColourCalls = 0; g_SlotLookups = 0;
    g_RectangleDraws = 0;
}

// Issue #55 (skilltimer): the two draw calls no earlier feature used -
// draw_rectangle_colour (`bar`) and draw_line (`arc`) - and draw_text
// (`number`, already reachable through HhDrawHeadLabels, but not yet
// recorded by this harness), each recording ARGUMENTS, not just a count, the
// same discipline g_DrawnRects/g_SetAlphas already apply to draw_rectangle.
struct ColourRectDraw { double x0 = 0, y0 = 0, x1 = 0, y1 = 0, colour = 0; };
struct LineDraw { double x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
// `valign` is the vertical alignment in force when the text was drawn (the
// last draw_set_valign before it), so a scenario can assert the anchor's
// meaning, not only its coordinates.
struct TextDraw { double x = 0, y = 0; std::string text; double valign = -1; };
static std::vector<ColourRectDraw> g_ColourRectDraws;
static std::vector<LineDraw> g_LineDraws;
static std::vector<TextDraw> g_TextDraws;
// The font/halign/valign get/set pair, the same shape g_LastSetColour/
// g_LastSetAlpha already give draw_get_colour/draw_get_alpha: draw_get_*
// answers a fixed "whatever the game had before" sentinel, draw_set_*
// records the last value written, so a scenario can assert BOTH that the
// look applied its own value and that it was restored afterwards.
static const double kPrevFont = 222.0, kPrevHalign = 9.0, kPrevValign = 8.0;
static const double kFontIdx = 777.0;   // what asset_get_index("__newfont6") answers when it resolves
static double g_LastSetFont = -1, g_LastSetHalign = -1, g_LastSetValign = -1;
static void resetSkillTimerDrawRecord() {
    g_ColourRectDraws.clear(); g_LineDraws.clear(); g_TextDraws.clear();
    g_LastSetFont = g_LastSetHalign = g_LastSetValign = -1;
}

// S: the instance list behind an object index. Every index a scenario did not
// give its own list keeps answering `world.instances`, which is what every
// scenario written before the shipped table drives.
static std::vector<AoeInst>& instancesFor(double objIdx) {
    auto it = world.instancesByIndex.find(objIdx);
    return it == world.instancesByIndex.end() ? world.instances : it->second;
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++g_AnyCallCount;
        const std::string fn = name;
        if (fn == "asset_get_index") {
            const std::string want = args[0].ToString();
            ++g_AssetLookups[want];
            if (want == "UI_Hud_Talent_obj") return RValue(world.hudTalentObjectResolves ? kHudObjIdx : -1.0);
            if (want == "Universal_Double_Cast_obj") {
                ++g_DcResolveCalls;
                return RValue(world.doubleCastObjectResolves ? kDcObjIdx : -1.0);
            }
            // Issue #55 (skilltimer's `number` look): resolved by name every
            // draw, never a hard-coded index.
            if (want == "__newfont6") return RValue(world.fontResolves ? kFontIdx : -1.0);
            auto named = world.objIndexByName.find(want);
            if (named != world.objIndexByName.end())
                return RValue(world.aoeObjectResolves ? named->second : -1.0);
            return RValue(world.aoeObjectResolves ? kAoeObjIdx : -1.0);
        }
        if (fn == "instance_number") {
            ++g_InstanceEnumCalls;
            if (world.instanceNumberThrows) throw std::runtime_error("instance_number EXCEPTION");
            return RValue((double)instancesFor(args[0].ToDouble()).size());
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
            if (i < 0 || (size_t)i >= instancesFor(obj).size()) return RValue();   // VALUE_UNDEFINED
            RValue r; r.m_Kind = VALUE_REF;
            r.text = "aoe:" + std::to_string((long long)obj) + ":" + std::to_string(i);
            r.number = obj;
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
                ++g_SlotLookups;
                RValue r; r.m_Kind = VALUE_ARRAY; r.text = "row0";
                return r;
            }
            if (tag.rfind("aoe:", 0) != 0) return RValue();
            const size_t sep = tag.find(':', 4);
            const double obj = std::stod(tag.substr(4, sep - 4));
            const size_t i = (size_t)std::stoi(tag.substr(sep + 1));
            std::vector<AoeInst>& list = instancesFor(obj);
            if (i >= list.size()) return RValue();
            const AoeInst& a = list[i];
            if (a.throws.count(field)) throw std::runtime_error("field EXCEPTION");
            if (field == "isMyClient") {
                if (a.isMyClientThrows) throw std::runtime_error("isMyClient EXCEPTION");
                return a.isMyClient;
            }
            if (field == "purgatory") {
                if (a.purgatoryThrows) throw std::runtime_error("purgatory EXCEPTION");
                return a.purgatory;
            }
            if (field == "destroyTimer") {
                if (a.destroyTimerThrows) throw std::runtime_error("destroyTimer EXCEPTION");
                return a.destroyTimer;
            }
            auto extra = a.extra.find(field);
            if (extra != a.extra.end()) return extra->second;
            return RValue();
        }
        // S (the guard's sub-talent gate): global.subTalentMap, read the way
        // the hook reads it - exists, get, array_get at the measured index,
        // then `t<id>` and `s<NN>`.
        if (fn == "variable_global_exists") {
            return MakeBool(args[0].ToString() == "subTalentMap" && world.sub.globalExists);
        }
        if (fn == "variable_global_get") {
            if (args[0].ToString() != "subTalentMap") return RValue();
            if (world.sub.getThrows) throw std::runtime_error("subTalentMap EXCEPTION");
            if (!world.sub.isArray) return MakeReal(7.0);   // a number, not an array
            RValue r; r.m_Kind = VALUE_ARRAY; r.text = "subTalentMap";
            return r;
        }
        // R (`tgl fields`): an AOE instance's member names, tagged with the
        // instance they came from, so a snapshot names which one it read.
        if (fn == "variable_instance_get_names") {
            ++g_NamesCalls;
            RValue r; r.m_Kind = VALUE_ARRAY; r.text = "names:" + args[0].text;
            return r;
        }
        if (fn == "array_length") {
            if (args[0].text == "row0") return RValue((double)world.row0.size());
            if (args[0].text == "subTalentMap") return RValue((double)world.sub.length);
            if (args[0].text.rfind("names:", 0) == 0) return RValue(2.0);
            return RValue(0.0);
        }
        if (fn == "array_get") {
            if (args[0].text.rfind("names:", 0) == 0) {
                const int i = (int)args[1].ToDouble();
                return i == 0 ? RValue("isMyClient") : (i == 1 ? RValue("purgatory") : RValue());
            }
            if (args[0].text == "row0") {
                const int i = (int)args[1].ToDouble();
                if (i < 0 || (size_t)i >= world.row0.size()) return RValue();
                RValue r; r.m_Kind = VALUE_OBJECT; r.text = "row0elem:" + std::to_string(i);
                return r;
            }
            if (args[0].text == "subTalentMap") {
                const int i = (int)args[1].ToDouble();
                if (i < 0 || i >= world.sub.length) return RValue();   // out of range
                if (world.sub.entryNotStruct.count(i)) return MakeReal(11.0);   // a number, not a struct
                RValue r; r.m_Kind = VALUE_OBJECT; r.text = "submap:" + std::to_string(i);
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
            if (tag.rfind("submap:", 0) == 0) {
                const int index = std::stoi(tag.substr(7));
                if (world.sub.entryThrows.count(index)) throw std::runtime_error("subTalentMap entry EXCEPTION");
                if (field.size() < 2 || field[0] != 't') return RValue();
                const int talent = std::stoi(field.substr(1));
                auto byIndex = world.sub.levels.find(index);
                if (byIndex == world.sub.levels.end()) return RValue();
                auto byTalent = byIndex->second.find(talent);
                if (byTalent == byIndex->second.end()) return RValue();   // `t<id>` absent
                RValue r; r.m_Kind = VALUE_OBJECT;
                r.text = "subt:" + std::to_string(index) + ":" + std::to_string(talent);
                return r;
            }
            if (tag.rfind("subt:", 0) == 0) {
                const size_t sep = tag.find(':', 5);
                const int index = std::stoi(tag.substr(5, sep - 5));
                const int talent = std::stoi(tag.substr(sep + 1));
                if (field.size() < 2 || field[0] != 's') return RValue();
                const int slot = std::stoi(field.substr(1));
                auto byIndex = world.sub.levels.find(index);
                if (byIndex == world.sub.levels.end()) return RValue();
                auto byTalent = byIndex->second.find(talent);
                if (byTalent == byIndex->second.end()) return RValue();
                auto value = byTalent->second.find(slot);
                if (value == byTalent->second.end()) return RValue();   // `s<NN>` absent
                return value->second;
            }
            return RValue();
        }
        if (fn == "draw_get_colour") return RValue(kPrevColour);
        if (fn == "draw_get_alpha") return RValue(kPrevAlpha);
        if (fn == "make_colour_rgb") {
            ++g_MakeColourCalls;
            g_ColourR = args[0].ToDouble(); g_ColourG = args[1].ToDouble(); g_ColourB = args[2].ToDouble();
            return RValue(123456.0);
        }
        if (fn == "draw_set_colour") { g_LastSetColour = args[0].ToDouble(); return RValue(); }
        if (fn == "draw_set_alpha") {
            g_LastSetAlpha = args[0].ToDouble(); g_SetAlphas.push_back(g_LastSetAlpha); return RValue();
        }
        if (fn == "draw_rectangle") {
            if (world.drawRectangleThrows) throw std::runtime_error("draw_rectangle EXCEPTION");
            ++g_RectangleDraws;
            g_DrawnRects.push_back({ args[0].ToDouble(), args[1].ToDouble(),
                                     args[2].ToDouble(), args[3].ToDouble() });
            return RValue();
        }
        // Issue #55 (skilltimer): `bar`'s filled rectangle, `arc`'s perimeter
        // segments, and `number`'s text and font/alignment state.
        if (fn == "draw_rectangle_colour") {
            if (world.drawRectangleColourThrows) throw std::runtime_error("draw_rectangle_colour EXCEPTION");
            g_ColourRectDraws.push_back({ args[0].ToDouble(), args[1].ToDouble(),
                                          args[2].ToDouble(), args[3].ToDouble(), args[4].ToDouble() });
            return RValue();
        }
        if (fn == "draw_line") {
            if (world.drawLineThrows) throw std::runtime_error("draw_line EXCEPTION");
            g_LineDraws.push_back({ args[0].ToDouble(), args[1].ToDouble(),
                                    args[2].ToDouble(), args[3].ToDouble() });
            return RValue();
        }
        if (fn == "draw_text") {
            g_TextDraws.push_back({ args[0].ToDouble(), args[1].ToDouble(), args[2].ToString(), g_LastSetValign });
            return RValue();
        }
        if (fn == "draw_get_font") return RValue(kPrevFont);
        if (fn == "draw_set_font") { g_LastSetFont = args[0].ToDouble(); return RValue(); }
        if (fn == "draw_get_halign") return RValue(kPrevHalign);
        if (fn == "draw_set_halign") { g_LastSetHalign = args[0].ToDouble(); return RValue(); }
        if (fn == "draw_get_valign") return RValue(kPrevValign);
        if (fn == "draw_set_valign") { g_LastSetValign = args[0].ToDouble(); return RValue(); }
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

// R (`tgprobe tgl`'s sampler): the frame counter and room key it reads. The
// room key stands in for a builtin read, so it counts as a call.
static uint64_t g_RuntimeFrame = 0;
static int64_t CurrentRoomKey() { ++g_AnyCallCount; return 1; }
// Describe()'s shape for the scalars the snapshot keeps, without the whole
// describer.
static std::string TgProbeDescribeShort(const RValue& v, size_t cap = 80) {
    (void)cap;
    if (v.m_Kind == VALUE_BOOL) return v.boolean ? "true" : "false";
    if (v.m_Kind == VALUE_STRING) return v.text;
    return std::to_string(v.number);
}

// ---- what the injected production code leans on ---------------------------
// PRODUCTION_CONSTANTS

// PRODUCTION_TOGGLESKILL

// PRODUCTION_SKILLTIMER

// Issue #55 follow-up (D-S4): a small stand-in for the generated
// SkillTimerNames.hpp table - this harness cannot carry the real 700+-entry
// hs-game-sdk table (its own HeroSiege::Objects::GameObject enum above is
// itself a small stand-in), so it hand-writes the two entries the rule/*
// scenarios below need. tools/gen_skill_timer_names.py and
// SkillTimerRuleContractTests (test_toggle_skill_contract.py) are what pin
// the REAL generated header against hs-game-sdk; this table is test data,
// never spliced from production. Deliberately carries no "arrowturret"-style
// companion key, so rule/companion_never_enters_the_table can assert the
// same structural absence the real generator's companion exclusion produces.
namespace ForgePact {
inline constexpr SkillTimerNameEntry kSkillTimerNames[] = {
    { "rulealpha", HeroSiege::Objects::GameObject::Rule_Alpha_obj },
    { "rulebeta", HeroSiege::Objects::GameObject::Rule_Beta_obj },
};
inline constexpr int kSkillTimerNameCount =
    (int)(sizeof(kSkillTimerNames) / sizeof(kSkillTimerNames[0]));
}
static constexpr int kRuleAlphaIndex = 0, kRuleBetaIndex = 1;

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

// S (the per-row border counters): zeroed field by field rather than by
// assigning a fresh struct, because every member is volatile.
static void resetTibRow(int r) {
    g_TibRow[r].drawn = 0; g_TibRow[r].on = 0; g_TibRow[r].off = 0;
    g_TibRow[r].unreadable = 0; g_TibRow[r].unresolved = 0; g_TibRow[r].noSlot = 0;
}

// Issue #55 (skilltimer): the countdown's own per-row counters, zeroed field
// by field for the same reason as resetTibRow above, plus its latch state
// (a plain struct, no volatile members, so a fresh SkillTimerRowState{} is
// fine there) and its aggregate counters.
static void resetStRow(int r) {
    g_StRow[r].drawn = 0; g_StRow[r].noInstance = 0; g_StRow[r].unreadable = 0;
    g_StRow[r].expired = 0; g_StRow[r].toggleOn = 0; g_StRow[r].toggleUnreadable = 0;
    g_StRow[r].unresolved = 0; g_StRow[r].noSlot = 0; g_StRow[r].latched = 0; g_StRow[r].unlatched = 0;
}
static void resetSkillTimer() {
    for (int r = 0; r < ForgePact::kSkillTimerRowCount; ++r) {
        g_SkillTimerRowState[r] = ForgePact::SkillTimerRowState{};
        resetStRow(r);
    }
    g_StDrawExc = 0; g_StFontUnresolved = 0;
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Off);
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
// S: the guard's baseline world is "row 0 resolved, its toggle sub-talent
// allocated" - the shape session 6 measured for an actual Purgatory toggle,
// and the only shape in which a refusal is correct at all (D-P3). A scenario
// that is about the gate changes exactly the one thing it is about; every
// scenario written for T1 keeps its own expected values because this baseline
// reproduces T1's unconditional refusal.
static void resetGuardSubTalent(double level) {
    world.sub.levels[ForgePact::kToggleSubTalentMapIndex][kToggleIndicatorTalentId]
                    [ForgePact::kToggleSkillRows[0].subTalentSlot] = MakeReal(level);
}
static void resetGuard(bool enabled) {
    world = World{};
    g_OrigTalentUseClass = &FakeTalentUseClassOriginal;
    ForgePact::ToggleGuardMod::Instance().SetEnabled(enabled, /*alreadyHooked=*/true);
    g_TgdRefused = 0; g_TgdPassed = 0; g_TgdProcSeen = 0; g_TgdSelfUnreadable = 0; g_TgdObjUnresolved = 0;
    g_TgdSubOff = 0; g_TgdSubUnreadable = 0;
    g_TgdSubIndex.store(-1);
    g_ToggleGuardDcObjIdx.store(-1);
    for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
    g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
    resetGuardSubTalent(3.0);   // session 6 read `s12=real:3.000000` when allocated
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

    // S: the shipped table's talent ids are resolved at runtime from each
    //    row's `abilityId`; the harness sets them directly instead. Every
    //    `indicator_*` scenario below drives ROW 0 alone - the other four rows
    //    stay unresolved, exactly as a live session looks before their
    //    `abilityId` has been matched - so each counter keeps the single-row
    //    expected value T1 measured.
    for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
    g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);

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

    // 20. ON, own+marked, slot found: marks the slot exactly once (one
    //     kToggleMarkerBands-band pass, D-U13 - three gold passes before it).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        g_TibDrawn = 0; g_RectangleDraws = 0;
        ToggleIndicatorDraw();
        checkInt("indicator_on/own_on_outlines_slot", g_TibDrawn, 1);
        checkInt("indicator_on/own_on_outlines_slot/rectangles", g_RectangleDraws, kToggleMarkerBands);
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
    //     Counting is not isolation, though: the marker's colour and alpha
    //     are set before the throw, so they must STILL be restored, or every
    //     HUD draw after this one inherits the marker's deep red at the last
    //     band's alpha (re-review finding, P2).
    resetWorld();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.drawRectangleThrows = true;
    {
        g_TibDrawn = 0; g_TibDrawExc = 0;
        g_LastSetColour = -999; g_LastSetAlpha = -999;
        ToggleIndicatorDraw();
        checkInt("indicator_on/draw_exception_counts/drawn", g_TibDrawn, 0);
        checkInt("indicator_on/draw_exception_counts", g_TibDrawExc, 1);
        checkNear("indicator_on/draw_exception_counts/colour_restored", g_LastSetColour, kPrevColour);
        checkNear("indicator_on/draw_exception_counts/alpha_restored", g_LastSetAlpha, kPrevAlpha);
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

    // ---- S: the D-U13 marker, the D-U12 box and the per-row discriminators --
    // What was drawn, not how many calls were made: a scenario that only
    // counted draw_rectangle calls could not tell a deepred alpha ramp from
    // ten identical gold rectangles.

    // 30. The colour is D-U13's `deepred`, from exactly one make_colour_rgb
    //     call - the same triple the sprite probe's own preset carries, which
    //     a contract test pins the two sides of.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        ToggleIndicatorDraw();
        checkNear("border/marker_colour_is_deepred/r", g_ColourR, 140.0);
        checkNear("border/marker_colour_is_deepred/g", g_ColourG, 24.0);
        checkNear("border/marker_colour_is_deepred/b", g_ColourB, 28.0);
        checkInt("border/marker_colour_is_deepred", g_MakeColourCalls, 1);
    }

    // 31. The bands' alpha falls strictly outwards, full at the innermost and
    //     zero at the outermost, and the pre-draw alpha is restored after the
    //     last band.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        ToggleIndicatorDraw();
        bool decreasing = g_SetAlphas.size() == (size_t)kToggleMarkerBands + 1;
        for (size_t i = 1; decreasing && i + 1 < g_SetAlphas.size(); ++i) {
            if (!(g_SetAlphas[i] < g_SetAlphas[i - 1])) decreasing = false;
        }
        checkBool("border/marker_alpha_ramps_outwards/strictly_decreasing", decreasing, true);
        checkNear("border/marker_alpha_ramps_outwards/innermost",
                  g_SetAlphas.empty() ? -1.0 : g_SetAlphas.front(), 1.0);
        checkNear("border/marker_alpha_ramps_outwards/outermost",
                  g_SetAlphas.size() >= (size_t)kToggleMarkerBands ? g_SetAlphas[kToggleMarkerBands - 1] : -1.0, 0.0);
        checkNear("border/marker_alpha_ramps_outwards", g_SetAlphas.empty() ? -1.0 : g_SetAlphas.back(), kPrevAlpha);
    }

    // 32. D-U12's worked example, end to end: the slot's own live navBbox
    //     `385.700006, 1711.000000, 124.700000 x 139.200000` becomes the
    //     accepted whole-pixel box `120 x 126 @388,1711`, so the innermost
    //     band's rectangle is exactly 388, 1711, 508, 1837. Nothing hardcodes
    //     those numbers in the plugin - they come out of the offset.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    world.row0 = { { (double)kToggleIndicatorTalentId, 385.700006, 1711.000000, 124.700000, 139.200000 } };
    {
        ToggleIndicatorDraw();
        const DrawnRect first = g_DrawnRects.empty() ? DrawnRect{} : g_DrawnRects.front();
        checkNear("border/marker_box_is_derived_and_whole_pixel/x1", first.x1, 388.0);
        checkNear("border/marker_box_is_derived_and_whole_pixel/y1", first.y1, 1711.0);
        checkNear("border/marker_box_is_derived_and_whole_pixel/x2", first.x2, 508.0);
        checkNear("border/marker_box_is_derived_and_whole_pixel/y2", first.y2, 1837.0);
        checkInt("border/marker_box_is_derived_and_whole_pixel", (long)g_DrawnRects.size(), kToggleMarkerBands);
    }

    // 33. Soul Spurn's own decision is unchanged by the generalisation: own +
    //     purgatory 0.09 is still exactly one marker.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        g_TibDrawn = 0;
        ToggleIndicatorDraw();
        checkInt("border/soul_spurn_marker_unchanged_semantics/drawn", g_TibDrawn, 1);
        checkInt("border/soul_spurn_marker_unchanged_semantics", (long)g_DrawnRects.size(), kToggleMarkerBands);
    }

    // 34. Two ON rows with different slots: two slot lookups and two markers.
    //     Row 1 (`lunarOrbit`) has no ownership field and no discriminator,
    //     so its controller instance lights it on its own.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    {
        const int kLunarId = 358;   // session 6's measured id, harness-side only
        g_ToggleTableIds.Set(1, kLunarId);
        world.objIndexByName[HeroSiege::Objects::GetObjectName(ForgePact::kToggleSkillRows[1].onObject)] = 500.0;
        world.instancesByIndex[500.0] = { Unattributed() };
        world.instances = { OwnMarked(0.09) };
        world.row0 = { { (double)kToggleIndicatorTalentId, 100.0, 200.0, 50.0, 60.0 },
                       { (double)kLunarId, 300.0, 400.0, 50.0, 60.0 } };
        g_TibDrawn = 0;
        ToggleIndicatorDraw();
        checkInt("border/two_rows_two_borders/drawn", g_TibDrawn, 2);
        checkInt("border/two_rows_two_borders/slot_lookups", g_SlotLookups, 2);
        checkInt("border/two_rows_two_borders", (long)g_DrawnRects.size(), 2 * kToggleMarkerBands);
        g_ToggleTableIds.Set(1, -1);
    }

    // 35. A `none-needed` row (D-P5): no discriminator is required, so any own
    //     instance lights it, and an instance whose ownership read would have
    //     thrown still counts as own because the row names no ownership field
    //     at all (D-N3) - the read never asks.
    {
        const int kLunarId = 358;
        auto lunarRow = [&](AoeInst inst) {
            resetWorld(); resetDrawRecord();
            g_ToggleBorderOn.store(true);
            for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
            g_ToggleTableIds.Set(1, kLunarId);
            world.objIndexByName[HeroSiege::Objects::GetObjectName(ForgePact::kToggleSkillRows[1].onObject)] = 500.0;
            world.instancesByIndex[500.0] = { inst };
            world.row0 = { { (double)kLunarId, 100.0, 200.0, 50.0, 60.0 } };
            g_TibDrawn = 0;
            ToggleIndicatorDraw();
        };
        checkBool("border/no_discriminator_row_lights_on_any_own/requireMarker",
                  ForgePact::ToggleRowRequiresMark(ForgePact::kToggleSkillRows[1]), false);
        lunarRow(OwnUnmarked());   // a readable-but-zero marker field is irrelevant here
        checkInt("border/no_discriminator_row_lights_on_any_own", g_TibDrawn, 1);

        AoeInst throwing;
        throwing.isMyClient = RValue();
        throwing.isMyClientThrows = true;
        lunarRow(throwing);
        checkInt("border/ownership_none_counts_every_instance_own", g_TibDrawn, 1);
    }

    // 35b. Per-row counters (phase S review follow-up). The aggregate line
    //      sums all five rows, so a player with nothing toggled reads `off=`
    //      at five times the draw count, and neither "which row was ON" nor
    //      "which row lost its slot" can be answered from it. One draw with
    //      row 0 ON, row 1 resolved but absent and the rest unresolved must
    //      land in three different rows' counters, and each row's own line
    //      must name the row by its `abilityId`.
    {
        const int kLunarId = 358;
        resetWorld(); resetDrawRecord();
        g_ToggleBorderOn.store(true);
        for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) { g_ToggleTableIds.Set(r, -1); resetTibRow(r); }
        g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
        g_ToggleTableIds.Set(1, kLunarId);
        world.objIndexByName[HeroSiege::Objects::GetObjectName(ForgePact::kToggleSkillRows[1].onObject)] = 500.0;
        world.instancesByIndex[500.0] = {};            // row 1 resolves, nothing live: Off
        world.instances = { OwnMarked(0.09) };         // row 0: ON
        world.row0 = { { (double)kToggleIndicatorTalentId, 100.0, 200.0, 50.0, 60.0 } };
        ToggleIndicatorDraw();
        checkInt("border/per_row_counters_name_the_row/row0_on", g_TibRow[0].on, 1);
        checkInt("border/per_row_counters_name_the_row/row0_drawn", g_TibRow[0].drawn, 1);
        checkInt("border/per_row_counters_name_the_row/row0_off", g_TibRow[0].off, 0);
        checkInt("border/per_row_counters_name_the_row/row1_off", g_TibRow[1].off, 1);
        checkInt("border/per_row_counters_name_the_row/row1_on", g_TibRow[1].on, 0);
        checkInt("border/per_row_counters_name_the_row/row2_unresolved", g_TibRow[2].unresolved, 1);
        const std::string row0Line = ToggleBorderRowCountersLine(0);
        const std::string row1Line = ToggleBorderRowCountersLine(1);
        checkBool("border/per_row_counters_name_the_row/row0_named",
                  row0Line.find(ForgePact::kToggleSkillRows[0].abilityId) != std::string::npos
                  && row0Line.find("on=1") != std::string::npos, true);
        checkBool("border/per_row_counters_name_the_row/row1_named",
                  row1Line.find(ForgePact::kToggleSkillRows[1].abilityId) != std::string::npos
                  && row1Line.find("off=1") != std::string::npos, true);

        // The slot failure lands on the row that failed it, not in a shared
        // total - the other half of what the aggregate line cannot say.
        resetWorld(); resetDrawRecord();
        g_ToggleBorderOn.store(true);
        for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) { g_ToggleTableIds.Set(r, -1); resetTibRow(r); }
        g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
        world.instances = { OwnMarked(0.09) };
        world.row0 = { { 9999.0, 100.0, 200.0, 50.0, 60.0 } };   // no slot carries row 0's talent
        ToggleIndicatorDraw();
        checkInt("border/per_row_counters_name_the_row/row0_noSlot", g_TibRow[0].noSlot, 1);
        checkInt("border/per_row_counters_name_the_row", g_TibRow[0].drawn, 0);
        for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
        g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
    }

    // 36. The timer discriminator (`maelstromOfFrost`): EXACT equality with
    //     the value session 6 measured the row held at while on. A counting
    //     timer, any other negative, and an unreadable read each draw nothing
    //     - the last of them counted rather than guessed either way.
    {
        const int kMaelstromId = 430;   // session 6's measured id, harness-side only
        const int kRow = ForgePact::kToggleSkillRowCount - 1;
        auto timerRow = [&](const RValue& timer, bool throwsRead) {
            resetWorld(); resetDrawRecord();
            g_ToggleBorderOn.store(true);
            for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
            g_ToggleTableIds.Set(kRow, kMaelstromId);
            world.objIndexByName[HeroSiege::Objects::GetObjectName(ForgePact::kToggleSkillRows[kRow].onObject)] = 600.0;
            AoeInst inst = OwnMarked(0.0);   // own; this row's discriminator is the timer
            inst.destroyTimer = timer;
            inst.destroyTimerThrows = throwsRead;
            world.instancesByIndex[600.0] = { inst };
            world.row0 = { { (double)kMaelstromId, 100.0, 200.0, 50.0, 60.0 } };
            g_TibDrawn = 0; g_TibOff = 0; g_TibUnreadable = 0;
            ToggleIndicatorDraw();
        };
        timerRow(MakeReal(-1.0), false);
        checkInt("border/timer_at_infinite_draws_full", g_TibDrawn, 1);
        timerRow(MakeReal(57.0), false);
        checkInt("border/timer_counting_down_draws_nothing/off", g_TibOff, 1);
        checkInt("border/timer_counting_down_draws_nothing", g_TibDrawn, 0);
        timerRow(MakeReal(-0.737424), false);
        checkInt("border/timer_other_negative_draws_nothing", g_TibDrawn, 0);
        timerRow(RValue(), false);   // undefined: never defaulted to a number
        checkInt("border/timer_unreadable_draws_nothing_and_counts/undefined", g_TibUnreadable, 1);
        timerRow(MakeReal(-1.0), true);   // a throwing read, with a value that WOULD have lit it
        checkInt("border/timer_unreadable_draws_nothing_and_counts/throws", g_TibUnreadable, 1);
        checkInt("border/timer_unreadable_draws_nothing_and_counts", g_TibDrawn, 0);
    }

    // 37. A row whose talent id has not been resolved from its `abilityId` yet
    //     is skipped and counted - never read, never drawn, never guessed.
    resetWorld(); resetDrawRecord();
    g_ToggleBorderOn.store(true);
    world.instances = { OwnMarked(0.09) };
    {
        for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
        g_TibDrawn = 0; g_TibUnresolved = 0;
        const long enumBefore = g_InstanceEnumCalls;
        ToggleIndicatorDraw();
        checkInt("table/unresolved_row_skipped_and_counted/drawn", g_TibDrawn, 0);
        checkInt("table/unresolved_row_skipped_and_counted/no_enumeration", g_InstanceEnumCalls - enumBefore, 0);
        checkInt("table/unresolved_row_skipped_and_counted", g_TibUnresolved, ForgePact::kToggleSkillRowCount);
        g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
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

    // ---- S: the guard's sub-talent gate (D-P3) -------------------------------
    // The refusal costs the player a cast, so it happens only when the toggle
    // sub-talent is actually allocated - read at the call, with the talent the
    // call named. Everything else passes and is counted, because fail-open is
    // vanilla behaviour and a mod that silently eats casts is worse than one
    // that occasionally does nothing.

    // 40a. Allocated: the proc re-cast is refused, as T1 always did.
    resetGuard(true);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/table_talent_with_subtalent_refused/refused", g_TgdRefused, 1);
        checkInt("guard_on/table_talent_with_subtalent_refused/subOff", g_TgdSubOff, 0);
        checkInt("guard_on/table_talent_with_subtalent_refused", c.tramp, 0);
    }

    // 40b. Respecced out: session 6 measured `s12=real:0.000000`, key present.
    //      The call is the player's own plain cast, so it goes through.
    resetGuard(true);
    resetGuardSubTalent(0.0);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/table_talent_without_subtalent_passes/subOff", g_TgdSubOff, 1);
        checkInt("guard_on/table_talent_without_subtalent_passes/refused", g_TgdRefused, 0);
        checkInt("guard_on/table_talent_without_subtalent_passes", c.tramp, 1);
    }

    // 40c. Every shape the read can fail in, one scenario each, so none of
    //      them is inferred from another: the global absent, the global not an
    //      array, an array too short to hold the measured index (and no other
    //      index carrying the talent either), `t<id>` absent everywhere,
    //      `s<NN>` non-numeric, and a throw. Each passes and counts once.
    {
        struct SubShape { const char* name; void (*apply)(); };
        static const SubShape kShapes[] = {
            { "global_absent", []() { world.sub.globalExists = false; } },
            { "not_an_array",  []() { world.sub.isArray = false; } },
            { "index_out_of_range", []() { world.sub.length = ForgePact::kToggleSubTalentMapIndex; } },
            { "talent_key_absent", []() { world.sub.levels.clear(); } },
            { "slot_non_numeric", []() {
                  world.sub.levels[ForgePact::kToggleSubTalentMapIndex][kToggleIndicatorTalentId]
                                  [ForgePact::kToggleSkillRows[0].subTalentSlot] = RValue("3"); } },
            { "read_throws", []() { world.sub.getThrows = true; } },
        };
        for (const SubShape& s : kShapes) {
            resetGuard(true);
            s.apply();
            GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
            const std::string label = std::string("guard_on/subtalent_unreadable_passes_and_counts/") + s.name;
            checkInt(label + "/subUnreadable", g_TgdSubUnreadable, 1);
            checkInt(label + "/refused", g_TgdRefused, 0);
            checkInt(label, c.tramp, 1);
        }
        checkBool("guard_on/subtalent_unreadable_passes_and_counts",
                  sizeof(kShapes) / sizeof(kShapes[0]) == 6, true);
    }

    // 40d. The measured index is a starting point, not an assumption: when
    //      `t<talentId>` lives at some other array index, the read finds it
    //      there and the gate still works. Session 6 measured index 1 on one
    //      character on one build; if that index is a character or player slot
    //      elsewhere, a fixed index would leave the guard silently inert
    //      (phase S review follow-up).
    resetGuard(true);
    world.sub.levels.clear();
    world.sub.levels[3][kToggleIndicatorTalentId][ForgePact::kToggleSkillRows[0].subTalentSlot] = MakeReal(3.0);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/subtalent_other_index_answers/refused", g_TgdRefused, 1);
        checkInt("guard_on/subtalent_other_index_answers/subUnreadable", g_TgdSubUnreadable, 0);
        checkInt("guard_on/subtalent_other_index_answers/index", g_TgdSubIndex.load(), 3);
        checkInt("guard_on/subtalent_other_index_answers", c.tramp, 0);
    }

    // 40e. The same, unallocated: the index that answers is the one whose
    //      `t<id>` struct exists, and its `s<NN>` decides - so a respecced-out
    //      sub-talent at another index still passes and counts subOff, rather
    //      than being reported as unreadable.
    resetGuard(true);
    world.sub.levels.clear();
    world.sub.levels[4][kToggleIndicatorTalentId][ForgePact::kToggleSkillRows[0].subTalentSlot] = MakeReal(0.0);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/subtalent_measured_index_struct_absent_falls_back/subOff", g_TgdSubOff, 1);
        checkInt("guard_on/subtalent_measured_index_struct_absent_falls_back/refused", g_TgdRefused, 0);
        checkInt("guard_on/subtalent_measured_index_struct_absent_falls_back/index", g_TgdSubIndex.load(), 4);
        checkInt("guard_on/subtalent_measured_index_struct_absent_falls_back", c.tramp, 1);
    }

    // 40f. No index carries the talent's struct at all: the read is unreadable,
    //      the call passes and is counted, and the stat line says plainly that
    //      no index answered rather than naming one that did not.
    resetGuard(true);
    world.sub.levels.clear();
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/subtalent_no_index_answers/subUnreadable", g_TgdSubUnreadable, 1);
        checkInt("guard_on/subtalent_no_index_answers/refused", g_TgdRefused, 0);
        checkInt("guard_on/subtalent_no_index_answers/index", g_TgdSubIndex.load(), -1);
        checkInt("guard_on/subtalent_no_index_answers", c.tramp, 1);
    }

    // 40g. One unusable entry costs ONE index, not the scan. A junk slot
    //      earlier in the array - a number where a struct should be, or an
    //      entry whose read throws - must not end the walk before the index
    //      that really carries `t<talentId>`, or the guard is inert again with
    //      `subIndex=none` and a climbing `subUnreadable=` as the only symptom
    //      (the shape the index fix exists to remove). `read_throws` above
    //      throws for the whole global and cannot tell these two apart.
    resetGuard(true);
    world.sub.levels.clear();
    world.sub.entryNotStruct.insert(0);
    world.sub.entryThrows.insert(2);
    world.sub.levels[5][kToggleIndicatorTalentId][ForgePact::kToggleSkillRows[0].subTalentSlot] = MakeReal(3.0);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/subtalent_bad_entry_costs_one_index/refused", g_TgdRefused, 1);
        checkInt("guard_on/subtalent_bad_entry_costs_one_index/subUnreadable", g_TgdSubUnreadable, 0);
        checkInt("guard_on/subtalent_bad_entry_costs_one_index/index", g_TgdSubIndex.load(), 5);
        checkInt("guard_on/subtalent_bad_entry_costs_one_index", c.tramp, 0);
    }

    // 40h. A talent that is not in the shipped table is never a member, so the
    //      sub-talent is not even read.
    resetGuard(true);
    {
        const long before = g_AnyCallCount;
        GuardCall c = CallGuard(&dcSelf, 999.0, false);
        checkInt("guard_on/non_table_talent_passes/refused", g_TgdRefused, 0);
        checkInt("guard_on/non_table_talent_passes/sub_not_read", g_TgdSubOff + g_TgdSubUnreadable, 0);
        checkInt("guard_on/non_table_talent_passes", c.tramp, 1);
        (void)before;
    }

    // 40i. Every row unresolved: the guard covers nothing, and an unnamed
    //      talent (a0 that is not a number, read back as -1) must not match an
    //      unresolved row's own -1.
    resetGuard(true);
    for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
    {
        GuardCall c = CallGuard(&dcSelf, (double)kToggleIndicatorTalentId, false);
        checkInt("guard_on/unresolved_row_passes/refused", g_TgdRefused, 0);
        checkInt("guard_on/unresolved_row_passes", c.tramp, 1);

        RValue a0;   // VALUE_UNDEFINED: the call named no talent at all
        RValue a1 = MakeReal(0);
        RValue* args[2] = { &a0, &a1 };
        RValue result = MakeReal(-12345);
        const long trampBefore = g_TrampCalls;
        HookTalentUseClass(&dcSelf, nullptr, result, 2, args);
        checkInt("guard_on/unresolved_row_passes/unnamed_talent_never_matches",
                 g_TrampCalls - trampBefore, 1);
    }

    // ---- R (issue #11 generalisation): the research table's generalised read
    // and timer sampler (`tgprobe tgl`, research build only). The pure pieces
    // are spliced from the tgprobe block; none of them ships. -----------------

    // 41. Row 0 through the generalised read (object index resolved by name,
    //     marker "purgatory", the default ownership field, timer
    //     "destroyTimer") decides exactly what the shipped read decides, on
    //     every world the shipped read's own scenarios use - the live
    //     agree=/disagree= control, proven here first. A negative control
    //     shows the comparison can see a difference at all.
    {
        const std::vector<std::vector<AoeInst>> worlds = {
            {}, { OwnMarked(0.09) }, { OwnUnmarked() }, { OwnMarkerUnreadable() }, { OwnByNumber(1.0) },
            { Foreign(0.09) }, { Unattributed(), Unattributed() }, { OwnMarked(), Foreign() },
            { OwnUnmarked(), OwnMarked(0.09) },
        };
        long cases = 0, mismatches = 0;
        auto compare = [&]() {
            ForgePact::ToggleIndicatorReadDetail shipped;
            const ForgePact::ToggleIndicatorState shippedState = ToggleIndicatorRead(&shipped, false);
            double idx = -1.0;
            TgProbeTglResolveObject(std::string(HeroSiege::Objects::GetObjectName(
                HeroSiege::Objects::GameObject::White_Mage_Soul_Spurn_AOE_obj)), idx);
            TgTglReadResult r;
            const ForgePact::ToggleIndicatorState state = TgProbeTglRead(idx, "purgatory", "", "destroyTimer", &r);
            ++cases;
            if (state != shippedState || !TgProbeTglSameDetail(shipped, r.d)) ++mismatches;
        };
        for (const std::vector<AoeInst>& w : worlds) { resetWorld(); world.instances = w; compare(); }
        resetWorld(); world.instances = { OwnMarked() }; world.instanceNumberThrows = true; compare();
        resetWorld(); world.instances = { OwnMarked() }; world.aoeObjectResolves = false; compare();
        resetWorld();
        for (int i = 0; i < kToggleIndicatorScanCap + 3; ++i) world.instances.push_back(OwnMarked());
        compare();
        checkInt("table/generalised_read_matches_shipped_read_on_row0/cases", cases, 12);
        checkInt("table/generalised_read_matches_shipped_read_on_row0", mismatches, 0);

        resetWorld();
        world.instances = { OwnUnmarked() };
        ForgePact::ToggleIndicatorReadDetail shipped;
        ToggleIndicatorRead(&shipped, false);
        TgTglReadResult noMarker;
        TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &noMarker);   // no marker: a different split
        checkBool("table/generalised_read_matches_shipped_read_on_row0/control_detects_difference",
                  TgProbeTglSameDetail(shipped, noMarker.d), false);
    }

    // 42. A row with no marker field: every own instance counts as marked, so
    //     the marker-required decision is On for an own instance whose
    //     `purgatory` reads 0 (what the shipped row would call a plain cast).
    //     Negative control: a foreign instance still stays Off.
    resetWorld();
    world.instances = { OwnUnmarked() };
    {
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, nullptr, "", nullptr, &r);
        checkInt("table/marker_none_lights_on_any_own/markedMine", r.d.markedMine, 1);
        checkState("table/marker_none_lights_on_any_own",
                   ForgePact::ToggleIndicatorModel::Decide(r.d, /*requireMarker=*/true), ForgePact::ToggleIndicatorState::On);
    }
    resetWorld();
    world.instances = { Foreign(0.0) };
    {
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, nullptr, "", nullptr, &r);
        checkState("table/marker_none_lights_on_any_own/foreign_stays_off",
                   ForgePact::ToggleIndicatorModel::Decide(r.d, /*requireMarker=*/true), ForgePact::ToggleIndicatorState::Off);
    }
    // ... and a row with no ownership field (D-N3) counts every instance as
    // own, including one whose isMyClient could not have been read.
    resetWorld();
    world.instances = { Unattributed() };
    {
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, nullptr, nullptr, nullptr, &r);
        checkInt("table/ownership_none_counts_every_instance_own/unattributed", r.d.unattributed, 0);
        checkInt("table/ownership_none_counts_every_instance_own", r.d.mine, 1);
    }

    // 43. A row whose object name does not resolve: Unreadable, and not one
    //     enumeration call is made on the -1 index.
    resetWorld();
    world.aoeObjectResolves = false;
    world.instances = { OwnMarked() };   // must not matter - the object never resolved
    {
        double idx = 7.0;
        checkBool("table/entry_object_unresolved_is_unreadable/resolved",
                  TgProbeTglResolveObject("Exo_Lunar_Orbit_obj", idx), false);
        const long enumBefore = g_InstanceEnumCalls;
        TgTglReadResult r;
        const ForgePact::ToggleIndicatorState state = TgProbeTglRead(idx, nullptr, "", "destroyTimer", &r);
        checkInt("table/entry_object_unresolved_is_unreadable/no_enumeration", g_InstanceEnumCalls - enumBefore, 0);
        checkState("table/entry_object_unresolved_is_unreadable", state, ForgePact::ToggleIndicatorState::Unreadable);
    }

    // 44. The timer sampler: three draws of one appearance reading 144, 100
    //     and 57 report the first and the last draw's value (session 4's
    //     Soul Spurn shape without Purgatory), and a timer held at the
    //     predicted infinite value -1 is counted by atPredicted=. Only an own
    //     instance's timer is read: a foreign instance notes nothing.
    {
        TgTglTimer t;
        for (double v : { 144.0, 100.0, 57.0 }) {
            resetWorld();
            world.instances = { WithTimer(OwnUnmarked(), MakeReal(v)) };
            TgTglReadResult r;
            TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &r);
            TgProbeTglTimerNote(t, r, -1.0);
        }
        const std::string line = TgProbeTglTimerLine(t);
        checkBool("table/timer_sample_reads_first_and_last/first", line.find("first=144.000000") != std::string::npos, true);
        checkBool("table/timer_sample_reads_first_and_last/last", line.find("last=57.000000") != std::string::npos, true);
        checkBool("table/timer_sample_reads_first_and_last/min_max",
                  line.find("min=57.000000") != std::string::npos && line.find("max=144.000000") != std::string::npos, true);
        checkBool("table/timer_sample_reads_first_and_last/unreadable_zero", line.find("unreadable=0") != std::string::npos, true);

        TgTglTimer held;
        for (int i = 0; i < 3; ++i) {
            resetWorld();
            world.instances = { WithTimer(OwnMarked(0.09), MakeReal(-1.0)) };
            TgTglReadResult r;
            TgProbeTglRead(kAoeObjIdx, "purgatory", "", "destroyTimer", &r);
            TgProbeTglTimerNote(held, r, -1.0);
        }
        checkBool("table/timer_sample_reads_first_and_last/held_at_predicted",
                  TgProbeTglTimerLine(held).find("atPredicted=3") != std::string::npos, true);

        TgTglTimer foreignOnly;
        resetWorld();
        world.instances = { WithTimer(Foreign(0.0), MakeReal(-1.0)) };
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &r);
        TgProbeTglTimerNote(foreignOnly, r, -1.0);
        checkInt("table/timer_sample_reads_first_and_last/foreign_not_read", foreignOnly.draws, 0);
        checkBool("table/timer_sample_reads_first_and_last",
                  line.find("first=144.000000") != std::string::npos && line.find("last=57.000000") != std::string::npos, true);
    }

    // 45. An undefined, a throwing and a string timer field are each counted
    //     unreadable and printed as `unreadable` - never defaulted to -1 or 0,
    //     either of which would read as a real timer value.
    {
        TgTglTimer t;
        resetWorld();
        world.instances = { OwnUnmarked() };   // destroyTimer undefined
        {
            TgTglReadResult r;
            TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &r);
            TgProbeTglTimerNote(t, r, -1.0);
        }
        world.instances[0].destroyTimerThrows = true;
        {
            TgTglReadResult r;
            TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &r);
            TgProbeTglTimerNote(t, r, -1.0);
        }
        world.instances[0].destroyTimerThrows = false;
        world.instances[0].destroyTimer = RValue("-1");
        {
            TgTglReadResult r;
            TgProbeTglRead(kAoeObjIdx, nullptr, "", "destroyTimer", &r);
            TgProbeTglTimerNote(t, r, -1.0);
        }
        const std::string line = TgProbeTglTimerLine(t);
        checkInt("table/timer_unreadable_is_reported_not_defaulted/count", t.unreadable, 3);
        checkInt("table/timer_unreadable_is_reported_not_defaulted/atPredicted", t.atPredicted, 0);
        checkBool("table/timer_unreadable_is_reported_not_defaulted/first", line.find("first=unreadable") != std::string::npos, true);
        checkBool("table/timer_unreadable_is_reported_not_defaulted/last", line.find("last=unreadable") != std::string::npos, true);
        checkBool("table/timer_unreadable_is_reported_not_defaulted",
                  line.find("=-1") == std::string::npos && line.find("=0.000000") == std::string::npos
                  && line.find("first=unreadable") != std::string::npos, true);
    }

    // 46. `tgprobe tgl` is off by default, and off it returns before a single
    //     builtin call, so no other research session pays for the per-draw
    //     reads. Positive control: switched on, the same draw reads every row.
    //     While on, a present instance's `last` field snapshot is retaken at
    //     most once every kTgTglSnapshotEveryDraws draws; the reads and the
    //     timer note stay per draw.
    resetWorld();
    world.instances = { OwnMarked(0.09) };
    {
        g_TgTgl.clear(); g_TgTglSeeded = false;
        checkBool("table/sampler_off_calls_no_builtin/default_off", g_TgTglSamplerOn, false);
        const long before = g_AnyCallCount;
        TgProbeTglAfterDraw();
        checkInt("table/sampler_off_calls_no_builtin", g_AnyCallCount - before, 0);

        g_TgTglSamplerOn = true;
        const long onBefore = g_AnyCallCount;
        TgProbeTglAfterDraw();
        checkBool("table/sampler_off_calls_no_builtin/control_on_reads",
                  g_AnyCallCount - onBefore > 0 && !g_TgTgl.empty() && g_TgTgl[0].samples == 1, true);

        g_TgTgl.clear(); g_TgTglSeeded = false;
        const long namesBefore = g_NamesCalls;
        const int draws = 2 * (int)kTgTglSnapshotEveryDraws + 1;
        for (int i = 0; i < draws; ++i) { ++g_RuntimeFrame; TgProbeTglAfterDraw(); }
        // One snapshot for `first` when the appearance starts, then one
        // `last` per full kTgTglSnapshotEveryDraws draws: 3 per row, not 61.
        checkInt("table/sampler_on_snapshots_at_most_every_30_draws/per_row",
                 (g_NamesCalls - namesBefore) / (long)g_TgTgl.size(), 3);
        checkInt("table/sampler_on_snapshots_at_most_every_30_draws/timer_per_draw",
                 g_TgTgl[0].samples, draws);
        checkBool("table/sampler_on_snapshots_at_most_every_30_draws",
                  (g_NamesCalls - namesBefore) == 3 * (long)g_TgTgl.size()
                  && g_TgTgl[0].fieldsLast.frame == (long)g_RuntimeFrame, true);
        g_TgTglSamplerOn = false;
        g_TgTgl.clear(); g_TgTglSeeded = false;
    }

    // 47. `tgl fields` snapshots the own instance the read used, not
    //     instance 0: a foreign instance sits at index 0 and the own one at
    //     index 1, and the snapshot shows the own one's fields. Negative
    //     control: with only a foreign instance present, nothing is read or
    //     stored and the line says `fields: no own instance`.
    resetWorld();
    world.instances = { Foreign(0.0), OwnMarked(0.09) };
    {
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, "purgatory", "", nullptr, &r);
        TgTglFieldSample s;
        TgProbeTglSnapshot(r, s);
        checkBool("table/fields_snapshot_uses_own_instance/own_fields",
                  s.have && s.text.find("isMyClient=true") != std::string::npos
                  && s.text.find("purgatory=0.09") != std::string::npos, true);
        checkBool("table/fields_snapshot_uses_own_instance",
                  s.have && s.text.find("isMyClient=false") == std::string::npos, true);
    }
    resetWorld();
    world.instances = { Foreign(0.0) };
    {
        TgTglReadResult r;
        TgProbeTglRead(kAoeObjIdx, "purgatory", "", nullptr, &r);
        TgTglFieldSample s;
        const long namesBefore = g_NamesCalls;
        TgProbeTglSnapshot(r, s);
        checkInt("table/fields_snapshot_uses_own_instance/no_own_reads_nothing", g_NamesCalls - namesBefore, 0);
        checkBool("table/fields_snapshot_uses_own_instance/no_own_stores_nothing", s.have || !s.text.empty(), false);
        checkBool("table/fields_snapshot_uses_own_instance/no_own_line",
                  TgProbeTglFieldsText(s).find("fields: no own instance") != std::string::npos, true);
    }

    // ---- issue #55: the timed-skill countdown (`skilltimer`) ---------------
    // Session 8: the countdown reads its own table, kSkillTimerRows, and its
    // ids come from g_SkillTimerTableIds, never the toggle table's. Every
    // scenario written before that table drives the countdown's soulSpurn
    // row - the row whose `isMyClient` ownership and toggle twin (toggle row
    // soulSpurn, marker `purgatory`) the Soul Spurn fixture instances below
    // were written for - resolved to the fixture slot's talent id, every
    // other countdown row left unresolved. The toggle table keeps its own
    // row 0 resolved, the same convention the indicator/guard scenarios above
    // use. g_ToggleBorderOn is forced false throughout: several looks below
    // share draw_rectangle/draw_set_alpha with the border's own marker, and a
    // leftover `true` from an earlier block would draw a second, unrelated
    // pass into the same recordings.
    auto countdownRow = [](const char* abilityId) {
        for (int r = 0; r < ForgePact::kSkillTimerRowCount; ++r)
            if (std::string(ForgePact::kSkillTimerRows[r].abilityId) == abilityId) return r;
        return -1;
    };
    const int kStSoul = countdownRow("soulSpurn");
    const int kStHz = countdownRow("healingZone");
    const int kStBb = countdownRow("bladeBarrier");
    const int kStMf = countdownRow("maelstromOfFrost");
    checkBool("skilltimer/fixture_rows_exist", kStSoul >= 0 && kStHz >= 0 && kStBb >= 0 && kStMf >= 0, true);
    auto resolveOnlyCountdownRow = [](int row, int talentId) {
        for (int r = 0; r < ForgePact::kSkillTimerRowCount; ++r) g_SkillTimerTableIds.Set(r, -1);
        if (row >= 0) g_SkillTimerTableIds.Set(row, talentId);
    };
    for (int r = 0; r < ForgePact::kToggleSkillRowCount; ++r) g_ToggleTableIds.Set(r, -1);
    g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);
    resolveOnlyCountdownRow(kStSoul, kToggleIndicatorTalentId);
    g_ToggleBorderOn.store(false);

    // Expected box for the default row0 fixture, through the shipped box
    // arithmetic (never restated as literals) - every geometry check below
    // is derived from this, not from a hand-computed pixel pair.
    double stEx = 0, stEy = 0, stEw = 0, stEh = 0;
    ToggleIndicatorMarkerBox(world.row0[0].navBboxX, world.row0[0].navBboxY,
                             world.row0[0].navBboxWidth, world.row0[0].navBboxHeight,
                             stEx, stEy, stEw, stEh);

    // 1. Off is the first statement: no runtime call at all, even with a
    //    readable own instance sitting there.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Off);
    {
        long before = g_AnyCallCount;
        SkillTimerDraw();
        checkInt("skilltimer/off_makes_no_runtime_calls", g_AnyCallCount - before, 0);
    }

    // 2. No own instance at all: draws nothing, counts noInstance.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = {};
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        SkillTimerDraw();
        checkInt("skilltimer/no_instance_draws_nothing", (long long)g_StRow[kStSoul].noInstance, 1);
        checkInt("skilltimer/no_instance_draws_nothing/rects", (long long)g_ColourRectDraws.size(), 0);
    }

    // 3. An own instance exists but its own timer field is unreadable
    //    (undefined): draws nothing, counts unreadable, never noInstance.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = { OwnUnmarked() };   // destroyTimer left undefined
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        SkillTimerDraw();
        checkInt("skilltimer/unreadable_timer_draws_nothing", (long long)g_StRow[kStSoul].unreadable, 1);
        checkInt("skilltimer/unreadable_timer_draws_nothing/rects", (long long)g_ColourRectDraws.size(), 0);
    }

    // 4. A foreign instance, even with a readable timer, is never counted:
    //    the row reads as having no OWN instance at all.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(Foreign(), MakeReal(50.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        SkillTimerDraw();
        checkInt("skilltimer/foreign_instance_not_counted", (long long)g_StRow[kStSoul].noInstance, 1);
        checkInt("skilltimer/foreign_instance_not_counted/not_unreadable", (long long)g_StRow[kStSoul].unreadable, 0);
    }

    // 5. A row whose toggle read says On never gets a countdown (D-T4) -
    //    even though its own timer is readable.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnMarked(0.09), MakeReal(50.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        SkillTimerDraw();
        checkInt("skilltimer/toggle_on_suppresses", (long long)g_StRow[kStSoul].toggleOn, 1);
        checkInt("skilltimer/toggle_on_suppresses/rects", (long long)g_ColourRectDraws.size(), 0);
    }

    // 6. An unresolved row's talent id is skipped and counted, the same
    //    fail-safe the indicator applies.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerTableIds.Set(kStSoul, -1);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        SkillTimerDraw();
        checkInt("skilltimer/unresolved_row_skipped", (long long)g_StRow[kStSoul].unresolved, 1);
        checkInt("skilltimer/unresolved_row_skipped/rects", (long long)g_ColourRectDraws.size(), 0);
    }
    g_SkillTimerTableIds.Set(kStSoul, kToggleIndicatorTalentId);   // restore for the rest of this block

    // 7. A slot miss is charged to THIS mod's own noSlot, never to
    //    toggleborder's noHud/noRow0/noTalent counters (the shared lookup's
    //    failure-reason split moved to the caller for exactly this reason).
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    world.hudTalentObjectResolves = false;
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        g_TibNoHud = 0; resetTibRow(0);
        SkillTimerDraw();
        checkInt("skilltimer/slot_miss_not_charged_to_toggleborder", (long long)g_StRow[kStSoul].noSlot, 1);
        checkInt("skilltimer/slot_miss_not_charged_to_toggleborder/tibNoHud", g_TibNoHud, 0);
        checkInt("skilltimer/slot_miss_not_charged_to_toggleborder/tibRowNoSlot", (long long)g_TibRow[0].noSlot, 0);
    }

    // ---- route B: the latch model itself (game-independent) ---------------

    // 8. No latch yet: the first reading IS the total, fraction 1.0.
    {
        ForgePact::SkillTimerRowState state;
        auto d = ForgePact::SkillTimerModel::Decide(state, true, true, 100.0);
        checkBool("skilltimer/first_sight_latches_full/outcome",
                  d.outcome == ForgePact::SkillTimerOutcome::Drawn, true);
        checkNear("skilltimer/first_sight_latches_full", d.fraction, 1.0);
        checkBool("skilltimer/first_sight_latches_full/latched", d.latchedThisCall, true);
    }

    // 9. A later, smaller reading (not a rise) divides by the held latch
    //    rather than re-latching.
    {
        ForgePact::SkillTimerRowState state;
        ForgePact::SkillTimerModel::Decide(state, true, true, 100.0);
        auto d = ForgePact::SkillTimerModel::Decide(state, true, true, 40.0);
        checkNear("skilltimer/fraction_is_remaining_over_latch", d.fraction, 0.4);
        checkBool("skilltimer/fraction_is_remaining_over_latch/not_relatched", d.latchedThisCall, false);
    }

    // 10. A rise - a new cast - re-latches at the new, higher value.
    {
        ForgePact::SkillTimerRowState state;
        ForgePact::SkillTimerModel::Decide(state, true, true, 100.0);
        ForgePact::SkillTimerModel::Decide(state, true, true, 40.0);   // falling within the same cast
        auto d = ForgePact::SkillTimerModel::Decide(state, true, true, 200.0);   // a rise: a new cast
        checkNear("skilltimer/rise_relatches", d.fraction, 1.0);
        checkBool("skilltimer/rise_relatches/latched", d.latchedThisCall, true);
    }

    // 11. The instance disappearing drops a held latch and counts it, so the
    //     NEXT appearance re-latches full rather than dividing by a stale
    //     value.
    {
        ForgePact::SkillTimerRowState state;
        ForgePact::SkillTimerModel::Decide(state, true, true, 100.0);
        auto d = ForgePact::SkillTimerModel::Decide(state, false, false, 0.0);
        checkBool("skilltimer/instance_gone_unlatches/outcome",
                  d.outcome == ForgePact::SkillTimerOutcome::NoInstance, true);
        checkBool("skilltimer/instance_gone_unlatches", d.unlatchedThisCall, true);
        checkBool("skilltimer/instance_gone_unlatches/state_cleared", state.latched, false);
    }

    // 12. A non-positive reading (a held negative, or a plain cast's trailing
    //     tail) draws nothing and never latches - the held latch, if any,
    //     stays exactly as it was.
    {
        ForgePact::SkillTimerRowState state;
        ForgePact::SkillTimerModel::Decide(state, true, true, 100.0);
        auto d = ForgePact::SkillTimerModel::Decide(state, true, true, -0.6);
        checkBool("skilltimer/non_positive_draws_nothing_and_never_latches/outcome",
                  d.outcome == ForgePact::SkillTimerOutcome::Expired, true);
        checkBool("skilltimer/non_positive_draws_nothing_and_never_latches", d.latchedThisCall, false);
        checkNear("skilltimer/non_positive_draws_nothing_and_never_latches/latch_untouched", state.latch, 100.0);
    }

    // ---- the four looks, drawn against a real fraction < 1.0 --------------
    // Every scenario below latches full on a first draw (cleared from the
    // recordings), then re-reads at a smaller, non-rising value so the SECOND
    // draw is the one actually checked, at a known fraction.

    // 13. `bar`: left edge inset, bottom edge above the box, width scaled by
    //     the fraction - the live-confirmed placement.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(100.0)) };
    SkillTimerDraw();   // latches at 100, fraction 1.0 - not what this checks
    resetDrawRecord(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(40.0)) };   // fraction 0.4
    SkillTimerDraw();
    {
        const double usableWidth = stEw - 2.0 * kSkillTimerBarInset;
        const double barWidth = usableWidth * 0.4;
        const double bx0 = stEx + kSkillTimerBarInset, by1 = stEy - kSkillTimerBarGap;
        const double bx1 = bx0 + barWidth, by0 = by1 - kSkillTimerBarHeight;
        checkInt("skilltimer/bar_geometry/count", (long long)g_ColourRectDraws.size(), 1);
        if (!g_ColourRectDraws.empty()) {
            const ColourRectDraw& r = g_ColourRectDraws.back();
            checkNear("skilltimer/bar_geometry/x0", r.x0, bx0);
            checkNear("skilltimer/bar_geometry/y0", r.y0, by0);
            checkNear("skilltimer/bar_geometry/x1", r.x1, bx1);
            checkNear("skilltimer/bar_geometry", r.y1, by1);
        }
        checkNear("skilltimer/bar_geometry/colour_r", g_ColourR, kSkillTimerColourR);
        checkNear("skilltimer/bar_geometry/colour_g", g_ColourG, kSkillTimerColourG);
        checkNear("skilltimer/bar_geometry/colour_b", g_ColourB, kSkillTimerColourB);
    }

    // 14. A bar under 1 px wide draws nothing at all (D4's sub-pixel guard).
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(1000.0)) };
    SkillTimerDraw();
    resetDrawRecord(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(9.0)) };   // fraction 0.009
    SkillTimerDraw();
    checkInt("skilltimer/bar_subpixel_draws_nothing", (long long)g_ColourRectDraws.size(), 0);

    // 15. `number`: the fraction as a whole percentage, centred, anchored
    //     top-aligned at the box's BOTTOM edge plus kSkillTimerTextOffsetDy
    //     (2026-09-21 live tuning: the owner's own probe, same box, same
    //     font, `textoffset 0 -106` - "perfect"). Ship draws the probe's
    //     exact formula so what was judged is what ships (D-N1).
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Number);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(100.0)) };
    SkillTimerDraw();
    resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(42.0)) };   // fraction 0.42 -> 42%
    SkillTimerDraw();
    {
        const double tx = stEx + stEw / 2.0 + kSkillTimerTextOffsetDx;
        const double ty = stEy + stEh + kSkillTimerTextOffsetDy;
        checkInt("skilltimer/number_text_and_anchor/count", (long long)g_TextDraws.size(), 1);
        if (!g_TextDraws.empty()) {
            checkNear("skilltimer/number_text_and_anchor/x", g_TextDraws.back().x, tx);
            checkNear("skilltimer/number_text_and_anchor/y", g_TextDraws.back().y, ty);
            checkNear("skilltimer/number_text_and_anchor/valign_top", g_TextDraws.back().valign, 0.0);
            checkBool("skilltimer/number_text_and_anchor", g_TextDraws.back().text == "42%", true);
        }
    }

    // 16. A fraction that rounds to 0% draws nothing at all - the ship-only
    //     difference from the research instrument, which keeps 0% as its own
    //     liveness signal.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Number);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(1000.0)) };
    SkillTimerDraw();
    resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(4.0)) };   // fraction 0.004 -> rounds to 0%
    SkillTimerDraw();
    checkInt("skilltimer/number_zero_percent_draws_nothing", (long long)g_TextDraws.size(), 0);
    checkNear("skilltimer/number_zero_percent_draws_nothing/no_font_set", g_LastSetFont, -1.0);

    // 17. Every draw state `number` touches - font, colour, alpha, halign,
    //     valign - is restored to its pre-draw value afterwards.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Number);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(100.0)) };
    SkillTimerDraw();
    resetSkillTimerDrawRecord();
    g_LastSetColour = -999; g_LastSetAlpha = -999;
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(42.0)) };
    SkillTimerDraw();
    checkNear("skilltimer/number_restores_draw_state/font", g_LastSetFont, kPrevFont);
    checkNear("skilltimer/number_restores_draw_state/halign", g_LastSetHalign, kPrevHalign);
    checkNear("skilltimer/number_restores_draw_state/valign", g_LastSetValign, kPrevValign);
    checkNear("skilltimer/number_restores_draw_state/colour", g_LastSetColour, kPrevColour);
    checkNear("skilltimer/number_restores_draw_state", g_LastSetAlpha, kPrevAlpha);

    // 18. `arc`: each nested band traces only `fraction` of its own
    //     perimeter, clockwise from the top-left corner - checked on the
    //     innermost band, which (at this fraction) stays within the top edge.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Arc);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(1000.0)) };
    SkillTimerDraw();
    resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };   // fraction 0.05
    SkillTimerDraw();
    {
        const double perimeter0 = 2.0 * (stEw + stEh);
        const double length0 = 0.05 * perimeter0;
        checkInt("skilltimer/arc_traces_fraction_of_perimeter/count", (long long)g_LineDraws.size(), (long long)kSkillTimerBands);
        if (!g_LineDraws.empty()) {
            const LineDraw& first = g_LineDraws.front();
            checkNear("skilltimer/arc_traces_fraction_of_perimeter/x0", first.x0, stEx);
            checkNear("skilltimer/arc_traces_fraction_of_perimeter/y0", first.y0, stEy);
            checkNear("skilltimer/arc_traces_fraction_of_perimeter/x1", first.x1, stEx + length0);
            checkNear("skilltimer/arc_traces_fraction_of_perimeter", first.y1, stEy);
        }
    }

    // 19. `fade`: the same 10 bands as `arc`, whole rectangle each, alpha
    //     `(1 - i/9) * fraction`.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Fade);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(100.0)) };
    SkillTimerDraw();
    resetDrawRecord(); resetSkillTimerDrawRecord();
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };   // fraction 0.5
    SkillTimerDraw();
    {
        // The 10 band alphas, plus one more call restoring the pre-draw
        // alpha afterwards (SkillTimerDraw's own save/restore around the
        // style call) - so only the leading kSkillTimerBands entries are
        // this look's own bands.
        bool alphasOk = g_SetAlphas.size() >= (size_t)kSkillTimerBands;
        for (int i = 0; alphasOk && i < kSkillTimerBands; ++i) {
            const double t = (double)i / (double)(kSkillTimerBands - 1);
            if (std::fabs(g_SetAlphas[(size_t)i] - (1.0 - t) * 0.5) > 1e-6) alphasOk = false;
        }
        checkBool("skilltimer/fade_scales_band_alpha", alphasOk, true);
        checkInt("skilltimer/fade_scales_band_alpha/rects", (long long)g_DrawnRects.size(), (long long)kSkillTimerBands);
    }

    // 20. A throwing style draw is counted and never leaves the marker's
    //     colour/alpha set for the next HUD draw to inherit - the same
    //     restore-on-the-throw-path discipline ToggleIndicatorDraw uses.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Arc);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    world.drawLineThrows = true;
    g_LastSetColour = -999; g_LastSetAlpha = -999;
    SkillTimerDraw();
    checkInt("skilltimer/draw_throw_restores_and_counts/drawn", (long long)g_StRow[kStSoul].drawn, 0);
    checkInt("skilltimer/draw_throw_restores_and_counts", g_StDrawExc, 1);
    checkNear("skilltimer/draw_throw_restores_and_counts/colour_restored", g_LastSetColour, kPrevColour);
    checkNear("skilltimer/draw_throw_restores_and_counts/alpha_restored", g_LastSetAlpha, kPrevAlpha);
    world.drawLineThrows = false;

    // ---- session 8: the countdown's own table (kSkillTimerRows) -----------

    // 21. A countdown row with no toggle twin (healingZone) makes no toggle
    //     read at all: no toggle-table row's object is even resolved. Its
    //     instance's `isMyClient` is unreadable on purpose - the row ships
    //     with no ownership field (session 8 measured `own=unreadable`), so
    //     every instance is own, and a toggle read of any twin would have
    //     answered Unreadable and suppressed it. Drawing proves neither ran.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(kStHz, kToggleIndicatorTalentId);
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.instances = { WithTimer(Unattributed(), MakeReal(1152.0)) };
    g_AssetLookups.clear();
    SkillTimerDraw();
    {
        long toggleLookups = 0;
        for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t)
            toggleLookups += g_AssetLookups[HeroSiege::Objects::GetObjectName(ForgePact::kToggleSkillRows[t].onObject)];
        checkInt("skilltimer/non_toggle_row_makes_no_toggle_read/drawn", (long long)g_StRow[kStHz].drawn, 1);
        checkInt("skilltimer/non_toggle_row_makes_no_toggle_read", toggleLookups, 0);
        checkInt("skilltimer/non_toggle_row_makes_no_toggle_read/no_toggle_count",
                 (long long)(g_StRow[kStHz].toggleOn + g_StRow[kStHz].toggleUnreadable), 0);
    }

    // 22. A countdown row that IS a toggle row (maelstromOfFrost) still runs
    //     that toggle row's read first and is suppressed while it is on: an
    //     own instance holding destroyTimer at the measured -1 is the toggle,
    //     charged to toggleOn - not to `expired`, which is what the timer
    //     read alone would have made of a -1.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(kStMf, kToggleIndicatorTalentId);
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(-1.0)) };
    g_AssetLookups.clear();
    SkillTimerDraw();
    checkInt("skilltimer/toggle_row_still_suppressed_when_on", (long long)g_StRow[kStMf].toggleOn, 1);
    checkInt("skilltimer/toggle_row_still_suppressed_when_on/not_expired", (long long)g_StRow[kStMf].expired, 0);
    checkInt("skilltimer/toggle_row_still_suppressed_when_on/rects", (long long)g_ColourRectDraws.size(), 0);
    checkBool("skilltimer/toggle_row_still_suppressed_when_on/toggle_object_read",
              g_AssetLookups[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Prophet_Maelstrom_obj)] >= 1, true);

    // 23. Every toggle-table row resolved, every countdown row not: the
    //     countdown takes its ids from its own table only, so every row is
    //     skipped as unresolved and no instance is even counted.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(-1, 0);
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, kToggleIndicatorTalentId + t);
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.instances = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    {
        const long enumBefore = g_InstanceEnumCalls;
        SkillTimerDraw();
        long unresolved = 0;
        for (int r = 0; r < ForgePact::kSkillTimerRowCount; ++r) unresolved += g_StRow[r].unresolved;
        checkInt("skilltimer/unresolved_countdown_row_skipped", unresolved, ForgePact::kSkillTimerRowCount);
        checkInt("skilltimer/unresolved_countdown_row_skipped/rects", (long long)g_ColourRectDraws.size(), 0);
        checkInt("skilltimer/unresolved_countdown_row_skipped/no_instance_read", g_InstanceEnumCalls - enumBefore, 0);
    }
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, -1);
    g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);

    // 24. Two countdown rows on two slots keep their own latches: Healing
    //     Zone latches at 1152 and Blade Barrier at 1296 (session 8's first
    //     readings); Healing Zone falling to 576 reads 50% while Blade
    //     Barrier, unchanged, still reads 100%.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(kStHz, 252);
    g_SkillTimerTableIds.Set(kStBb, 137);
    world.row0 = { { 252.0, 100.0, 200.0, 50.0, 60.0 }, { 137.0, 300.0, 200.0, 50.0, 60.0 } };
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::White_Mage_Healing_Zone_obj)] = 600.0;
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Samurai_Blade_Barrier_obj)] = 601.0;
    world.instancesByIndex[600.0] = { WithTimer(Unattributed(), MakeReal(1152.0)) };
    world.instancesByIndex[601.0] = { WithTimer(OwnUnmarked(), MakeReal(1296.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Number);
    SkillTimerDraw();
    checkNear("skilltimer/rows_keep_separate_latches/first", g_SkillTimerRowState[kStHz].latch, 1152.0);
    checkNear("skilltimer/rows_keep_separate_latches/second", g_SkillTimerRowState[kStBb].latch, 1296.0);
    resetSkillTimerDrawRecord();
    world.instancesByIndex[600.0] = { WithTimer(Unattributed(), MakeReal(576.0)) };
    SkillTimerDraw();
    checkBool("skilltimer/rows_keep_separate_latches",
              std::fabs(g_SkillTimerRowState[kStHz].latch - 1152.0) < 1e-6
              && std::fabs(g_SkillTimerRowState[kStBb].latch - 1296.0) < 1e-6, true);
    {
        bool half = false, full = false;
        for (const TextDraw& t : g_TextDraws) { if (t.text == "50%") half = true; if (t.text == "100%") full = true; }
        checkBool("skilltimer/rows_keep_separate_latches/fraction", half && full && g_TextDraws.size() == 2, true);
    }
    resolveOnlyCountdownRow(kStSoul, kToggleIndicatorTalentId);

    // ---- session 8: `tgprobe sweep`, every class's timed skill at once ----
    // Every root name the harness does not map answers kAoeObjIdx and
    // `world.instances`, so each instance below is seen under all six roots
    // - the same double-enumeration the live sentry/ability parents give -
    // and a count that summed across roots would say so.
    const double kSweepObjA = 3697.0, kSweepObjB = 5738.0;
    auto resetSweep = [&]() { g_TgSweep.clear(); g_TgSweepOn = false; g_TgSweepDraws = 0; g_TgSweepIndexUnreadable = 0; };

    // S1. Off by default, and off makes no builtin call at all; switched on
    //     the same draw reads (positive control).
    resetWorld(); resetSweep();
    world.instances = { OfObject(WithTimer(OwnUnmarked(), MakeReal(100.0)), kSweepObjA) };
    {
        checkBool("sweep/off_makes_no_runtime_calls/default_off", g_TgSweepOn, false);
        const long before = g_AnyCallCount;
        TgProbeSweepAfterDraw();
        checkBool("sweep/off_makes_no_runtime_calls", g_AnyCallCount - before == 0 && g_TgSweep.empty(), true);
        g_TgSweepOn = true;
        const long onBefore = g_AnyCallCount;
        TgProbeSweepAfterDraw();
        checkBool("sweep/off_makes_no_runtime_calls/control_on_reads",
                  g_AnyCallCount - onBefore > 0 && g_TgSweep.count((int)kSweepObjA) == 1, true);
    }

    // S2. An appearance is a rising edge: present, present, absent, present
    //     is two appearances; `draws` is the current appearance's, and the
    //     instance counted under six roots is still one instance.
    resetWorld(); resetSweep(); g_TgSweepOn = true;
    {
        const AoeInst a = OfObject(WithTimer(OwnUnmarked(), MakeReal(100.0)), kSweepObjA);
        world.instances = { a, a };
        ++g_RuntimeFrame; TgProbeSweepAfterDraw();
        ++g_RuntimeFrame; TgProbeSweepAfterDraw();
        world.instances.clear();
        ++g_RuntimeFrame; TgProbeSweepAfterDraw();
        const bool gone = !g_TgSweep[(int)kSweepObjA].present;
        world.instances = { a };
        ++g_RuntimeFrame; TgProbeSweepAfterDraw();
        const TgSweepRecord& rec = g_TgSweep[(int)kSweepObjA];
        checkInt("sweep/appearance_counts_rising_edge", rec.app, 2);
        checkBool("sweep/appearance_counts_rising_edge/absent_draw_clears_present", gone, true);
        checkInt("sweep/appearance_counts_rising_edge/draws_is_current_appearance", rec.draws, 1);
        checkInt("sweep/appearance_counts_rising_edge/totalDraws", rec.totalDraws, 3);
        checkInt("sweep/appearance_counts_rising_edge/maxInst_not_summed_across_roots", rec.maxInst, 2);
    }

    // S3. `first` is the first READABLE value of the appearance: an
    //     unreadable first draw leaves it unset, the next numeric draw sets
    //     it, a later draw moves only `last`; a new appearance restarts it.
    resetWorld(); resetSweep(); g_TgSweepOn = true;
    {
        world.instances = { OfObject(OwnUnmarked(), kSweepObjA) };   // destroyTimer undefined
        TgProbeSweepAfterDraw();
        const bool unsetAfterUnreadable = !g_TgSweep[(int)kSweepObjA].haveFirst;
        world.instances = { OfObject(WithTimer(OwnUnmarked(), MakeReal(50.0)), kSweepObjA) };
        TgProbeSweepAfterDraw();
        world.instances = { OfObject(WithTimer(OwnUnmarked(), MakeReal(40.0)), kSweepObjA) };
        TgProbeSweepAfterDraw();
        const TgSweepRecord& rec = g_TgSweep[(int)kSweepObjA];
        checkBool("sweep/first_is_first_readable_of_appearance/unset_after_unreadable", unsetAfterUnreadable, true);
        checkNear("sweep/first_is_first_readable_of_appearance", rec.first, 50.0);
        checkNear("sweep/first_is_first_readable_of_appearance/last", rec.last, 40.0);
        checkNear("sweep/first_is_first_readable_of_appearance/min", rec.min, 40.0);
        checkNear("sweep/first_is_first_readable_of_appearance/max", rec.max, 50.0);
        checkInt("sweep/first_is_first_readable_of_appearance/timerUnreadable", rec.timerUnreadable, 1);
        world.instances.clear();
        TgProbeSweepAfterDraw();
        world.instances = { OfObject(WithTimer(OwnUnmarked(), MakeReal(70.0)), kSweepObjA) };
        TgProbeSweepAfterDraw();
        const TgSweepRecord& again = g_TgSweep[(int)kSweepObjA];
        checkNear("sweep/first_is_first_readable_of_appearance/restarts", again.first, 70.0);
        checkInt("sweep/first_is_first_readable_of_appearance/restarts_unreadable", again.timerUnreadable, 0);
    }

    // S4. An unreadable timer never becomes a number: undefined, a throw, a
    //     string and a bool all count timerUnreadable and leave first/min/
    //     max unset. Negative control beside it: an int64 reading counts.
    resetWorld(); resetSweep(); g_TgSweepOn = true;
    {
        AoeInst undef = OfObject(OwnUnmarked(), kSweepObjA);
        AoeInst threw = OfObject(OwnUnmarked(), kSweepObjA); threw.destroyTimerThrows = true;
        RValue text("144"); RValue flag = MakeBool(true);
        AoeInst str = OfObject(WithTimer(OwnUnmarked(), text), kSweepObjA);
        AoeInst boo = OfObject(WithTimer(OwnUnmarked(), flag), kSweepObjA);
        for (const AoeInst& a : { undef, threw, str, boo }) { world.instances = { a }; TgProbeSweepAfterDraw(); }
        const TgSweepRecord& rec = g_TgSweep[(int)kSweepObjA];
        checkBool("sweep/unreadable_never_defaults", !rec.haveFirst && rec.first == 0.0 && rec.min == 0.0 && rec.max == 0.0, true);
        checkInt("sweep/unreadable_never_defaults/count", rec.timerUnreadable, 4);
        checkInt("sweep/unreadable_never_defaults/draws", rec.draws, 4);
        RValue i64; i64.m_Kind = VALUE_INT64; i64.number = 144.0;
        world.instances = { OfObject(WithTimer(OwnUnmarked(), i64), kSweepObjA) };
        TgProbeSweepAfterDraw();
        checkBool("sweep/unreadable_never_defaults/control_int64_reads",
                  g_TgSweep[(int)kSweepObjA].haveFirst && g_TgSweep[(int)kSweepObjA].first == 144.0, true);
        // An object_index that is not an index (undefined) is counted and
        // makes no record at all - never object 0.
        world.instances = { WithTimer(OwnUnmarked(), MakeReal(9.0)) };
        const long idxBefore = g_TgSweepIndexUnreadable;
        TgProbeSweepAfterDraw();
        checkBool("sweep/unreadable_never_defaults/index_unreadable_counted",
                  g_TgSweepIndexUnreadable > idxBefore && g_TgSweep.count(0) == 0, true);
    }

    // S5. The draw's value is the largest reading among the object's
    //     instances not measured foreign; a foreign instance's larger timer
    //     is not taken, and two objects on one root keep separate records.
    resetWorld(); resetSweep(); g_TgSweepOn = true;
    {
        world.instances = {
            OfObject(WithTimer(OwnUnmarked(), MakeReal(100.0)), kSweepObjA),
            OfObject(WithTimer(OwnUnmarked(), MakeReal(300.0)), kSweepObjA),
            OfObject(WithTimer(Foreign(), MakeReal(999.0)), kSweepObjA),
            OfObject(WithTimer(OwnUnmarked(), MakeReal(7.0)), kSweepObjB),
        };
        TgProbeSweepAfterDraw();
        checkNear("sweep/largest_reading_of_draw_wins", g_TgSweep[(int)kSweepObjA].first, 300.0);
        checkNear("sweep/largest_reading_of_draw_wins/foreign_not_taken", g_TgSweep[(int)kSweepObjA].max, 300.0);
        checkNear("sweep/largest_reading_of_draw_wins/other_object_separate", g_TgSweep[(int)kSweepObjB].first, 7.0);
        checkInt("sweep/largest_reading_of_draw_wins/maxInst", g_TgSweep[(int)kSweepObjA].maxInst, 3);
        // Only foreign instances with a timer: present, but no own reading.
        world.instances = { OfObject(WithTimer(Foreign(), MakeReal(999.0)), kSweepObjB) };
        TgProbeSweepAfterDraw();
        checkInt("sweep/largest_reading_of_draw_wins/foreign_only_is_unreadable",
                 g_TgSweep[(int)kSweepObjB].timerUnreadable, 1);
    }

    // S6. Ownership readability is counted per draw: every isMyClient
    //     kind-checked (readable), some (mixed), none (unreadable) - and the
    //     record's own= says mixed once draws disagree.
    resetWorld(); resetSweep(); g_TgSweepOn = true;
    {
        const AoeInst own = OfObject(WithTimer(OwnUnmarked(), MakeReal(10.0)), kSweepObjA);
        const AoeInst unattr = OfObject(WithTimer(Unattributed(), MakeReal(10.0)), kSweepObjA);
        world.instances = { own, own };       TgProbeSweepAfterDraw();
        world.instances = { own, unattr };    TgProbeSweepAfterDraw();
        world.instances = { unattr, unattr }; TgProbeSweepAfterDraw();
        const TgSweepRecord& rec = g_TgSweep[(int)kSweepObjA];
        checkInt("sweep/ownership_readability_counted_per_draw/readable", rec.ownReadableDraws, 1);
        checkInt("sweep/ownership_readability_counted_per_draw/mixed", rec.ownMixedDraws, 1);
        checkInt("sweep/ownership_readability_counted_per_draw/unreadable", rec.ownUnreadableDraws, 1);
        checkBool("sweep/ownership_readability_counted_per_draw",
                  std::string(TgProbeSweepOwnText(rec)) == "mixed", true);
        // An unattributed instance still counts as own for the timer (D-N3):
        // the third draw read its 10.
        checkInt("sweep/ownership_readability_counted_per_draw/unattributed_timer_read", rec.timerUnreadable, 0);
        resetSweep(); g_TgSweepOn = true;
        world.instances = { own };    TgProbeSweepAfterDraw();
        checkBool("sweep/ownership_readability_counted_per_draw/all_readable",
                  std::string(TgProbeSweepOwnText(g_TgSweep[(int)kSweepObjA])) == "readable", true);
        resetSweep(); g_TgSweepOn = true;
        world.instances = { unattr }; TgProbeSweepAfterDraw();
        checkBool("sweep/ownership_readability_counted_per_draw/all_unreadable",
                  std::string(TgProbeSweepOwnText(g_TgSweep[(int)kSweepObjA])) == "unreadable", true);
    }
    resetSweep();

    // ---- issue #55 follow-up (D-S4): rule-based coverage of untested skills

    // R1-R6. SkillTimerRuleModel::Eligible is a pure, game-independent
    // function (header-side, SkillTimerMod.hpp) - these need no game API
    // stand-in at all. Pinned points from context, "Eligibility, read once
    // per room": cooldown == the floor is ineligible, a hair above it is
    // eligible; duration == 0 is ineligible regardless of cooldown.
    checkBool("rule/eligible_duration_and_cooldown",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, 5.0, true, false, false), true);
    checkBool("rule/cooldown_at_floor_is_ineligible",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, ForgePact::kSkillTimerCooldownFloor, true, false, false), false);
    checkBool("rule/cooldown_just_above_floor_is_eligible",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, ForgePact::kSkillTimerCooldownFloor + 0.0000001, true, false, false), true);
    checkBool("rule/duration_zero_is_ineligible",
              ForgePact::SkillTimerRuleModel::Eligible(0.0, 5.0, true, false, false), false);
    checkBool("rule/unreadable_field_is_ineligible_and_counted",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, 5.0, /*readable=*/false, false, false), false);
    checkBool("rule/deny_list_wins_over_the_rule",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, 5.0, true, /*denied=*/true, false), false);
    checkBool("rule/explicit_row_wins_over_the_rule",
              ForgePact::SkillTimerRuleModel::Eligible(5.0, 5.0, true, false, /*isExplicitRow=*/true), false);

    // R7. Structural exclusion is the GENERATOR's job (tools/gen_skill_timer_names.py),
    // not this pure function - this harness's own stand-in table above
    // deliberately carries no "arrowturret"-style companion key, the same
    // absence the real generator's Player_Sentry_Parent_obj exclusion
    // produces (pinned for the real table by
    // test_generated_table_has_no_companion_and_no_ambiguous_entry).
    {
        bool found = false;
        for (int i = 0; i < ForgePact::kSkillTimerNameCount; ++i)
            if (std::string(ForgePact::kSkillTimerNames[i].key) == "arrowturret") found = true;
        checkBool("rule/companion_never_enters_the_table", found, false);
    }

    // The runtime-built rule map itself, driven directly (the same shape
    // skilltimer/* scenarios drive g_SkillTimerTableIds - the walk that
    // BUILDS this map is not spliced here; test_toggle_skill_contract.py
    // pins it against the plugin's own text).
    auto resetRuleEntries = [](std::vector<std::pair<int, int>> entries) {
        g_SkillTimerRuleCount = 0;
        for (int i = 0; i < ForgePact::kSkillTimerRuleCap; ++i) g_SkillTimerRuleEntries[i] = ForgePact::SkillTimerRuleEntry{};
        for (const std::pair<int, int>& p : entries) {
            ForgePact::SkillTimerRuleEntry e;
            e.talentId = p.first;
            e.nameIndex = p.second;
            e.abilityId = "ruleTest";
            g_SkillTimerRuleEntries[g_SkillTimerRuleCount] = e;
            g_SkillTimerRuleCount = g_SkillTimerRuleCount + 1;
        }
    };

    // R8. A rule entry whose talent id is not on the hotbar costs nothing
    // beyond the one shared row0 walk every active entry pays together: no
    // toggle read, no object resolve, no instance scan.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(-1, 0);
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, -1);
    resetRuleEntries({ { 999, kRuleAlphaIndex } });   // 999 is on no hotbar slot (default row0 only has 240)
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Alpha_obj)] = 701.0;
    world.instancesByIndex[701.0] = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    {
        const long enumBefore = g_InstanceEnumCalls;
        g_AssetLookups.clear();
        SkillTimerDraw();
        // The delta of 1 is the shared hotbar walk's own instance_find(hud, 0)
        // - paid once regardless of how many entries are active - not a scan
        // of this entry's own object, which is never even resolved by name.
        checkInt("rule/slot_off_hotbar_costs_no_instance_scan", g_InstanceEnumCalls - enumBefore, 1);
        checkInt("rule/slot_off_hotbar_costs_no_instance_scan/no_object_resolve",
                 g_AssetLookups[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Alpha_obj)], 0);
        checkInt("rule/slot_off_hotbar_costs_no_instance_scan/no_outcome_counted",
                 g_RuleDrawn + g_RuleNoInstance + g_RuleUnreadable + g_RuleExpired + g_RuleToggleOn + g_RuleNoObject, 0);
    }

    // R9. A talent whose object never resolves by name is counted
    // (ruleNoObject) and draws nothing - the instance scan never runs.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(-1, 0);
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, -1);
    resetRuleEntries({ { 903, kRuleAlphaIndex } });
    world.row0 = { { 903.0, 100.0, 200.0, 50.0, 60.0 } };
    world.aoeObjectResolves = false;   // Rule_Alpha_obj is never registered by name below
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    {
        const long enumBefore = g_InstanceEnumCalls;
        SkillTimerDraw();
        checkInt("rule/no_object_by_name_is_counted_not_drawn", g_RuleNoObject, 1);
        checkInt("rule/no_object_by_name_is_counted_not_drawn/not_drawn", g_RuleDrawn, 0);
        checkInt("rule/no_object_by_name_is_counted_not_drawn/no_instance_scan",
                 g_InstanceEnumCalls - enumBefore, 1);   // the shared hotbar walk's own instance_find(hud, 0) only
    }
    world.aoeObjectResolves = true;

    // R10. A rule entry that is ALSO a toggle-table row (its talent id
    // matches a resolved toggle row) is suppressed while that row reads On -
    // D-T4, the same suppression the explicit rows' own twin check applies.
    // The entry's own object is never even resolved.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(-1, 0);
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, -1);
    g_ToggleTableIds.Set(0, kToggleIndicatorTalentId);   // row 0 = soulSpurn, Marker "purgatory"
    resetRuleEntries({ { kToggleIndicatorTalentId, kRuleAlphaIndex } });
    world.row0 = { { (double)kToggleIndicatorTalentId, 100.0, 200.0, 50.0, 60.0 } };
    world.instances = { OwnMarked() };   // row 0's own onObject (default index) - marked ON
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Alpha_obj)] = 702.0;
    world.instancesByIndex[702.0] = { WithTimer(OwnUnmarked(), MakeReal(50.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Bar);
    g_AssetLookups.clear();
    SkillTimerDraw();
    checkInt("rule/toggle_twin_is_suppressed_when_on", g_RuleToggleOn, 1);
    checkInt("rule/toggle_twin_is_suppressed_when_on/not_drawn", g_RuleDrawn, 0);
    checkInt("rule/toggle_twin_is_suppressed_when_on/object_never_resolved",
             g_AssetLookups[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Alpha_obj)], 0);
    for (int t = 0; t < ForgePact::kToggleSkillRowCount; ++t) g_ToggleTableIds.Set(t, -1);

    // R11. Two active rule entries keep separate latches - the same shape
    // skilltimer/rows_keep_separate_latches pins for the four explicit rows.
    resetWorld(); resetSkillTimer(); resetSkillTimerDrawRecord();
    resolveOnlyCountdownRow(-1, 0);
    resetRuleEntries({ { 910, kRuleAlphaIndex }, { 911, kRuleBetaIndex } });
    world.row0 = { { 910.0, 100.0, 200.0, 50.0, 60.0 }, { 911.0, 300.0, 200.0, 50.0, 60.0 } };
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Alpha_obj)] = 703.0;
    world.objIndexByName[HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Rule_Beta_obj)] = 704.0;
    world.instancesByIndex[703.0] = { WithTimer(OwnUnmarked(), MakeReal(200.0)) };
    world.instancesByIndex[704.0] = { WithTimer(OwnUnmarked(), MakeReal(400.0)) };
    g_SkillTimerStyle.store(ForgePact::SkillTimerStyle::Number);
    SkillTimerDraw();
    checkNear("rule/entries_keep_separate_latches/first", g_SkillTimerRuleEntries[0].state.latch, 200.0);
    checkNear("rule/entries_keep_separate_latches/second", g_SkillTimerRuleEntries[1].state.latch, 400.0);
    world.instancesByIndex[703.0] = { WithTimer(OwnUnmarked(), MakeReal(100.0)) };
    SkillTimerDraw();
    checkBool("rule/entries_keep_separate_latches",
              std::fabs(g_SkillTimerRuleEntries[0].state.latch - 200.0) < 1e-6
              && std::fabs(g_SkillTimerRuleEntries[1].state.latch - 400.0) < 1e-6, true);

    // The read never makes a player-resolving call, in any scenario above -
    // counted here, at the end, so it covers every one of them.
    checkInt("read/no_player_lookup", g_ResolveCalls, 0);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
