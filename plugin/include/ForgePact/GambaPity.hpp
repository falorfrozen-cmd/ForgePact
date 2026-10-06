#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ForgePact::GambaPity {

// ---- gambapity's decision core (ForgePact #134 phase 6, player build) -------
//
// The Mods -> Quality of Life switch `gambapity` guarantees Goburin's Head
// (the unique charm at repository type 10 / sub 0 / base 98, key
// `charms_goburins_head`) from the gamba machine (Slot_Machine_01_obj): it
// counts the machine explosions that did not drop a head, and the explosion
// that brings the count to the configured number drops exactly one head and
// starts the count over. ModuleMain.cpp's adapter feeds this header the
// measured events and carries out what it decides:
//
//   - a machine seen in a frame, by id, and whether its sprite is the
//     destroyed one. Live 4 measured the explosion as the machine's sprite
//     changing to Slot_Machine_01_Destroyed_spr with the instance kept, so a
//     live-to-destroyed change is one explosion, and a machine already
//     destroyed at first sight is none. While on, an explosion adds one to the
//     count the moment it is seen, so one abandoned by a room change still
//     counts;
//   - a Goburin's Head build (CreateItemNew returned the charm, any self),
//     and a machine-self CreateDefaultParams (0, 98);
//   - at the explosion's deadline, the ground heads near the machine.
//
// Spins and payouts are not an input, so neither can count, force or reset.
// The decision is made kSettleFrames presented frames after the sprite
// change, in this order: the room changed -> abandoned (count kept); a
// natural-head signal in the window -> natural (no force, at any count: this
// explosion's addition and every one before it leave the count); on and this
// explosion standing at the threshold in the count (its own count: the
// additions up to and including its own) -> force (a confirmed drop takes
// this explosion's addition and every one before it out of the count,
// keeping a later pending explosion's, which then forces on its own count if
// that is still at the threshold - exactly one head per explosion; a refused
// one keeps the count so the next explosion forces), unless the ground near the machine did not read, now or
// at its first sight -> ground-unread (refused, count kept: a second head is
// worse than a late one); otherwise below (count kept).
//
// The counter file's text, both ways (CounterFileText, ParseCounterFile), is
// here too: version 2 holds the explosion count, and an unversioned file is
// the older spin count, read as 0.
//
// It is game-independent by contract - numbers, strings, frame numbers and
// machine/instance ids, never an instance, an RValue or a game constant - so
// tests/gamba_pity_harness.cpp compiles it whole, the way GambaProbe.hpp is.
// The adapter supplies the machine-self predicate, the sprite comparison (by
// name), the charm identity and the file I/O.

// The settle span: the decision waits this many presented frames after the
// sprite change, so a head the game builds after the change can appear first.
inline constexpr int64_t kSettleFrames = 60;
// The look-back: a head build this many frames before the sprite change still
// belongs to the explosion.
inline constexpr int64_t kLookBackFrames = 30;
// The ground check's radius around the machine, in room pixels.
inline constexpr double kGroundRadius = 256.0;
// A machine whose first-sight ground scan did not read is scanned again, at
// most once per this many frames, while it is still live.
inline constexpr int64_t kBaselineRetryFrames = 30;

// What a machine sighting was.
enum class Sighting { None, FirstSeen, Exploded };

// What an explosion's deadline decided.
// GroundUnread: the count is at the threshold, but the ground near the machine
// (now, or at its first sight) could not be read, so a head the game placed
// could not be ruled out: the force is refused and the counter kept, since a
// second head is worse than a late one.
enum class Outcome { Abandoned, Natural, Force, Below, GroundUnread };

// One explosion awaiting its deadline.
struct Explosion {
    int64_t id = -1;          // the machine's instance id
    int64_t frame = 0;        // the frame the destroyed sprite was first seen
    int64_t deadline = 0;     // frame + kSettleFrames
    int64_t room = -1;        // the room at the sprite change
    double x = 0.0, y = 0.0;  // the machine's position at the sprite change
    std::string signal;       // "build" / "machine-build" once one fell in the window
    int countAfter = 0;       // the count its own addition brought the counter to; 0 when it did not count
    int64_t addSeq = 0;       // its addition's place among this session's additions (1, 2, ...); 0 when it did not count
};

