#pragma once

#include "Common.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace ForgePact {

// ---- Pet moves on from loot it cannot pick up (issue #94) -------------------
//
// Reported: with lots of loot on the ground (gold, runes and other
// socketables, crafting materials) the companion stays on one item, hopping
// around it without taking it or moving on. A static reading of Companion_obj's
// events (docs/pet-loot-stuck-research.md; nothing there is measured yet) says
// why: the pet's `lootTarget` is replaced only when that instance ceases to
// exist, arrival does not end the travel, and an item whose pickup fails is
// left on the ground and passes the next scan. So one item the game cannot
// pick up pins the pet for as long as it lies there.
//
// A second shape was measured on 2026-10-02 (docs/pet-loot-stuck-research.md,
// "Live 2 results"): the target can also be an id that is not loot at all.
// The game frees a destroyed item's instance id and reuses it for whatever is
// created next, so `lootTarget` starts naming a stranger (in the measured
// case a zone's decoration object) and `instance_exists` keeps passing, so
// the game never retargets. The pet travels to that object and grinds at it
// - 52-88 px away, `move=true`, `deltaSpeed` at travel speed - while the
// player walks away. For that shape the watch below is the wrong tool (its
// 90-frame count needs the pet within 160 px, and the pet has no business at
// that object at all): a live target whose `object_index` is from neither the
// ground-item nor the coin family can never be picked up, so it is given up
// on the tick that sees it, through the header's PetLootRoute.
//
// The mod does not need to know why the pickup failed. It watches for the
// observable shape - the same target, within reach, not going away - and when
// it sees it, the tick (ModuleMain.cpp's PetLootUnstickTick) hands the item
// back to the game's own "not yet for the pet" timer (`itemCompanionTimer`,
// kPetLootHoldFrames), drops the pet's target and clears its loot list, so the
// game's next scan picks something else. The clear is the load-bearing write:
// the item timer only keeps an item out of a new scan, so a list left holding
// the item hands it straight back. It is gated on `ds_exists(lootList,
// ds_type_list)`, never on the value's kind (a live ds handle may reach us as
// VALUE_REF), and a clear the tick refuses is counted and logged. Nothing is
// collected, destroyed or credited by the mod; there is no hook.
//
// The game may hand a dropped target straight back in its very next Step (a
// coin has no timer; a ground item's hold may not take), so no tick reads "no
// target" in between. The watch therefore re-arms at each give-up - the same
// target, still in reach, is given up again every kPetLootStuckFrames frames -
// and PetLootRepickRing counts such a take-back as a re-pick while held, once
// per give-up, so `petunstick 0` names it.
//
// PetLootStuckWatch and PetLootRepickRing are game-independent by contract -
// frame numbers, target ids and a distance in pixels, never an instance - so
// tests/pet_loot_unstick_harness.cpp compiles them whole. Frames are the
// tick's own count, one per tick while the mod is on.

// Consecutive frames the same target has been within reach without going away
// before it is given up: 1.5 s at 60 fps. The game tries the pickup on every
// frame the pet has arrived, so by then it has failed dozens of times.
inline constexpr int64_t kPetLootStuckFrames = 90;
// "Within reach", pet to target. The game's own pickup circle is 144 px around
// the pet; a pet travelling to a far item is beyond this and never counts.
inline constexpr double kPetLootStuckRadiusPx = 160.0;
// The value written into a given-up item's `itemCompanionTimer`, in the game's
// own frame units (about 10 s): long enough that the pet visibly moves on and
// other items get their turn, short enough that an item whose pickup failed
// for a passing reason is retried. The same hold the Pet Quest Collector uses.
inline constexpr int64_t kPetLootHoldFrames = 600;

