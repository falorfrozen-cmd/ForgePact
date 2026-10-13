#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ForgePact::JumpScenery {

// ---- jumpprobe's decision core (ForgePact #16, research build only) --------
//
// docs/jump-scenery-research.md asks how the game decides that the player's
// universal jump is blocked by scenery, and whether answering "no collision"
// to the player's own collision queries while airborne lets the jump cross a
// prop. ModuleMain.cpp's `jumpprobe pass` is the one lever that asks it: while
// it is on, a jump-script entry with the local player as self opens a window
// of `frames` frames (or, with `hold`, the window stays open while the lever
// is on), and inside that window a hooked collision builtin called by the
// local player against a scenery family is answered without running the
// game's own function.
//
// This header is that decision, and nothing else: the window arithmetic, the
// family rule (through an is-ancestor callback the adapter supplies - the
// game's object_is_ancestor, by name), the answer each builtin gets, and the
// counters `pass stat` prints. It is game-independent by contract - frame
// numbers, object indices and booleans, never an instance or an RValue - so
// tests/jump_scenery_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp supplies the frame number, whether a call's self is the
// local player, the object argument, the detours and the printing.
//
// The lever is deliberately blunt. It skips the landing check too, so the
// live session can measure what the game itself does with a landing point
// inside a prop - the question a shipped "valid landing" rule has to answer.

// What a hooked call gets back. RunOriginal is the only answer that runs the
// game's function; every other one is written into the result instead.
enum class Answer : int {
    RunOriginal,
    Noone,   // the instance-returning queries: no instance there
    False,   // the boolean meeting queries: nothing met
    Zero,    // tilemap_get_at_pixel: an empty tile
    True,    // place_free, and CanMove: the way is free
};

inline constexpr std::string_view AnswerName(Answer a)
{
    switch (a) {
    case Answer::RunOriginal: return "original";
    case Answer::Noone:       return "noone";
    case Answer::False:       return "false";
    case Answer::Zero:        return "0";
    case Answer::True:        return "true";
    }
    return "?";
}

// GameMaker's `noone`, the value an instance-returning query answers when it
// finds nothing.
inline constexpr double kNoone = -4.0;

// The builtin rows: the collision builtins the player's own Step code is
// measured to call every step, and the standard GML collision names no hook
// in this plugin has covered yet (docs/jump-scenery-research.md § Static
// search). `objectArg` is the position of the object argument, -1 for the two
// rows that take none: their answer cannot be decided by the family rule, so
// they are answered only under FamilyRule::All.
struct BuiltinRow {
    std::string_view name;
    int              objectArg;
    Answer           answer;
};

enum class Builtin : int {
    PositionMeeting, PlaceMeeting, PlaceFree, InstancePlace, InstancePosition,
    CollisionPoint, CollisionLine, CollisionRectangle, CollisionCircle, TilemapGetAtPixel,
};
inline constexpr int kBuiltinCount = 10;

inline constexpr BuiltinRow kBuiltins[kBuiltinCount] = {
    { "position_meeting",     2,  Answer::False },
    { "place_meeting",        2,  Answer::False },
    { "place_free",           -1, Answer::True  },
    { "instance_place",       2,  Answer::Noone },
    { "instance_position",    2,  Answer::Noone },
    { "collision_point",      2,  Answer::Noone },
    { "collision_line",       4,  Answer::Noone },
    { "collision_rectangle",  4,  Answer::Noone },
    { "collision_circle",     3,  Answer::Noone },
    { "tilemap_get_at_pixel", -1, Answer::Zero  },
};

// The `scripts` flag's three rows: the named collision scripts whose return
// the window rewrites as well, each to its own "no collision".
enum class ScriptLever : int { CanMove, InstancePlaceTallest, TilePlaceMeeting };
inline constexpr int kScriptLeverCount = 3;
inline constexpr Answer kScriptAnswers[kScriptLeverCount] = { Answer::True, Answer::Noone, Answer::False };

// Which objects the lever answers for. Props: Collision_Prop_obj or one of its
// descendants (the scenery). All: Collision_Parent_obj or a descendant (also
// the level's blocks, the map-edge walls and the zone gates), and the two
// object-less rows. Any (the walk lever, ForgePact #158): the local player's
// own query whatever object it names - outside Collision_Parent_obj, `all`,
// an unresolved argument - and the two object-less rows. A walk into the prop
// a jump crossed stayed blocked under `all`; `any` asks whether a query
// naming some other object is what stops it.
enum class FamilyRule : int { Props, All, Any };