// The deadline's decision for one explosion.
struct Decision {
    Outcome outcome = Outcome::Below;
    std::string signal;       // the natural outcome's signal: ground, build or machine-build
    bool groundUnread = false; // a below outcome whose ground scan (or baseline) did not read
};

class Pity {
public:
    // ---- the state the panel's switch and range set -------------------------
    // `gambapity <count>`: on with `count` as the threshold. `gambapity off`
    // keeps the counter; a forced drop or a natural head resets it.
    void SetEnabled(bool on) { enabled_ = on; }
    bool Enabled() const { return enabled_; }

    void SetThreshold(int threshold) { threshold_ = threshold; }
    int Threshold() const { return threshold_; }

    // The current count: the explosions without a head since the last reset.
    int Count() const { return count_; }

    // Restore the persisted count (the adapter reads the counter file through
    // ParseCounterFile; this core holds no file I/O).
    void SetCount(int count) { count_ = count > 0 ? count : 0; }

    // A natural charm build with no explosion of its own - the machine-self
    // CreateDefaultParams (0, 98) - starts the guarantee over, at any count:
    // every addition so far goes.
    void OnNaturalDrop()
    {
        count_ = 0;
        clearedThrough_ = added_;
    }

    // Where explosion `e` stands in the count now: the count less the
    // additions made after it. An explosion that did not count, or whose
    // addition a reset already took, stands at 0 or below.
    int64_t Position(const Explosion& e) const
    {
        if (e.addSeq <= 0 || e.addSeq <= clearedThrough_) return 0;
        return static_cast<int64_t>(count_) - (added_ - e.addSeq);
    }

    // `gambapity off`: disarmed, the counter kept. The machine records and any
    // pending explosion go too, so turning it on again starts every machine
    // from a fresh first sight.
    void Off()
    {
        enabled_ = false;
        machines_.clear();
        pending_.clear();
        recentBuilds_.clear();
    }

    // ---- the room ------------------------------------------------------------
    // Called once a frame before the machines. A room change clears the
    // machine records (a pending explosion keeps its own room, and its
    // deadline abandons it). Returns whether the room changed.
    bool OnRoom(int64_t room)
    {
        if (room == room_) return false;
        room_ = room;
        machines_.clear();
        recentBuilds_.clear();
        return true;
    }

    // ---- the machine watch ---------------------------------------------------
    // One machine read in frame `frame`. First sight records the machine and
    // returns FirstSeen (the adapter then takes its ground baseline); a later
    // live-to-destroyed change registers one pending explosion and returns
    // Exploded, and while on adds one to the count at once (the explosion line
    // then shows the count with it). A machine already destroyed at first
    // sight never explodes.
    Sighting ObserveMachine(int64_t id, bool destroyed, int64_t frame, double x, double y)
    {
        auto it = machines_.find(id);
        if (it == machines_.end()) {
            machines_[id] = Machine{ destroyed, false, frame + kBaselineRetryFrames, {} };
            ++machinesSeen_;
            return Sighting::FirstSeen;
        }
        if (it->second.destroyed || !destroyed) return Sighting::None;
        it->second.destroyed = true;
        Explosion e;
        e.id = id;
        e.frame = frame;
        e.deadline = frame + kSettleFrames;
        e.room = room_;
        e.x = x;
        e.y = y;
        // A head build in the look-back belongs to this explosion.
        for (const auto& b : recentBuilds_)
            if (b.first >= frame - kLookBackFrames && e.signal.empty()) e.signal = b.second;
        // Counted at detection, so an explosion a room change abandons still
        // counts. Off, nothing counts.
        if (enabled_) {
            ++count_;
            e.countAfter = count_;
            e.addSeq = ++added_;
        }
        pending_.push_back(e);
        ++explosions_;
        return Sighting::Exploded;
    }

    // The ground heads (Loot_Ground_obj instance ids holding the charm) near a
    // machine at its first sight: the baseline a later head is compared with.
    // `read` false: the scan did not read, so the baseline is unknown and,
    // until a later scan reads (NeedsBaseline), the machine's explosion cannot
    // be forced (Decide answers GroundUnread). An unread scan never replaces a
    // baseline that read.
    void SetBaseline(int64_t id, const std::vector<int64_t>& heads, bool read = true)
    {
        auto it = machines_.find(id);
        if (it == machines_.end() || (it->second.baselineRead && !read)) return;
        it->second.baseline = std::set<int64_t>(heads.begin(), heads.end());
        it->second.baselineRead = read;
    }