class PetLootStuckWatch {
public:
    // Fed once per tick with the pet's current loot target (none when it has
    // none) and the pet-to-target distance in pixels. Returns true on the
    // frame the same target has been within kPetLootStuckRadiusPx for
    // kPetLootStuckFrames consecutive frames, and re-arms on answering: a
    // target that is still the target and still in reach on the next frames
    // (the game took it straight back) is given up again exactly
    // kPetLootStuckFrames frames after the previous give-up, and not a frame
    // earlier. No target, a different target, a distance beyond the radius
    // (or an unreadable one: NaN compares false) or a frame number that skips
    // restarts the count. A give-up the tick could not carry out is followed
    // by Reset(), which restarts it too.
    bool Observe(int64_t frame, std::optional<double> target, double distancePx) {
        const bool consecutive = m_HaveLast && frame == m_LastFrame + 1;
        m_LastFrame = frame;
        m_HaveLast = true;

        if (!target) { Forget(); return false; }
        if (!m_Target || *m_Target != *target) {
            m_Target = target;
            m_Run = 0;
        }
        if (!consecutive || !(distancePx <= kPetLootStuckRadiusPx)) m_Run = 0;
        if (!(distancePx <= kPetLootStuckRadiusPx)) return false;

        ++m_Run;
        if (m_Run > m_Longest.load()) m_Longest.store(m_Run);
        if (m_Run < kPetLootStuckFrames) return false;
        m_Run = 0;   // re-arm: the next in-reach frame counts 1 again
        return true;
    }

    // No pet, an unreadable or dead target: forget the current one. The
    // longest run is kept; it is a since-load figure.
    void Reset() {
        Forget();
        m_HaveLast = false;
    }

    // The current same-target in-reach run since the last start or give-up,
    // in frames.
    int64_t Run() const { return m_Run; }
    // The longest such run since load, for `petunstick 0`: the longest a
    // target sat in reach between give-ups. The watch re-arms at each
    // give-up, so while the mod is on it never exceeds kPetLootStuckFrames;
    // a give-up that did not move the pet shows as `re-picked while held=`
    // instead (PetLootRepickRing).
    int64_t Longest() const { return m_Longest.load(); }

private:
    void Forget() {
        m_Target.reset();
        m_Run = 0;
    }

    std::optional<double> m_Target;
    int64_t m_Run = 0;
    int64_t m_LastFrame = 0;
    bool m_HaveLast = false;
    std::atomic<int64_t> m_Longest{ 0 };
};

// What a given-up target was, recorded with the give-up: a ground item (the
// tick set, or tried to set, its itemCompanionTimer), a coin (no timer; only
// the target was dropped), or neither.
enum class PetLootKind { Other, Ground, Coin };

// The tick's routing question for a live target, from what the game's own
// data says the target is: feed the same-target watch, or drop it on sight?
// A live target from neither loot family can never be picked up - the pet's
// list only ever holds ground items and coins - so an id that names anything
// else is a stale one the game reused (above), and waiting out the watch
// would leave the pet grinding at it. An unreadable kind is not evidence of a
// wrong id, so it keeps the watch route. Pure, so the harness pins it.
enum class PetLootTargetRoute { Watch, DropOnSight };

inline PetLootTargetRoute PetLootRoute(bool kindRead, bool isGround, bool isCoin)
{
    if (!kindRead) return PetLootTargetRoute::Watch;
    if (isGround || isCoin) return PetLootTargetRoute::Watch;
    return PetLootTargetRoute::DropOnSight;
}

// A re-pick while held: the pet's live target is one given up less than
// kPetLootHoldFrames frames ago.
struct PetLootRepick {
    PetLootKind kind = PetLootKind::Other;
    int64_t age = 0;   // frames since that give-up
};

// Whether a give-up held. The tick remembers its last kSize give-ups (id,
// frame, kind), and on every tick that sees a live target asks Seen() whether
// that target is one of them taken back. It is, when an entry for the same id
// is younger than kPetLootHoldFrames and either the previous tick's live
// target was a different id or none (the pet left it and came back), or the
// entry was recorded on the frame immediately before this one (the game
// handed it straight back, so the previous tick's target is this very id). A
// target that stays the target after being counted is not counted again on
// the ticks after, until the next give-up or until it leaves and comes back.
// Ages are in the tick's frames; the hold is in the game's own frames; both
// run at the game's frame rate. The entries are touched only from the tick;
// the counters are read by `petunstick 0` on the IPC thread.
class PetLootRepickRing {
public:
    static constexpr int kSize = 8;

    // A give-up the tick carried out (the target was dropped) on `frame`.
    void Remember(int64_t frame, double id, PetLootKind kind) {
        Entry& e = m_Entries[m_Next];
        e.id = id;
        e.frame = frame;
        e.kind = kind;
        e.used = true;
        m_Next = (m_Next + 1) % kSize;
    }