inline constexpr std::string_view FamilyName(FamilyRule r)
{
    switch (r) {
    case FamilyRule::Props: return "props";
    case FamilyRule::All:   return "all";
    case FamilyRule::Any:   return "any";
    }
    return "?";
}

// The object tally (ForgePact #158): per builtin row, the local player's own
// calls counted by the object index they name (-1 for the object-less rows,
// and for an argument that is no object), whatever the lever does. At most
// kTallyMaxObjects distinct objects a row; the calls naming any further one
// are counted under `other`.
inline constexpr int kTallyMaxObjects = 64;

struct ObjectCount {
    int      object = -1;
    uint64_t n = 0;
};

struct RowTally {
    ObjectCount objects[kTallyMaxObjects]{};
    int         used = 0;    // distinct objects with a slot
    uint64_t    other = 0;   // calls naming an object past the cap
    uint64_t    total = 0;   // every counted call of the row
};

// `objects`' last line: the rows with a counted call, the distinct objects
// (index 0 or above) named across them, and how many of those lie outside
// Collision_Parent_obj's family.
struct TallySummary {
    int rows = 0;
    int distinct = 0;
    int outside = 0;
};

// Where one call went. The order of the checks is the order here, after
// Passed: the self first, then the lever, the window and the family.
enum class Outcome : int { Passed, Passthrough, OutsideWindow, OtherSelf, OtherFamily };

struct Counters {
    uint64_t passed = 0;          // answered without running the original
    uint64_t passthrough = 0;     // the local player's call while the lever (or its `scripts` flag) is off
    uint64_t outsideWindow = 0;   // the local player's call, lever on, no window open
    uint64_t otherSelf = 0;       // a call whose self is not the local player
    uint64_t otherFamily = 0;     // inside the window, but not an object the rule answers for
};

inline constexpr int64_t kDefaultFrames = 90;     // until the live session measures the airborne length
inline constexpr int64_t kMinFrames = 1;
inline constexpr int64_t kMaxFrames = 600;        // ten seconds: no jump lasts longer
inline constexpr int kTraceMaxLines = 600;        // `trace` lines in one session

class Probe {
public:
    // The adapter's is-ancestor question: is `ancestor` an ancestor of
    // `object`? It is asked once per object and family, then remembered -
    // an object's parent does not change while the game runs.
    using IsAncestorFn = std::function<bool(int object, int ancestor)>;

    void SetIsAncestor(IsAncestorFn fn) { isAncestor_ = std::move(fn); familyCache_.clear(); }

    // The two families, as object indices the adapter resolved by name; -1
    // when a name did not resolve, and the rule then answers for nothing.
    void SetFamilies(int propFamily, int allFamily)
    {
        propFamily_ = propFamily;
        allFamily_ = allFamily;
        familyCache_.clear();
    }
    int PropFamily() const { return propFamily_; }
    int AllFamily() const { return allFamily_; }

    // ---- what makes the probe do anything per call ----------------------
    // Armed (`arm`), tracing (`trace 1`), tallying (from `hook` on) or the
    // lever on: the adapter refreshes the local player and classifies calls
    // only then. Otherwise its detours only count calls, and its per-frame
    // tick returns at once.
    void SetArmed(bool on) { armed_ = on; }
    void SetTrace(bool on) { trace_ = on; }
    void SetTally(bool on) { tallyOn_ = on; }
    bool Armed() const { return armed_; }
    bool Tracing() const { return trace_; }
    bool Tallying() const { return tallyOn_; }
    bool Active() const { return armed_ || trace_ || tallyOn_ || lever_; }

    // ---- the lever -------------------------------------------------------
    // `pass 1 [frames] [props|all|any] [scripts] [hold]`: on, with every
    // counter zeroed and no window open. False, and nothing changed, for
    // frames out of range. The object tally is not one of those counters:
    // only `objects reset` clears it, so a walk counted with the lever off
    // can be compared with the same walk under it.
    //
    // `hold` keeps the window open for as long as the lever is on, with no
    // jump script at all. The static reading predicts that neither window
    // opener fires on the local keypress (CA_playerJump is the co-op relay,
    // PlayerForceJump is reached from a hit and a launcher), and a lever that
    // depends on them would then answer nothing while reporting itself ON.
    // `hold` is the lever's own positive control: walking into a prop with it
    // on proves that an answered builtin changes the player's movement.
    bool SetLever(int64_t frames, FamilyRule rule, bool scripts, bool hold = false)
    {
        if (frames < kMinFrames || frames > kMaxFrames) return false;
        lever_ = true;
        frames_ = frames;
        rule_ = rule;
        scripts_ = scripts;
        hold_ = hold;
        windowOpen_ = false;
        windowsOpened_ = 0;
        for (Counters& c : builtin_) c = Counters{};
        for (Counters& c : script_) c = Counters{};
        return true;
    }