    // Should the adapter scan the ground again for this machine's baseline at
    // `frame`? Only while the machine is still live (a scan after its
    // explosion could take the explosion's own head for the baseline), its
    // baseline has not read, and kBaselineRetryFrames have passed since the
    // last try; a true answer books the next try.
    bool NeedsBaseline(int64_t id, int64_t frame)
    {
        auto it = machines_.find(id);
        if (it == machines_.end() || it->second.destroyed || it->second.baselineRead) return false;
        if (frame < it->second.nextBaselineTry) return false;
        it->second.nextBaselineTry = frame + kBaselineRetryFrames;
        return true;
    }

    bool BaselineRead(int64_t id) const
    {
        auto it = machines_.find(id);
        return it != machines_.end() && it->second.baselineRead;
    }

    // A machine instance the watch found but could not read (its id, sprite or
    // position): skipped this frame, and counted so the skip is never silent.
    void NoteMachineUnread() { ++machinesUnread_; }

    // The heads in `heads` that were not in the machine's baseline.
    int NewHeads(int64_t id, const std::vector<int64_t>& heads) const
    {
        auto it = machines_.find(id);
        int n = 0;
        for (int64_t h : heads)
            if (it == machines_.end() || !it->second.baseline.count(h)) ++n;
        return n;
    }

    // ---- head builds ---------------------------------------------------------
    // The forced drop runs inside an own-drop scope: a head build inside it is
    // our own, counted as own-head-builds (the build detector's control),
    // never as a signal.
    void BeginOwnDrop() { ownDrop_ = true; }
    void EndOwnDrop() { ownDrop_ = false; }
    bool InOwnDrop() const { return ownDrop_; }

    // A CreateItemNew whose returned item is the charm, any self. Inside a
    // pending explosion's window it makes that explosion natural; outside
    // every window it changes nothing, since Item Truth's menu builds and the
    // Angelic pool also build the charm. Returns whether it was our own.
    bool OnHeadBuild(int64_t frame) { return NoteBuild(frame, "build"); }

    // A machine-self CreateDefaultParams (0, 98): resets the counter at once
    // (the caller logs it), and is a signal for any explosion whose window
    // holds it.
    void OnMachineCharmBuild(int64_t frame)
    {
        OnNaturalDrop();
        NoteBuild(frame, "machine-build");
    }

    // ---- the deadline --------------------------------------------------------
    // The pending explosions whose deadline has come, removed from the
    // pending list in the order they exploded. Each is then decided.
    std::vector<Explosion> TakeDue(int64_t frame)
    {
        std::vector<Explosion> due;
        std::vector<Explosion> keep;
        for (const Explosion& e : pending_) (e.deadline <= frame ? due : keep).push_back(e);
        pending_.swap(keep);
        // The look-back only needs builds this recent.
        recentBuilds_.erase(std::remove_if(recentBuilds_.begin(), recentBuilds_.end(),
            [frame](const std::pair<int64_t, std::string>& b) { return b.first < frame - kLookBackFrames; }),
            recentBuilds_.end());
        return due;
    }