    // Called once per tick that saw a live target `id` on `frame`; `previous`
    // is the previous tick's live target, none when that tick saw none or was
    // not the frame before. Counts and returns the re-pick, by the newest
    // matching give-up's kind, or nothing.
    std::optional<PetLootRepick> Seen(int64_t frame, double id, std::optional<double> previous) {
        const bool leftAndCameBack = !previous || *previous != id;
        const Entry* hit = nullptr;
        for (const Entry& e : m_Entries) {
            if (!e.used || e.id != id) continue;
            const int64_t age = frame - e.frame;
            if (age <= 0 || age >= kPetLootHoldFrames) continue;
            if (!leftAndCameBack && e.frame != frame - 1) continue;
            if (!hit || e.frame > hit->frame) hit = &e;
        }
        if (!hit) return std::nullopt;
        m_Repicked.fetch_add(1);
        if (hit->kind == PetLootKind::Ground) m_Ground.fetch_add(1);
        else if (hit->kind == PetLootKind::Coin) m_Coin.fetch_add(1);
        return PetLootRepick{ hit->kind, frame - hit->frame };
    }

    long Repicked() const { return m_Repicked.load(); }
    long Ground() const { return m_Ground.load(); }
    long Coin() const { return m_Coin.load(); }

private:
    struct Entry {
        double id = -1.0;
        int64_t frame = 0;
        PetLootKind kind = PetLootKind::Other;
        bool used = false;
    };
    Entry m_Entries[kSize];
    int m_Next = 0;
    std::atomic<long> m_Repicked{ 0 };
    std::atomic<long> m_Ground{ 0 };
    std::atomic<long> m_Coin{ 0 };
};

// Off by default, like every ForgePact mod: `petunstick 1` / `petunstick 0`.
// The watch and the counters live here so `petunstick 0` (the IPC thread) can
// read what the tick (the game thread) did; the watch itself is only touched
// from the tick.
class PetLootUnstickMod {
public:
    static PetLootUnstickMod& Instance() {
        static PetLootUnstickMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }

    // "petunstick 1" / "petunstick 0".
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        Out(std::string("petunstick -> ") + (enabled
            ? "ON (the pet gives up an item it has sat on for 1.5 s and moves on; the item waits ~10 s)"
            : "OFF"));
    }

    // The tick's question: give this target up now? Never while off, and a
    // call while off forgets the watch, so a run cannot straddle the mod
    // being switched off and on.
    bool Observe(int64_t frame, std::optional<double> target, double distancePx) {
        if (!IsEnabled()) { m_Watch.Reset(); return false; }
        return m_Watch.Observe(frame, target, distancePx);
    }

    // No pet out, or no live target: forget the current one.
    void Reset() { m_Watch.Reset(); }

    // The tick's give-ups and the re-picks it counted; the tick remembers and
    // asks, `petunstick 0` reads the counters.
    PetLootRepickRing& Repicks() { return m_Repicks; }
    const PetLootRepickRing& Repicks() const { return m_Repicks; }

    // A ground item handed back to its `itemCompanionTimer`.
    void NoteHeldBack() { m_HeldBack.fetch_add(1); }
    // A coin target dropped (a coin has no `itemCompanionTimer`; only the
    // pet's target is dropped, so a coin that sticks again counts again).
    void NoteCoinReleased() { m_CoinsReleased.fetch_add(1); }

    // Where each tick went, so `ticks=0`, "never found the pet", "the target
    // read failed" and "nothing was ever stuck" read differently. Every tick
    // while on calls NoteTick() first. A tick that returns before the watch
    // calls exactly one of NoteNoPet/NoteNoTarget/NoteTargetGone/
    // NoteUnreadable; the rest reached the watch. After a give-up the tick
    // may also count an unreadable `object_index` (the target is still
    // dropped) and a refused `lootList` clear (NoteListNotCleared).
    void NoteTick() { m_Ticks.fetch_add(1); }
    // No Companion_obj instance (a menu, no pet out). A normal state, not a
    // refusal: counted, never logged.
    void NoteNoPet() { m_NoPet.fetch_add(1); }
    // The pet's `lootTarget` is below 0: it has nothing to fetch. Normal.
    void NoteNoTarget() { m_NoTarget.fetch_add(1); }
    // `lootTarget` names an instance for which `instance_exists` is false.
    void NoteTargetGone() { m_TargetGone.fetch_add(1); }
    // A read the tick needed failed: it threw, or gave a non-finite number.
    // `field` is a string literal naming it ("lootTarget", "pet x", "target
    // y", ...); the first such refusal is logged once, the latest is kept
    // for the stat line.
    void NoteUnreadable(const char* field) {
        m_Unreadable.fetch_add(1);
        m_UnreadableLast.store(field);
        if (!m_UnreadableLogged.exchange(true))
            Out(std::string("petunstick: could not read ") + field +
                " (counted as unreadable=; logged once)");
    }
    // A give-up whose `ds_list_clear` the tick did not run: `lootList` threw,
    // was not a finite number, or `ds_exists(lootList, ds_type_list)` said
    // no. The target and timer writes still happened, but the pet may take
    // the same item straight back from the uncleared list. `why` is a string
    // literal; logged once, the latest kept.
    void NoteListNotCleared(const char* why) {
        m_ListNotCleared.fetch_add(1);
        m_ListNotClearedLast.store(why);
        if (!m_ListNotClearedLogged.exchange(true))
            Out(std::string("petunstick: gave up a target but did not clear lootList: ") + why +
                " (counted as list not cleared=; logged once)");
    }

    long HeldBack() const { return m_HeldBack.load(); }
    long CoinsReleased() const { return m_CoinsReleased.load(); }
    int64_t LongestRun() const { return m_Watch.Longest(); }
    long Ticks() const { return m_Ticks.load(); }
    long NoPet() const { return m_NoPet.load(); }
    long NoTarget() const { return m_NoTarget.load(); }
    long TargetGone() const { return m_TargetGone.load(); }
    long Unreadable() const { return m_Unreadable.load(); }
    long ListNotCleared() const { return m_ListNotCleared.load(); }

    // The success paths: what the mod did.
    std::string StatLine() const {
        return "petunstick stat: held back=" + std::to_string(HeldBack()) +
               " coins released=" + std::to_string(CoinsReleased()) +
               " longest same-target=" + std::to_string((long long)LongestRun()) + " frames";
    }

    // What `petunstick 0` prints: StatLine() followed, on the same line, by
    // where the ticks went and every refusal with the read it failed on, so a
    // bug report can tell "did nothing" from "could not look" from "did the
    // wrong thing".
    std::string FullStatLine() const {
        const char* unreadable = m_UnreadableLast.load();
        const char* notCleared = m_ListNotClearedLast.load();
        return StatLine() +
               " ticks=" + std::to_string(Ticks()) +
               " no pet=" + std::to_string(NoPet()) +
               " no target=" + std::to_string(NoTarget()) +
               " target gone=" + std::to_string(TargetGone()) +
               " unreadable=" + std::to_string(Unreadable()) +
               (unreadable ? std::string(" (last ") + unreadable + ")" : std::string()) +
               " list not cleared=" + std::to_string(ListNotCleared()) +
               (notCleared ? std::string(" (last ") + notCleared + ")" : std::string());
    }

private:
    PetLootUnstickMod() = default;
    std::atomic<bool> m_Enabled{ false };
    PetLootStuckWatch m_Watch;
    PetLootRepickRing m_Repicks;
    std::atomic<long> m_HeldBack{ 0 };
    std::atomic<long> m_CoinsReleased{ 0 };
    std::atomic<long> m_Ticks{ 0 };
    std::atomic<long> m_NoPet{ 0 };
    std::atomic<long> m_NoTarget{ 0 };
    std::atomic<long> m_TargetGone{ 0 };
    std::atomic<long> m_Unreadable{ 0 };
    std::atomic<long> m_ListNotCleared{ 0 };
    std::atomic<const char*> m_UnreadableLast{ nullptr };
    std::atomic<const char*> m_ListNotClearedLast{ nullptr };
    std::atomic<bool> m_UnreadableLogged{ false };
    std::atomic<bool> m_ListNotClearedLogged{ false };
};

} // namespace ForgePact
