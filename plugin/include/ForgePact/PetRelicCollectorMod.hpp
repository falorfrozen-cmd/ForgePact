#pragma once

#include "Common.hpp"
#include "PetQuestCollectorMod.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ForgePact {

// ---- Pet Collects Relics (ForgePact issue #124) -----------------------------
//
// While the player's pet is out, it walks to relics lying on screen and picks
// them up through the game's own loot pickup (`PickupLoot`, the script the
// companion and the player's own pickup both call), one at a time, raising
// the owned copy's level exactly as a hand pickup does. A relic the player
// already owns at 10/10 is never targeted: the game will not let it be picked
// up (an equipped copy at 10/10 makes PickupRelic return false), so a pet sent
// to it would be stuck on it, #94's shape. See
// ForgePact/docs/pet-relic-collector-research.md for the call shape and what
// the static reading established.
//
// It shares the Pet Quest Collector's targeting rather than copying it: the
// relic tick owns its own PetQuestSelector (nearest not held back, the family
// cursor, the hold-back), and both ticks walk the pet through one travel
// helper in ModuleMain.cpp. What is relic-specific lives here, game-
// independent by contract - relic ids, a maxed set, squared distances and
// frame numbers, never an instance - so tests/pet_relic_collector_harness.cpp
// compiles this header whole:
//
//   * FilterRelicCandidates drops every candidate whose relic is maxed before
//     PetQuestSelector::Pick sees it. With every relic on screen maxed, Pick
//     gets an empty list and the pet stays idle, by construction.
//   * PetRelicMaxedCache holds the maxed set between reads: re-read at most
//     once per kPetRelicMaxedRefreshTicks, and straight after a collect that
//     returned true (a pickup can take a relic to 10).
//   * PetFetchArbiter says which collector holds the pet's travel. One pet,
//     one fetch: without it two ticks would write the pet's x/y against each
//     other while both switches are on.
//
// The maxed set itself is read in ModuleMain.cpp through hs-game-sdk
// (HeroSiege::Player::GetMaxedRelicIds), never through RelicFilterMod's own
// scan, which answers nothing while the relic filter's switch is off.

// How often the cached maxed set is read again while nothing was collected.
// Once a second at 60 fps: the set changes only on a pickup (refreshed at
// once) or a relic equipped or moved by hand.
inline constexpr int64_t kPetRelicMaxedRefreshTicks = 60;

struct PetRelicCandidate {
    double id = 0.0;          // the ground instance's own `id`
    double distance2 = 0.0;   // squared pixels from the pet
    int relicId = -1;         // the dropped relic's id (its definition's `b`)
};

// The candidates whose relic is not in `maxed`, as the selector takes them.
// `dropped` gains one per candidate left out, for `skipped(maxed)=`.
inline std::vector<PetQuestCandidate> FilterRelicCandidates(const std::vector<PetRelicCandidate>& candidates,
                                                            const std::unordered_set<int>& maxed, long& dropped) {
    std::vector<PetQuestCandidate> kept;
    kept.reserve(candidates.size());
    for (const PetRelicCandidate& c : candidates) {
        if (maxed.count(c.relicId)) { ++dropped; continue; }
        kept.push_back({ c.id, c.distance2 });
    }
    return kept;
}

// Which collector walks the pet.
enum class PetFetcher : int { None = 0, Quest, Relic };

inline const char* PetFetcherName(PetFetcher who) {
    switch (who) {
    case PetFetcher::Quest: return "petquest";
    case PetFetcher::Relic: return "petrelic";
    default: return "none";
    }
}

// One pet, one fetch. A collector claims the travel when it picks a target
// and releases it when the travel ends (however it ends, the stand-down of a
// switched-off collector included); a tick whose collector does not hold it,
// while another does, picks nothing that frame.
class PetFetchArbiter {
public:
    static PetFetchArbiter& Instance() {
        static PetFetchArbiter s_Instance;
        return s_Instance;
    }

    // Free, or already ours.
    bool MayPick(PetFetcher who) const { return m_Holder == PetFetcher::None || m_Holder == who; }

    // Takes the travel for `who`; false, and nothing changed, while another
    // collector holds it.
    bool Claim(PetFetcher who) {
        if (!MayPick(who)) return false;
        m_Holder = who;
        return true;
    }

    // Gives the travel back. A release by a collector that does not hold it
    // does nothing, so a defensive release can never free another's travel.
    void Release(PetFetcher who) {
        if (m_Holder == who) m_Holder = PetFetcher::None;
    }

    PetFetcher Holder() const { return m_Holder; }

private:
    PetFetcher m_Holder = PetFetcher::None;
};

// The maxed set between reads, on the relic tick's own frame count.
class PetRelicMaxedCache {
public:
    // A read is due before the first one, after a collect marked the set
    // stale, and kPetRelicMaxedRefreshTicks after the last read.
    bool Due(int64_t frame) const {
        return !m_Valid || m_Stale || frame - m_Frame >= kPetRelicMaxedRefreshTicks;
    }

    void Store(std::unordered_set<int> ids, int64_t frame) {
        m_Ids = std::move(ids);
        m_Frame = frame;
        m_Valid = true;
        m_Stale = false;
        ++m_Scans;
    }

    // A collect returned true: the owned copy's level may have reached 10.
    void MarkStale() { m_Stale = true; }

    bool IsMaxed(int relicId) const { return m_Ids.count(relicId) != 0; }
    const std::unordered_set<int>& Ids() const { return m_Ids; }
    // Reads since load, for `maxed scans=`.
    long Scans() const { return m_Scans; }

private:
    std::unordered_set<int> m_Ids;
    int64_t m_Frame = 0;
    bool m_Valid = false;
    bool m_Stale = false;
    long m_Scans = 0;
};