    // Decide one due explosion, at the point of use: `room` is the room now,
    // `ground` the charm heads lying near the machine now, and `groundRead`
    // whether that scan read to the end. Abandoned, natural, below and
    // ground-unread are final here; a force waits for ForceConfirmed or
    // ForceRefused. A force needs this explosion to stand at the threshold
    // in the count (Position: an explosion still pending never forces on a
    // later one's addition) and both ground reads - the machine's baseline
    // and this one - since only they can rule out a head the game placed.
    // Each explosion is decided on its own count: explosions pending
    // together can each force (at threshold 1, three give three heads), one
    // head per explosion. A natural outcome takes this explosion's addition
    // and every one before it out of the count; a later pending explosion's
    // addition stays.
    Decision Decide(const Explosion& e, int64_t room, const std::vector<int64_t>& ground, bool groundRead = true)
    {
        Decision d;
        if (room != e.room) {
            ++abandoned_;
            d.outcome = Outcome::Abandoned;
            return d;
        }
        auto it = machines_.find(e.id);
        const bool baselineRead = it != machines_.end() && it->second.baselineRead;
        int fresh = 0;
        if (groundRead && baselineRead)
            for (int64_t h : ground)
                if (!ownHeads_.count(h) && !it->second.baseline.count(h)) ++fresh;
        if (fresh > 0 || !e.signal.empty()) {
            d.signal = fresh > 0 ? std::string("ground") : e.signal;
            d.outcome = Outcome::Natural;
            ResetThrough(e);
            ++natural_;
            return d;
        }
        if (enabled_ && threshold_ > 0 && Position(e) >= threshold_) {
            if (!groundRead || !baselineRead) {
                ++refused_;
                ++groundUnread_;
                d.outcome = Outcome::GroundUnread;
                return d;
            }
            d.outcome = Outcome::Force;
            return d;
        }
        ++below_;
        // Below with a ground that did not read: the natural ground signal was
        // not checked, so say so (BelowLine's tag) and count it.
        if (!groundRead || !baselineRead) {
            d.groundUnread = true;
            ++belowGroundUnread_;
        }
        d.outcome = Outcome::Below;
        return d;
    }

    // The forced head was placed and read back (its ground instance id, kept
    // so a later explosion's ground check does not take it for natural): the
    // count drops by this explosion's standing - its addition and every one
    // before it - so a later pending explosion's addition is kept.
    void ForceConfirmed(const Explosion& e, int64_t groundId)
    {
        ResetThrough(e);
        ++forced_;
        if (groundId >= 0) ownHeads_.insert(groundId);
    }

    // The forced drop was refused: the count is kept, so the next explosion's
    // addition is past the threshold and it tries again.
    void ForceRefused() { ++refused_; }

    size_t Pending() const { return pending_.size(); }
    int Explosions() const { return explosions_; }
    int Forced() const { return forced_; }
    int Natural() const { return natural_; }
    int Below() const { return below_; }
    int Refused() const { return refused_; }
    int Abandoned() const { return abandoned_; }
    int OwnHeadBuilds() const { return ownHeadBuilds_; }
    int MachinesSeen() const { return machinesSeen_; }
    int MachinesUnread() const { return machinesUnread_; }
    int GroundUnread() const { return groundUnread_; }
    int BelowGroundUnread() const { return belowGroundUnread_; }

    // ---- the lines gambapity prints (fixed text; the contract test and the
    // live procedure read them) ------------------------------------------------
    std::string StatusLine() const
    {
        return std::string("gambapity: ") + (enabled_ ? "on" : "off")
            + " count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_)
            + " explosions=" + std::to_string(explosions_)
            + " forced=" + std::to_string(forced_)
            + " natural=" + std::to_string(natural_)
            + " below=" + std::to_string(below_)
            + " refused=" + std::to_string(refused_)
            + " abandoned=" + std::to_string(abandoned_)
            + " own-head-builds=" + std::to_string(ownHeadBuilds_)
            + " machines=" + std::to_string(machinesSeen_)
            + " unread=" + std::to_string(machinesUnread_)
            + " ground-unread=" + std::to_string(groundUnread_)
            + " below-ground-unread=" + std::to_string(belowGroundUnread_);
    }

    // `headsNearby` is the baseline's count, or "unread (<stage>)" when the
    // first-sight scan did not read.
    static std::string MachineSeenLine(int64_t id, const std::string& sprite, const std::string& headsNearby)
    {
        return "gambapity: machine id=" + std::to_string(id) + " seen sprite=" + sprite
            + " heads-nearby=" + headsNearby;
    }

    std::string ExplosionLine(int64_t id, int64_t frame) const
    {
        return "gambapity: explosion id=" + std::to_string(id) + " count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_) + " frame=" + std::to_string(frame);
    }

    static std::string ForcedLine(double x, double y, int rarity, int attempt)
    {
        return "gambapity: forced Goburin's Head at " + std::to_string(std::llround(x)) + ","
            + std::to_string(std::llround(y)) + " (rarity " + std::to_string(rarity)
            + ", attempt " + std::to_string(attempt) + ") and reset the counter";
    }

