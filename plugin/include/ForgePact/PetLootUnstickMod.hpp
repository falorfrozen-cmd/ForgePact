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
// The mod does not need to know why the pickup failed. It watches for the
// observable shape - the same target, within reach, not going away - and when
// it sees it, the tick (ModuleMain.cpp's PetLootUnstickTick) hands the item
// back to the game's own "not yet for the pet" timer (`itemCompanionTimer`,
// kPetLootHoldFrames), drops the pet's target and clears its loot list, so the
// game's next scan picks something else. Nothing is collected, destroyed or
// credited by the mod; there is no hook.
//
// PetLootStuckWatch is game-independent by contract - a frame number, a target
// id and a distance in pixels, never an instance - so
// tests/pet_loot_unstick_harness.cpp compiles it whole. Frames are the tick's
// own count, one per tick while the mod is on.

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
    // kPetLootStuckFrames consecutive frames, and only once for that target:
    // the same id is not given up again until a different target, or none,
    // has been seen. No target, a different target, a distance beyond the
    // radius (or an unreadable one: NaN compares false) or a frame number that
    // skips restarts the count.
    bool Observe(int64_t frame, std::optional<double> target, double distancePx) {
        const bool consecutive = m_HaveLast && frame == m_LastFrame + 1;
        m_LastFrame = frame;
        m_HaveLast = true;

        if (!target) { Forget(); return false; }
        if (!m_Target || *m_Target != *target) {
            m_Target = target;
            m_GivenUp = false;
            m_Run = 0;
        }
        if (!consecutive || !(distancePx <= kPetLootStuckRadiusPx)) m_Run = 0;
        if (!(distancePx <= kPetLootStuckRadiusPx)) return false;

        ++m_Run;
        if (m_Run > m_Longest.load()) m_Longest.store(m_Run);
        if (m_GivenUp || m_Run < kPetLootStuckFrames) return false;
        m_GivenUp = true;
        return true;
    }

    // No pet, an unreadable or dead target: forget the current one. The
    // longest run is kept; it is a since-load figure.
    void Reset() {
        Forget();
        m_HaveLast = false;
    }

    // The current same-target in-reach run, in frames.
    int64_t Run() const { return m_Run; }
    // The longest such run since load, for `petunstick 0`. It keeps growing
    // past kPetLootStuckFrames while a given-up target stays in reach, so a
    // give-up that did not move the pet shows in the stat line.
    int64_t Longest() const { return m_Longest.load(); }

private:
    void Forget() {
        m_Target.reset();
        m_GivenUp = false;
        m_Run = 0;
    }

    std::optional<double> m_Target;
    bool m_GivenUp = false;
    int64_t m_Run = 0;
    int64_t m_LastFrame = 0;
    bool m_HaveLast = false;
    std::atomic<int64_t> m_Longest{ 0 };
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

    // A ground item handed back to its `itemCompanionTimer`.
    void NoteHeldBack() { m_HeldBack.fetch_add(1); }
    // A coin target dropped (a coin has no `itemCompanionTimer`; only the
    // pet's target is dropped, so a coin that sticks again counts again).
    void NoteCoinReleased() { m_CoinsReleased.fetch_add(1); }

    long HeldBack() const { return m_HeldBack.load(); }
    long CoinsReleased() const { return m_CoinsReleased.load(); }
    int64_t LongestRun() const { return m_Watch.Longest(); }

    // What `petunstick 0` prints, so a bug report can tell "did nothing"
    // from "did the wrong thing".
    std::string StatLine() const {
        return "petunstick stat: held back=" + std::to_string(HeldBack()) +
               " coins released=" + std::to_string(CoinsReleased()) +
               " longest same-target=" + std::to_string((long long)LongestRun()) + " frames";
    }

private:
    PetLootUnstickMod() = default;
    std::atomic<bool> m_Enabled{ false };
    PetLootStuckWatch m_Watch;
    std::atomic<long> m_HeldBack{ 0 };
    std::atomic<long> m_CoinsReleased{ 0 };
};

} // namespace ForgePact
