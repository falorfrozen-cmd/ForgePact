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

// ---- gambapity's decision core (ForgePact #134 phase 5, player build) -------
//
// The Mods -> Quality of Life switch `gambapity` guarantees Goburin's Head
// (the unique charm at repository type 10 / sub 0 / base 98, key
// `charms_goburins_head`) from the gamba machine (Slot_Machine_01_obj): the
// first machine that explodes after the configured number of spins drops
// exactly one head, and the counter starts over. ModuleMain.cpp's adapter feeds
// this header the measured events and carries out what it decides:
//
//   - a spin: one machine-self PickUpGoldCheck with a1=-10000 (counts one);
//   - a machine seen in a frame, by id, and whether its sprite is the
//     destroyed one. Live 4 measured the explosion as the machine's sprite
//     changing to Slot_Machine_01_Destroyed_spr with the instance kept, so a
//     live-to-destroyed change is one explosion, and a machine already
//     destroyed at first sight is none;
//   - a Goburin's Head build (CreateItemNew returned the charm, any self),
//     and a machine-self CreateDefaultParams (0, 98);
//   - at the explosion's deadline, the ground heads near the machine.
//
// Payouts are not an input, so no payout can force or reset. The decision is
// made kSettleFrames presented frames after the sprite change, in this order:
// the room changed -> abandoned (counter kept); a natural-head signal in the
// window -> natural (no force, counter reset, at any count); on and the count
// at the threshold -> force (a confirmed drop resets, a refused one keeps the
// counter so the next explosion forces); otherwise below (counter kept).
//
// It is game-independent by contract - numbers, strings, frame numbers and
// machine/instance ids, never an instance, an RValue or a game constant - so
// tests/gamba_pity_harness.cpp compiles it whole, the way GambaProbe.hpp is.
// The adapter supplies the machine-self predicate, the sprite comparison (by
// name) and the charm identity; this core holds no file I/O.

// One spin debits exactly 10,000 gold, so the counter - spins - reads as gold.
inline constexpr int64_t kGoldPerSpin = 10000;
// The settle span: the decision waits this many presented frames after the
// sprite change, so a head the game builds after the change can appear first.
inline constexpr int64_t kSettleFrames = 60;
// The look-back: a head build this many frames before the sprite change still
// belongs to the explosion.
inline constexpr int64_t kLookBackFrames = 30;
// The ground check's radius around the machine, in room pixels.
inline constexpr double kGroundRadius = 256.0;

// What a machine sighting was.
enum class Sighting { None, FirstSeen, Exploded };

// What an explosion's deadline decided.
enum class Outcome { Abandoned, Natural, Force, Below };

// One explosion awaiting its deadline.
struct Explosion {
    int64_t id = -1;          // the machine's instance id
    int64_t frame = 0;        // the frame the destroyed sprite was first seen
    int64_t deadline = 0;     // frame + kSettleFrames
    int64_t room = -1;        // the room at the sprite change
    double x = 0.0, y = 0.0;  // the machine's position at the sprite change
    std::string signal;       // "build" / "machine-build" once one fell in the window
};

// The deadline's decision for one explosion.
struct Decision {
    Outcome outcome = Outcome::Below;
    std::string signal;       // the natural outcome's signal: ground, build or machine-build
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

    // The current spin count: the spins seen since the last reset.
    int Count() const { return count_; }

    // Restore the persisted count (the adapter reads the counter file; this
    // core holds no file I/O).
    void SetCount(int count) { count_ = count > 0 ? count : 0; }

    // The gold equivalent of the count, one spin = kGoldPerSpin gold. Named
    // once here so the status line shares it (the panel's value box shows the
    // spin count, not the gold).
    int64_t GoldEquivalent() const { return static_cast<int64_t>(count_) * kGoldPerSpin; }

    // ---- spins ---------------------------------------------------------------
    // A spin: one machine-self PickUpGoldCheck with a1=-10000. Counts only
    // while on and machine-self; returns whether it counted. Nothing but a
    // spin raises the counter.
    bool OnSpin(bool machineSelf)
    {
        if (!enabled_ || !machineSelf) return false;
        ++count_;
        return true;
    }