    // `pass 0`: off, and any window closed. The counters stay for `pass stat`.
    void LeverOff()
    {
        lever_ = false;
        hold_ = false;
        windowOpen_ = false;
    }

    bool LeverOn() const { return lever_; }
    bool ScriptsOn() const { return lever_ && scripts_; }
    bool Holding() const { return lever_ && hold_; }
    int64_t Frames() const { return frames_; }
    FamilyRule Rule() const { return rule_; }

    // A jump script's entry (CA_playerJump, PlayerForceJump). Opens - or
    // restarts - the window at `frame` only while the lever is on and only
    // for the local player's own jump; true when it did. Under `hold` the
    // entry is still counted (windows-opened= is then J1's evidence that an
    // opener fired), and the window stays held either way.
    bool OnJumpEntry(int64_t frame, bool playerSelf)
    {
        if (!lever_ || !playerSelf) return false;
        windowOpen_ = true;
        windowStart_ = frame;
        ++windowsOpened_;
        return true;
    }

    // Open on frames start .. start + frames - 1, closed from start + frames;
    // open on every frame while held.
    bool WindowOpen(int64_t frame) const
    {
        return lever_ && (hold_ || InJumpWindow(frame));
    }

    // The per-frame tick: a window whose last frame has passed is closed, so
    // `pass stat` reads it as closed rather than as open with a stale start.
    void Tick(int64_t frame)
    {
        if (windowOpen_ && !InJumpWindow(frame)) windowOpen_ = false;
    }

    bool WindowRecorded() const { return windowOpen_; }
    int64_t WindowStart() const { return windowStart_; }
    uint64_t WindowsOpened() const { return windowsOpened_; }

    // Every row's outside-window= count, builtins and scripts together.
    uint64_t OutsideWindowTotal() const
    {
        uint64_t n = 0;
        for (const Counters& c : builtin_) n += c.outsideWindow;
        for (const Counters& c : script_) n += c.outsideWindow;
        return n;
    }

    // The lever is on and the local player's calls arrive, but no jump script
    // ever opened a window for them: every answer so far was "run the
    // original". `pass stat` and `show` name this state, so a lever that
    // answers nothing is never read as a lever that changed nothing.
    bool Inert() const
    {
        return lever_ && !hold_ && windowsOpened_ == 0 && OutsideWindowTotal() > 0;
    }

    // ---- the decisions ---------------------------------------------------
    // One hooked builtin call. `objectOf()` returns the object index of the
    // call's object argument (-1 when it is not one) and is called at most
    // once, only for the local player's call on a row that takes an object,
    // and only when the tally counts it or the answer depends on it (inside
    // the window, under `props` or `all`) - so a call nothing reads costs no
    // read of its arguments.
    template <class ObjectFn>
    Answer DecideBuiltin(Builtin row, int64_t frame, bool playerSelf, ObjectFn&& objectOf)
    {
        const int i = static_cast<int>(row);
        if (i < 0 || i >= kBuiltinCount) return Answer::RunOriginal;
        Counters& c = builtin_[i];
        const BuiltinRow& b = kBuiltins[i];
        if (!playerSelf) { ++c.otherSelf; return Answer::RunOriginal; }
        int object = -1;
        bool objectRead = false;
        const auto readObject = [&]() {
            if (!objectRead) { object = objectOf(); objectRead = true; }
            return object;
        };
        if (tallyOn_) TallyAdd(i, b.objectArg < 0 ? -1 : readObject());
        if (!lever_) { ++c.passthrough; return Answer::RunOriginal; }
        if (!WindowOpen(frame)) { ++c.outsideWindow; return Answer::RunOriginal; }
        bool answers = false;
        if (rule_ == FamilyRule::Any) {
            answers = true;
        } else if (b.objectArg < 0) {
            answers = rule_ == FamilyRule::All;
        } else {
            const int family = rule_ == FamilyRule::All ? allFamily_ : propFamily_;
            answers = InFamily(readObject(), family);
        }
        if (!answers) { ++c.otherFamily; return Answer::RunOriginal; }
        ++c.passed;
        return b.answer;
    }