    static std::string GroundAfterDropLine(int heads)
    {
        return "gambapity: ground check after the drop: heads=" + std::to_string(heads);
    }

    // The after-drop scan did not read: its stage, never a count.
    static std::string GroundAfterDropUnreadLine(const std::string& stage)
    {
        return "gambapity: ground check after the drop: unread (" + stage + ")";
    }

    static std::string NaturalSeenLine(const std::string& signal)
    {
        return "gambapity: the explosion's own Goburin's Head was seen (" + signal + "); no force, counter reset";
    }

    // The `count=` is the explosion's own standing (Position), the number it
    // was decided on, not the counter: a later pending explosion's addition
    // is not its own. `unreadStage` non-empty: the ground (or the baseline)
    // did not read, so the natural ground signal was not checked, and the
    // line says why.
    std::string BelowLine(const Explosion& e, const std::string& unreadStage = std::string()) const
    {
        return "gambapity: explosion below the threshold (count=" + std::to_string(std::max<int64_t>(0, Position(e)))
            + " threshold=" + std::to_string(threshold_) + "); counter kept"
            + (unreadStage.empty() ? std::string() : "; ground unread (" + unreadStage + ")");
    }

    // A baseline retry that read, for a machine whose first-sight scan did not.
    static std::string BaselineReadLine(int64_t id, int headsNearby)
    {
        return "gambapity: machine id=" + std::to_string(id) + " baseline read heads-nearby=" + std::to_string(headsNearby);
    }

    static std::string RefusedLine(const std::string& stage)
    {
        return "gambapity: forced drop refused - " + stage + "; counter kept";
    }

    static std::string AbandonedLine(int64_t id)
    {
        return "gambapity: explosion id=" + std::to_string(id) + " abandoned (room changed); counter kept";
    }

    static std::string NaturalBuildLine()
    {
        return "gambapity: a natural Goburin's Head build reset the counter";
    }

    // An unversioned counter file (the older spin count) was read as 0; the
    // adapter prints this once and rewrites the file as version 2.
    static std::string MigrationLine()
    {
        return "gambapity: the counter file held a spin count from an older version; the explosion count starts at 0";
    }

    // The status line's error suffix for a counter file of another version.
    static std::string VersionErrorText(int64_t version)
    {
        return "the gambapity counter file has version " + std::to_string(version)
            + ", which this build does not read; the explosion count starts at 0";
    }

private:
    struct Machine {
        bool destroyed = false;
        bool baselineRead = false;    // a baseline scan read to the end
        int64_t nextBaselineTry = 0;  // the frame of the next baseline retry while unread
        std::set<int64_t> baseline;   // charm heads near it at first sight
    };

    // Take explosion `e`'s addition and every one before it out of the count
    // (the count keeps only the additions after it). Nothing when a reset
    // already took it; an explosion that did not count resets everything.
    void ResetThrough(const Explosion& e)
    {
        if (e.addSeq <= 0) {
            OnNaturalDrop();
            return;
        }
        if (e.addSeq <= clearedThrough_) return;
        const int64_t after = added_ - e.addSeq;
        count_ = static_cast<int>(std::max<int64_t>(0, std::min<int64_t>(count_, after)));
        clearedThrough_ = e.addSeq;
    }

    // A head build at `frame`: our own inside the own-drop scope, else a
    // signal for every pending explosion whose window holds it, and kept for
    // the look-back of an explosion seen within kLookBackFrames.
    bool NoteBuild(int64_t frame, const char* signal)
    {
        if (ownDrop_) {
            ++ownHeadBuilds_;
            return true;
        }
        for (Explosion& e : pending_)
            if (frame >= e.frame - kLookBackFrames && frame <= e.deadline && e.signal.empty()) e.signal = signal;
        recentBuilds_.emplace_back(frame, signal);
        return false;
    }