    // A natural charm drop starts the guarantee over, at any count. The
    // machine-self CreateDefaultParams (0, 98) reset calls this directly.
    void OnNaturalDrop() { count_ = 0; }

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
    // Exploded. A machine already destroyed at first sight never explodes.
    Sighting ObserveMachine(int64_t id, bool destroyed, int64_t frame, double x, double y)
    {
        auto it = machines_.find(id);
        if (it == machines_.end()) {
            machines_[id] = Machine{ destroyed, {} };
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
        pending_.push_back(e);
        ++explosions_;
        return Sighting::Exploded;
    }

    // The ground heads (Loot_Ground_obj instance ids holding the charm) near a
    // machine at its first sight: the baseline a later head is compared with.
    void SetBaseline(int64_t id, const std::vector<int64_t>& heads)
    {
        auto it = machines_.find(id);
        if (it != machines_.end()) it->second.baseline = std::set<int64_t>(heads.begin(), heads.end());
    }

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
    // `ground` the charm heads lying near the machine now. Abandoned and
    // natural and below are final here; a force waits for ForceConfirmed or
    // ForceRefused.
    Decision Decide(const Explosion& e, int64_t room, const std::vector<int64_t>& ground)
    {
        Decision d;
        if (room != e.room) {
            ++abandoned_;
            d.outcome = Outcome::Abandoned;
            return d;
        }
        int fresh = 0;
        auto it = machines_.find(e.id);
        for (int64_t h : ground)
            if (!ownHeads_.count(h) && (it == machines_.end() || !it->second.baseline.count(h))) ++fresh;
        if (fresh > 0 || !e.signal.empty()) {
            d.signal = fresh > 0 ? std::string("ground") : e.signal;
            d.outcome = Outcome::Natural;
            OnNaturalDrop();
            ++natural_;
            return d;
        }
        if (enabled_ && threshold_ > 0 && count_ >= threshold_) {
            d.outcome = Outcome::Force;
            return d;
        }
        ++below_;
        d.outcome = Outcome::Below;
        return d;
    }

    // The forced head was placed and read back (its ground instance id, kept
    // so a later explosion's ground check does not take it for natural): the
    // counter resets.
    void ForceConfirmed(int64_t groundId)
    {
        count_ = 0;
        ++forced_;
        if (groundId >= 0) ownHeads_.insert(groundId);
    }

    // The forced drop was refused: the counter is kept, so the next explosion
    // tries again.
    void ForceRefused() { ++refused_; }

    size_t Pending() const { return pending_.size(); }
    int Explosions() const { return explosions_; }
    int Forced() const { return forced_; }
    int Natural() const { return natural_; }
    int Below() const { return below_; }
    int Refused() const { return refused_; }
    int Abandoned() const { return abandoned_; }
    int OwnHeadBuilds() const { return ownHeadBuilds_; }

    // ---- the lines gambapity prints (fixed text; the contract test and the
    // live procedure read them) ------------------------------------------------
    std::string StatusLine() const
    {
        return std::string("gambapity: ") + (enabled_ ? "on" : "off")
            + " count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_)
            + " gold=" + std::to_string(GoldEquivalent())
            + " explosions=" + std::to_string(explosions_)
            + " forced=" + std::to_string(forced_)
            + " natural=" + std::to_string(natural_)
            + " below=" + std::to_string(below_)
            + " refused=" + std::to_string(refused_)
            + " abandoned=" + std::to_string(abandoned_)
            + " own-head-builds=" + std::to_string(ownHeadBuilds_);
    }

    static std::string MachineSeenLine(int64_t id, const std::string& sprite, int headsNearby)
    {
        return "gambapity: machine id=" + std::to_string(id) + " seen sprite=" + sprite
            + " heads-nearby=" + std::to_string(headsNearby);
    }

    std::string ExplosionLine(const Explosion& e) const
    {
        return "gambapity: explosion id=" + std::to_string(e.id) + " count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_) + " frame=" + std::to_string(e.frame);
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

    static std::string NaturalSeenLine(const std::string& signal)
    {
        return "gambapity: the explosion's own Goburin's Head was seen (" + signal + "); no force, counter reset";
    }

    std::string BelowLine() const
    {
        return "gambapity: explosion below the threshold (count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_) + "); counter kept";
    }

    static std::string RefusedLine(const std::string& stage)
    {
        return "gambapity: forced drop refused - " + stage + "; counter kept";
    }

    static std::string AbandonedLine(const Explosion& e)
    {
        return "gambapity: explosion id=" + std::to_string(e.id) + " abandoned (room changed); counter kept";
    }

    static std::string NaturalBuildLine()
    {
        return "gambapity: a natural Goburin's Head build reset the counter";
    }

private:
    struct Machine {
        bool destroyed = false;
        std::set<int64_t> baseline;   // charm heads near it at first sight
    };

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
};

} // namespace ForgePact::GambaPity