    // One call of a `scripts` row. Answered only with the `scripts` flag, for
    // the local player, inside the window; no family rule (no object argument).
    Answer DecideScript(ScriptLever row, int64_t frame, bool playerSelf)
    {
        const int i = static_cast<int>(row);
        if (i < 0 || i >= kScriptLeverCount) return Answer::RunOriginal;
        Counters& c = script_[i];
        if (!playerSelf) { ++c.otherSelf; return Answer::RunOriginal; }
        if (!lever_ || !scripts_) { ++c.passthrough; return Answer::RunOriginal; }
        if (!WindowOpen(frame)) { ++c.outsideWindow; return Answer::RunOriginal; }
        ++c.passed;
        return kScriptAnswers[i];
    }

    const Counters& BuiltinCounters(Builtin row) const { return builtin_[static_cast<int>(row)]; }
    const Counters& ScriptCounters(ScriptLever row) const { return script_[static_cast<int>(row)]; }

    // Is `object` the family's own object or one of its descendants? Unknown
    // objects (-1), an unresolved family and a missing callback answer no:
    // the lever then runs the original, which is the vanilla game.
    bool InFamily(int object, int family)
    {
        if (object < 0 || family < 0) return false;
        if (object == family) return true;
        if (!isAncestor_) return false;
        const int64_t key = (static_cast<int64_t>(object) << 32) | static_cast<uint32_t>(family);
        const auto it = familyCache_.find(key);
        if (it != familyCache_.end()) return it->second;
        bool yes = false;
        try { yes = isAncestor_(object, family); } catch (...) { yes = false; }
        familyCache_.emplace(key, yes);
        return yes;
    }

    // ---- the object tally ------------------------------------------------
    const RowTally& TallyOf(Builtin row) const { return tally_[static_cast<int>(row)]; }

    // `objects reset`: every row's tally cleared. Whether it counts is
    // unchanged.
    void TallyReset() { for (RowTally& t : tally_) t = RowTally{}; }

    // A row's objects, the most-called first (ties by object index).
    std::vector<ObjectCount> TallySorted(Builtin row) const
    {
        const RowTally& t = TallyOf(row);
        std::vector<ObjectCount> out(t.objects, t.objects + t.used);
        std::sort(out.begin(), out.end(), [](const ObjectCount& a, const ObjectCount& b) {
            return a.n != b.n ? a.n > b.n : a.object < b.object;
        });
        return out;
    }

    // `objects` labels an object `family` when it is Collision_Parent_obj or
    // one of its descendants (what `all` answers for), `outside` otherwise;
    // -1 names no object and is never family.
    bool ObjectInFamily(int object) { return InFamily(object, allFamily_); }

    TallySummary TallyTotals()
    {
        TallySummary totals;
        std::vector<int> seen;
        for (const RowTally& t : tally_) {
            if (t.total > 0) ++totals.rows;
            for (int k = 0; k < t.used; ++k) {
                const int object = t.objects[k].object;
                if (object < 0 || std::find(seen.begin(), seen.end(), object) != seen.end()) continue;
                seen.push_back(object);
                ++totals.distinct;
                if (!ObjectInFamily(object)) ++totals.outside;
            }
        }
        return totals;
    }

    // ---- the trace's session cap -----------------------------------------
    // One `trace` line may be written; false once kTraceMaxLines were.
    bool TakeTraceLine()
    {
        if (traceLines_ >= kTraceMaxLines) return false;
        ++traceLines_;
        return true;
    }
    int TraceLines() const { return traceLines_; }

private:
    bool InJumpWindow(int64_t frame) const
    {
        return windowOpen_ && frame >= windowStart_ && frame - windowStart_ < frames_;
    }

    void TallyAdd(int row, int object)
    {
        RowTally& t = tally_[row];
        ++t.total;
        for (int k = 0; k < t.used; ++k)
            if (t.objects[k].object == object) { ++t.objects[k].n; return; }
        if (t.used < kTallyMaxObjects) { t.objects[t.used++] = ObjectCount{ object, 1 }; return; }
        ++t.other;
    }

    IsAncestorFn isAncestor_;
    int propFamily_ = -1;
    int allFamily_ = -1;
    std::unordered_map<int64_t, bool> familyCache_;
    bool armed_ = false;
    bool trace_ = false;
    bool tallyOn_ = false;
    bool lever_ = false;
    bool scripts_ = false;
    bool hold_ = false;
    FamilyRule rule_ = FamilyRule::Props;
    int64_t frames_ = kDefaultFrames;
    bool windowOpen_ = false;
    int64_t windowStart_ = 0;
    uint64_t windowsOpened_ = 0;
    Counters builtin_[kBuiltinCount]{};
    Counters script_[kScriptLeverCount]{};
    RowTally tally_[kBuiltinCount]{};
    int traceLines_ = 0;
};

} // namespace ForgePact::JumpScenery
