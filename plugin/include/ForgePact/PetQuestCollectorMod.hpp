#pragma once

#include "Common.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ForgePact {

// ---- target selection (issue #94) -------------------------------------------
//
// Reported: with many quest items on screen the pet circled one of them
// instead of collecting them one after another. The tick chose its target with
// no memory of failure (a static reading of our own code; no session named the
// cause): a collect that dispatched but left the item in place, a gate refusal
// at arrival, or a travel timeout each made the same item the next target,
// forever. And the Idle walk read at most 64 family instances per tick from
// index 0, while the family also holds the static props, so a collectable item
// past index 63 was never a candidate.
//
// The design covers every one of those at once rather than guessing which one
// the player met: a failed target is held back for kPetQuestHoldFrames and
// counted (`held back=` on `petquest 0`), and the family walk resumes where
// the budget stopped last tick, so every instance is read within a few ticks
// while one tick still reads at most the budget. Nothing here collects,
// removes or credits anything; it only chooses which item the tick walks to.
//
// Game-independent by contract - ids, squared distances and frame numbers,
// never an instance - so tests/pet_quest_collector_harness.cpp compiles it
// whole. Frames are the tick's own count, one per tick while the mod is on.

// Long enough that the pet visibly moves on to the next item and comes back
// to a failed one only after the rest had their turn (~10 s at 60 fps), short
// enough that an item whose collect failed for a passing reason is retried.
inline constexpr int64_t kPetQuestHoldFrames = 600;
// A runaway run of failures cannot grow the set; the hold closest to expiring
// gives way first.
inline constexpr size_t kPetQuestHoldMax = 32;

struct PetQuestCandidate {
    double id = 0.0;          // the instance's own `id`
    double distance2 = 0.0;   // squared pixels from the pet
};

// How a travel ended. Collected, Lost and Abandoned leave nothing to hold
// back: the item is gone, or the travel stopped for a reason that is not the
// item's (no pet, an unreadable position). Every other outcome left the item
// where it was.
enum class PetQuestOutcome : int {
    Collected = 1,   // the collect ran and the item is gone
    NoEffect,        // the collect dispatched and the item remained
    Gate,            // canPickup/lootType refused the collect at arrival
    Refused,         // the collect could not be made (no Loot_Manager_obj, no callable)
    Timeout,         // the pet never arrived and the collect-anyway did not remove it
    Lost,            // the item vanished while the pet travelled
    Abandoned,       // the travel stopped for a reason that is not the item's
};

class PetQuestSelector {
public:
    // While a travel is open this returns its target unchanged: the pet is
    // never sent somewhere else mid-walk. Otherwise it picks the nearest
    // candidate not held back at `frame` and opens a travel to it; with every
    // candidate held back (or none on screen) it picks none and stays idle,
    // so the pet waits for a hold to expire instead of spinning.
    std::optional<double> Pick(const std::vector<PetQuestCandidate>& candidates, int64_t frame) {
        if (m_Travelling) return m_Target;
        const PetQuestCandidate* best = nullptr;
        for (const PetQuestCandidate& c : candidates) {
            if (IsHeld(c.id, frame)) continue;
            if (!best || c.distance2 < best->distance2) best = &c;
        }
        if (!best) return std::nullopt;
        m_Target = best->id;
        m_Travelling = true;
        return m_Target;
    }

    // Closes the open travel with how it ended; returns true when that held
    // the target back (and counted it). A Note with no travel open does
    // nothing.
    bool Note(PetQuestOutcome outcome, int64_t frame) {
        if (!m_Travelling) return false;
        m_Travelling = false;
        switch (outcome) {
        case PetQuestOutcome::Collected:
        case PetQuestOutcome::Lost:
        case PetQuestOutcome::Abandoned:
            return false;
        default:
            Hold(m_Target, frame);
            m_HeldBack.fetch_add(1);
            return true;
        }
    }

    // Leaves `id` out of every pick until kPetQuestHoldFrames after `frame`.
    // A second hold on a held id restarts its hold instead of taking a slot.
    void Hold(double id, int64_t frame) {
        Prune(frame);
        const int64_t until = frame + kPetQuestHoldFrames;
        for (Held& h : m_Held) {
            if (h.id == id) { h.until = until; return; }
        }
        if (m_Held.size() >= kPetQuestHoldMax) {
            auto soonest = std::min_element(m_Held.begin(), m_Held.end(),
                [](const Held& a, const Held& b) { return a.until < b.until; });
            m_Held.erase(soonest);
        }
        m_Held.push_back({ id, until });
    }

    bool IsHeld(double id, int64_t frame) const {
        for (const Held& h : m_Held)
            if (h.id == id && frame < h.until) return true;
        return false;
    }

    size_t HeldCount(int64_t frame) const {
        size_t n = 0;
        for (const Held& h : m_Held)
            if (frame < h.until) ++n;
        return n;
    }