    bool enabled_ = false;
    int threshold_ = 0;
    int count_ = 0;
    int64_t added_ = 0;           // additions this session (each counted explosion's addSeq)
    int64_t clearedThrough_ = 0;  // the last addSeq a reset took out of the count
    int64_t room_ = -1;
    bool ownDrop_ = false;
    std::map<int64_t, Machine> machines_;
    std::vector<Explosion> pending_;
    std::vector<std::pair<int64_t, std::string>> recentBuilds_;
    std::set<int64_t> ownHeads_;
    int explosions_ = 0;
    int forced_ = 0;
    int natural_ = 0;
    int below_ = 0;
    int refused_ = 0;
    int abandoned_ = 0;
    int ownHeadBuilds_ = 0;
    int machinesSeen_ = 0;     // machines seen for the first time (cumulative)
    int machinesUnread_ = 0;   // machine reads skipped (an id, sprite or position that did not read)
    int groundUnread_ = 0;     // forces refused because a ground scan did not read
    int belowGroundUnread_ = 0; // below outcomes whose ground scan (or baseline) did not read
};

// ---- the counter file --------------------------------------------------------
// %LOCALAPPDATA%\Hero_Siege\forgepact_gamba_pity.json holds the count. The
// adapter reads and writes the file; the text is this header's, both ways.
// Version 2 is the explosion count. The phase-5 file, `{"count":<n>}` with no
// version, held spins and counts as version 1.
inline constexpr int64_t kCounterFileVersion = 2;

// Exactly `{"version":2,"count":<n>}`: no spaces, no newline.
inline std::string CounterFileText(int count)
{
    return "{\"version\":" + std::to_string(kCounterFileVersion) + ",\"count\":" + std::to_string(count > 0 ? count : 0) + "}";
}

// What a counter file's text held. `legacy`: an unversioned spin count, read
// as 0 (the adapter prints MigrationLine once and rewrites the file).
// `unknown`: another version, read as 0 (the adapter shows VersionErrorText
// and leaves the file until the count next changes). A missing, empty or
// unparseable text is 0 with neither flag.
struct CounterFile {
    int count = 0;
    bool legacy = false;
    bool unknown = false;
    int64_t version = 0;   // the unknown version's number
};

namespace detail {
inline bool CounterFileSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// An integer member `"<key>":<n>` of a one-level JSON object. `present` when
// the key is there; true when its value is a whole number of at most nine
// digits followed by `,` or `}` (white space allowed around it).
inline bool CounterFileInt(const std::string& text, const std::string& key, bool& present, int64_t& value)
{
    const std::string quoted = "\"" + key + "\"";
    size_t i = text.find(quoted);
    present = i != std::string::npos;
    if (!present) return false;
    i += quoted.size();
    while (i < text.size() && CounterFileSpace(text[i])) ++i;
    if (i >= text.size() || text[i] != ':') return false;
    ++i;
    while (i < text.size() && CounterFileSpace(text[i])) ++i;
    bool negative = false;
    if (i < text.size() && text[i] == '-') { negative = true; ++i; }
    const size_t start = i;
    int64_t n = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9' && i - start < 9) n = n * 10 + (text[i++] - '0');
    if (i == start || (i < text.size() && text[i] >= '0' && text[i] <= '9')) return false;
    while (i < text.size() && CounterFileSpace(text[i])) ++i;
    if (i >= text.size() || (text[i] != ',' && text[i] != '}')) return false;
    value = negative ? -n : n;
    return true;
}
} // namespace detail

inline CounterFile ParseCounterFile(const std::string& text)
{
    CounterFile file;
    size_t first = 0, last = text.size();
    while (first < last && detail::CounterFileSpace(text[first])) ++first;
    while (last > first && detail::CounterFileSpace(text[last - 1])) --last;
    if (last - first < 2 || text[first] != '{' || text[last - 1] != '}') return file;
    bool hasVersion = false, hasCount = false;
    int64_t version = 0, count = 0;
    const bool versionRead = detail::CounterFileInt(text, "version", hasVersion, version);
    const bool countRead = detail::CounterFileInt(text, "count", hasCount, count);
    if (hasVersion) {
        if (!versionRead) return file;   // a version that is not a number: unparseable
        if (version != kCounterFileVersion) {
            file.unknown = true;
            file.version = version;
            return file;
        }
        if (countRead && count > 0) file.count = static_cast<int>(count);
        return file;
    }
    if (countRead) file.legacy = true;   // the older spin count, never read as explosions
    return file;
}

} // namespace ForgePact::GambaPity