// Which script the collect calls. A is what ships: `PickupLoot`, the shape
// the companion's own call passes. B calls `PickupRelic` directly with the
// same self and other, and is selectable only in the research build
// (`petrelic route b`), so a refused route A costs a command, not a rebuild.
enum class PetRelicRoute : int { PickupLoot = 0, PickupRelic = 1 };

class PetRelicCollectorMod {
public:
    static PetRelicCollectorMod& Instance() {
        static PetRelicCollectorMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }

    // "petrelic 1" / "petrelic 0".
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        Out(std::string("petrelic -> ") + (enabled
            ? "ON (pet fetches relics on screen and picks them up; a relic owned at 10/10 is left alone)"
            : "OFF"));
    }

    PetRelicRoute Route() const { return m_Route.load(); }
    void SetRoute(PetRelicRoute route) { m_Route.store(route); }
    const char* RouteName() const { return Route() == PetRelicRoute::PickupRelic ? "b" : "a"; }

    PetRelicMaxedCache& Maxed() { return m_Maxed; }
    const PetRelicMaxedCache& Maxed() const { return m_Maxed; }

    // A collect that could not be made, with the reason `petrelic 0` names as
    // `(last <why>)`: no pet, no player, no item, no itemInstance, call
    // threw, no callable, returned false.
    void Refuse(const char* why) {
        refused.fetch_add(1);
        m_LastRefusal.store(why);
    }
    const char* LastRefusal() const { return m_LastRefusal.load(); }

    // A pickup that returned true with no raise of the owned copy seen, with
    // the reason `petrelic 0` names as `(last <why>)`: `no-raise(<before>->
    // <after>)` or `after-scan-incomplete(<scan>:<stage>)`. Nothing was
    // destroyed; the relic is held back. `why` must outlive the call.
    void NothingRaised(const char* why) {
        trueButNothingRaised.fetch_add(1);
        m_LastNothingRaised.store(why);
    }
    const char* LastNothingRaised() const { return m_LastNothingRaised.load(); }

    // The counters. Every one is printed by StatLine.
    std::atomic<long> petSeen{ 0 };              // relic ticks that found a Companion_obj out (0: no pet, so nothing else ran)
    std::atomic<long> collected{ 0 };          // a true return whose raise was seen: one level up, or newly owned at 1
    std::atomic<long> skippedMaxed{ 0 };         // candidate reads left out as maxed, and collects refused as maxed
    std::atomic<long> skippedNotRelic{ 0 };      // ground items on screen the SDK read did not call a relic
    std::atomic<long> skippedGate{ 0 };          // the target's itemActive read false at collect time
    std::atomic<long> itemActiveMissing{ 0 };    // the target carried no itemActive (counted, not treated as false)
    std::atomic<long> refused{ 0 };              // the collect could not be made, or the game refused it
    std::atomic<long> trueButNothingRaised{ 0 }; // a true return with no raise seen (nothing destroyed, held back)
    std::atomic<long> destroyedByPlugin{ 0 };    // ground relics the plugin destroyed after a seen raise
    std::atomic<long> destroyFailed{ 0 };        // the relic was still there after the plugin's destroy (held back)
    std::atomic<long> targetLost{ 0 };           // the target vanished while the pet travelled
    std::atomic<long> travelTimeouts{ 0 };       // the pet never reached the target

    // The one line `petrelic 0` prints (both builds), with the selector's
    // `held back=` and the tick's phase handed in.
    std::string StatLine(long heldBack, bool travelling) const {
        std::string ids;
        for (int id : SortedMaxed()) ids += (ids.empty() ? "" : ",") + std::to_string(id);
        return "petrelic stat: collected=" + std::to_string(collected.load())
            + " skipped(maxed)=" + std::to_string(skippedMaxed.load())
            + " skipped(not relic)=" + std::to_string(skippedNotRelic.load())
            + " skipped(gate)=" + std::to_string(skippedGate.load())
            + " itemActive missing=" + std::to_string(itemActiveMissing.load())
            + " refused=" + std::to_string(refused.load()) + " (last " + LastRefusal() + ")"
            + " true-but-nothing-raised=" + std::to_string(trueButNothingRaised.load())
            + " (last " + LastNothingRaised() + ")"
            + " destroyed-by-plugin=" + std::to_string(destroyedByPlugin.load())
            + " destroy-failed=" + std::to_string(destroyFailed.load())
            + " target lost=" + std::to_string(targetLost.load())
            + " travel timeouts=" + std::to_string(travelTimeouts.load())
            + " held back=" + std::to_string(heldBack)
            + " maxed scans=" + std::to_string(m_Maxed.Scans())
            + " maxed ids=" + (ids.empty() ? std::string("none") : ids)
            + " route=" + RouteName()
            + " phase=" + (travelling ? "travel" : "idle")
            + " pet-seen ticks=" + std::to_string(petSeen.load());
    }

private:
    PetRelicCollectorMod() = default;

    std::vector<int> SortedMaxed() const {
        std::vector<int> ids(m_Maxed.Ids().begin(), m_Maxed.Ids().end());
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    std::atomic<bool> m_Enabled{ false };
    std::atomic<PetRelicRoute> m_Route{ PetRelicRoute::PickupLoot };
    std::atomic<const char*> m_LastRefusal{ "none" };
    std::atomic<const char*> m_LastNothingRaised{ "none" };
    PetRelicMaxedCache m_Maxed;
};

} // namespace ForgePact