    // The family cursor. Returns the index this tick's walk starts at; the
    // walk reads min(total, budget) instances from there, wrapping at total.
    // A family within the budget is walked whole from 0 every tick.
    int NextStart(int total, int budget) {
        if (total <= 0 || budget <= 0 || total <= budget) { m_Cursor = 0; return 0; }
        const int start = m_Cursor % total;   // the family may have shrunk
        m_Cursor = (start + budget) % total;
        return start;
    }

    bool Travelling() const { return m_Travelling; }
    // Every hold since load, for `petquest 0`'s `held back=`.
    long HeldBack() const { return m_HeldBack.load(); }

private:
    struct Held { double id; int64_t until; };

    void Prune(int64_t frame) {
        m_Held.erase(std::remove_if(m_Held.begin(), m_Held.end(),
            [frame](const Held& h) { return frame >= h.until; }), m_Held.end());
    }

    std::vector<Held> m_Held;
    double m_Target = 0.0;
    bool m_Travelling = false;
    int m_Cursor = 0;
    std::atomic<long> m_HeldBack{ 0 };
};

// Pet Quest Collector: while enabled and the player has a pet out, the pet is
// meant to collect quest items anywhere on screen without the player hovering
// each one and pressing the interact key. See
// `ForgePact/docs/pet-quest-collector-plan.md` for the full design.
//
// STATUS (2026-09-11): WORKING, and re-confirmed live after the call route
// was rewritten to use no game addresses - the quest counter advanced on a
// pet collect, with `petquest 0` reporting
// `call route=script_execute (name-resolved, no layout)`,
// `dispatched-but-item-remained=0` and zero structural refusals.
//
// The mechanism, confirmed live -
// a plugin-invoked collect advanced a quest counter 7/15 -> 8/15, which is
// the pass condition the plan set (the objective credited, not merely the
// item removed). The call, every part of it measured rather than inferred:
// with the quest item as `self` and `Loot_Manager_obj` as `other`, invoke the
// item's own `m_Questpickup` method value with one real argument. It calls
// update_quest(questIndex, questObjectiveNumber, questValue) and
// QuestSaveUpdate internally, so the credit is inside the call and nothing
// here has to fake it.
//
// REVISED the same day, twice. That call originally went through the
// runtime's call-a-method-value helper at a fixed address, which would have
// jumped into unrelated bytes on the next game build - `relicgate`'s defect,
// shipped again. Reading the callable off the value's own CScriptRef needs no
// address but was then measured wrong for this runner (the `m_Quest*` values
// are not CScriptRefs here). What ships is `script_execute`, resolved by
// name, with self/other supplied through CallBuiltinEx: no address, and no
// struct layout either. The call shape never changed across any of it.
// `InvokeMethodValue` keeps the CScriptRef route as a validated fallback, and
// `petquest 0` reports which route ran. See agents.md, "Never Call an Address
// You Resolved by Hand", and docs/pet-quest-collector-c-research.md.
//
// Three earlier mechanisms were closed by measurement and one turned out to
// be an artefact:
//   B1/B2 (call or hook the script the interaction invokes) - the "0 calls"
//      that blocked these was the *instrument*: those hooks swap a pointer in
//      the script table, and compiled GML calls another script with a direct
//      call bound at compile time, which never reads that table;
//   B4 (fake the player's hover + keypress) - genuinely disproven: the
//      GML-visible input state is a downstream mirror of real device input.
// See docs/pet-quest-collector-c-research.md for the whole chain.
//
// What the tick does now: with the mod on and a pet out, it picks the nearest
// eligible quest item on screen that is not held back, walks the pet to it
// (writing x/y, the same way OrbPickupTick pulls globes), and on arrival
// invokes the confirmed collect - one item at a time, with a cooldown
// between, so it reads as the pet fetching rather than items silently
// vanishing. The choice itself is PetQuestSelector, above.
//
// Two gates are re-read from the live instance at collect time, never cached
// from selection: `canPickup`, and `lootType == 0`. Both are the game's own,
// taken from its real call site. lootType 0 is the m_Questpickup branch;
// the other quest object types (activate/destructible/interact/active) want a
// different m_Quest* method, none of which is confirmed, so they are left
// alone rather than guessed at. Driving a collect on an item the game would
// have skipped is exactly how an objective gets credited for something
// uncollectable, which the plan calls worse than no mod at all.
//
// Deliberately simpler than RelicFilterMod: no hook is installed by this mod
// (the tick only reads instance positions/state through YYTK's CallBuiltin,
// same chokepoint OrbPickupTick already uses), so there is no arm/pending
// lifecycle to manage and `build_cmds` can safely emit `petquest 1` at
// launch.
class PetQuestCollectorMod {
public:
    static PetQuestCollectorMod& Instance() {
        static PetQuestCollectorMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }

    // "petquest 1" / "petquest 0".
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        Out(std::string("petquest -> ") + (enabled ? "ON (pet fetches and collects lootType-0 quest items on screen)" : "OFF"));
    }

private:
    PetQuestCollectorMod() = default;
    std::atomic<bool> m_Enabled{ false };
};

} // namespace ForgePact
