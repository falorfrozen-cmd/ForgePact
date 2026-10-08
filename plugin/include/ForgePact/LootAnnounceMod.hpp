#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <utility>

namespace ForgePact {

// ---- Loot announcements: the decision core (ForgePact #17) -----------------
//
// `lootann 1` announces in the in-game chat an item of Heroic, Angelic,
// Unholy, Satanic or Mythic rarity that the game drops on the ground, the way
// online play announces it. Offline the game shows no such line
// (docs/loot-announcement-research.md).
//
// The rule this header decides, once per ground item the shared
// LootGroundInit detour saw:
//   - off: nothing is announced, counted or remembered, and no creation is
//     noted.
//   - the creation guard: an item counts only when the game built its item
//     struct through CreateItemNew in the frame it reached the ground or the
//     frame before (RUNTIME_DATA_MODELS.md section 16.1: the outermost return
//     is the finished item). The adapter notes each CreateItemNew return's
//     keys from the shared Hook_CreateItemNew; a bag drop, a re-drop after a
//     pickup, or anything else that puts an existing struct on the ground
//     finds no recent note and is held (held-bag-drop). This replaced a
//     "bag-drop window" around LootGroundDrop, whose detour counted 0 while
//     Live procedure 2's bag drop reached LootGroundInit and was announced.
//     Live procedure 3 measured the guard holding a bag drop (held-bag-drop)
//     while the game's own drops passed it (created rose with seen).
//   - the rarity is itemInfoStruct["27"] (RUNTIME_DATA_MODELS.md section
//     16.4). Only kAnnouncedRarities announce; anything else, and a rarity
//     that could not be read as a number (kRarityUnread), does not.
//   - each item announces once: the memory is the item's itemType and its
//     itemTimeStamp when the stamp is real (not empty, "0" or "undefined"),
//     otherwise the ground instance id with the stamp (itemDataHash is not an
//     identity, section 16.5). An item whose identity could not be read (a
//     real stamp with no itemType, or no real stamp and no ground id) is not
//     Identifiable: the adapter counts it (no-identity) and does not decide
//     it, so one unread item never stands for another.
//
// It is game-independent by contract - rarity codes, instance ids, item keys
// as integers, types and time stamps as text, never an instance or an RValue
// - so tests/loot_announce_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp derives an item's key (a struct by its object pointer, a
// reference by the value it holds; compared, never followed), reads the item
// off the ground instance by name, ages the creation window once per frame
// after its batch, and runs the sink the verdict asks for.
class LootAnnounceMod {
public:
    // itemInfoStruct["27"]: 9 Heroic, 7 Angelic, 10 Unholy, 6 Satanic, 5
    // Mythic. Angelic and Unholy are the two uniques tiers the game's own
    // online rule announces; Heroic sits above Satanic. Satanic and Mythic were
    // added on the owner's word (2026-10-08: the drops they expected to see
    // announced). Common, Superior, Rare and every other code stay held.
    static constexpr int kHeroic = 9;
    static constexpr int kAngelic = 7;
    static constexpr int kUnholy = 10;
    static constexpr int kSatanic = 6;
    static constexpr int kMythic = 5;
    static constexpr std::array<int, 5> kAnnouncedRarities = { kHeroic, kAngelic, kUnholy, kSatanic, kMythic };
    // The adapter's value for a rarity it could not read as a number.
    static constexpr int kRarityUnread = -1;
    // Items remembered at most; the oldest is forgotten first.
    static constexpr std::size_t kMemoryCap = 4096;
    // Item keys one creation window holds at most (one frame's notes); a note
    // past it is counted in create-overflow and not kept.
    static constexpr std::size_t kCreationCap = 4096;

    // An item struct's key, as the adapter derives it at both ends: the kind
    // of value (a struct or a reference) and the pointer or reference value.
    static constexpr int kKeyStruct = 1;
    static constexpr int kKeyReference = 2;
    struct ItemKey {
        int kind;
        std::uint64_t value;
        bool operator<(const ItemKey& o) const { return kind != o.kind ? kind < o.kind : value < o.value; }
    };

    // How the adapter shows the line (docs/loot-announcement-research.md,
    // "Route"). kShippedSink is the one the mod runs: `server`
    // (ChatAddServerMessage, measured to show a line offline). Live procedure
    // 1 (2026-10-04) showed no line from the other three, and none of them is
    // settled: `method` was refused because the probe of that build could not
    // name any anon@ method (it handed script_get_name an unconverted index),
    // so whether the ground item holds the announcement closure is not
    // established; NetworkSendChatMessageIngame and GetItemDropMessage
    // refused with the one item argument supplied (the itemInstance struct).
    // Live procedure 3 (2026-10-04) retested `method` with the fixed probe:
    // the anon control still resolved no anon@ method (each read as index
    // -1), so `method` stays unsettled and `server` ships by the owner's
    // rule; no further session is planned.
    enum class Sink : int { Method, NetSend, ChatAdd, Server };
    static constexpr Sink kShippedSink = Sink::Server;

    enum class Verdict : int { Off, Announce, HeldRarity, HeldNoRarity, HeldDuplicate, HeldBagDrop };

    struct Counters {
        long long seen = 0;          // items decided while on
        long long announced = 0;     // verdicts that asked for a line
        long long heldRarity = 0;    // a rarity that is not announced
        long long heldNoRarity = 0;  // no numeric rarity to read
        long long heldDuplicate = 0; // the same item seen again
        long long heldBagDrop = 0;   // not built this frame or the last: a bag drop, a re-drop, a restored item
        long long sinkRefused = 0;   // a line asked for that the sink could not show
        long long created = 0;       // item keys noted into the creation window
        long long createOverflow = 0; // notes past kCreationCap, not kept
    };

    static constexpr bool IsAnnouncedRarity(int code)
    {
        for (int r : kAnnouncedRarities)
            if (r == code) return true;
        return false;
    }

    static constexpr const char* SinkName(Sink s)
    {
        switch (s) {
        case Sink::Method: return "method";
        case Sink::NetSend: return "netsend";
        case Sink::ChatAdd: return "chatadd";
        case Sink::Server: return "server";
        }
        return "unknown";
    }

    static constexpr const char* VerdictName(Verdict v)
    {
        switch (v) {
        case Verdict::Off: return "off";
        case Verdict::Announce: return "announce";
        case Verdict::HeldRarity: return "held-rarity";
        case Verdict::HeldNoRarity: return "held-no-rarity";
        case Verdict::HeldDuplicate: return "held-duplicate";
        case Verdict::HeldBagDrop: return "held-bag-drop";
        }
        return "unknown";
    }

    // ---- the install: armed at launch, hooked once a character exists ----
    // The guide's Known Limitations item 8: a hook installed at character
    // select stalls the runner. A switch already on as the game starts (the
    // panel's launch commands carry `lootann 1`, consumed from the first frame
    // after setup, while character selection still runs) only arms; the
    // hooks go in on the first frame where setup is done and the local player
    // resolves (the adapter asks HhResolveLocalPlayer), once per session.
    // While armed, the adapter's tick looks for the player only on the frames
    // LooksForPlayer picks, counted from the first armed frame.
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    // Whether the adapter installs now: the switch on, setup done, the local
    // player resolved, and no install tried yet this session.
    bool ShouldInstall(bool setupDone, bool playerResolved, bool installTried) const
    {
        return m_Enabled && setupDone && playerResolved && !installTried;
    }
    // The stat line's ` install=` field: installed once the install ran,
    // waiting-for-character while on without it, not-armed while off and
    // never installed.
    const char* InstallState(bool installTried) const
    {
        if (installTried) return "installed";
        return m_Enabled ? "waiting-for-character" : "not-armed";
    }

    bool Enabled() const { return m_Enabled; }
    // Switching off keeps the memory and the counters, so an item still lying
    // there is not announced again when the switch comes back on; it clears
    // the creation window, so nothing built while off counts as new.
    void SetEnabled(bool on)
    {
        m_Enabled = on;
        if (!on) {
            m_CreatedNow.clear();
            m_CreatedBefore.clear();
        }
    }

    // A CreateItemNew return (or its argument 0), noted while on. A key
    // already in this frame's window is not counted again.
    void NoteCreated(const ItemKey& key)
    {
        if (!m_Enabled || m_CreatedNow.count(key)) return;
        if (m_CreatedNow.size() >= kCreationCap) {
            ++m_Stats.createOverflow;
            return;
        }
        m_CreatedNow.insert(key);
        ++m_Stats.created;
    }

    // Noted before this tick's decisions, in this frame or the one before.
    bool RecentlyCreated(const ItemKey& key) const
    {
        return m_CreatedNow.count(key) != 0 || m_CreatedBefore.count(key) != 0;
    }

    // Once per tick, after its decisions: this frame's notes become the
    // previous frame's, and the previous frame's are forgotten.
    void AgeCreationWindow()
    {
        m_CreatedBefore.swap(m_CreatedNow);
        m_CreatedNow.clear();
    }

    // The memory's identity of an item: its itemType and a real itemTimeStamp,
    // else the ground instance id with whatever stamp it carries.
    static bool IsRealStamp(const std::string& timeStamp)
    {
        return !timeStamp.empty() && timeStamp != "0" && timeStamp != "undefined";
    }
    static std::string Identity(int64_t instanceId, const std::string& itemType, const std::string& timeStamp)
    {
        if (IsRealStamp(timeStamp)) return "item:" + itemType + ":" + timeStamp;
        return "ground:" + std::to_string(instanceId) + ":" + timeStamp;
    }
    // Whether the adapter read the parts Identity() would use: the itemType
    // ("" when it could not be read) beside a real stamp, else the ground
    // instance id. Asked before Decide, so an unread part never reaches the
    // memory, where two unread items would compare equal.
    static bool Identifiable(bool groundIdRead, const std::string& itemType, const std::string& timeStamp)
    {
        return IsRealStamp(timeStamp) ? !itemType.empty() : groundIdRead;
    }

    // One ground item. `recentlyCreated` is RecentlyCreated() for the key of
    // the item struct it holds (false when it gave no key); the rest is read
    // off the ground instance, and is Identifiable (the adapter asks first).
    Verdict Decide(int rarityCode, bool recentlyCreated, int64_t instanceId, const std::string& itemType,
                   const std::string& timeStamp)
    {
        if (!m_Enabled) return Verdict::Off;
        ++m_Stats.seen;
        if (!recentlyCreated) {
            ++m_Stats.heldBagDrop;
            return Verdict::HeldBagDrop;
        }
        if (rarityCode == kRarityUnread) {
            ++m_Stats.heldNoRarity;
            return Verdict::HeldNoRarity;
        }
        if (!IsAnnouncedRarity(rarityCode)) {
            ++m_Stats.heldRarity;
            return Verdict::HeldRarity;
        }
        std::string key = Identity(instanceId, itemType, timeStamp);
        if (m_Seen.count(key)) {
            ++m_Stats.heldDuplicate;
            return Verdict::HeldDuplicate;
        }
        m_Seen.insert(key);
        m_Order.push_back(std::move(key));
        while (m_Order.size() > kMemoryCap) {
            m_Seen.erase(m_Order.front());
            m_Order.pop_front();
        }
        ++m_Stats.announced;
        return Verdict::Announce;
    }

    // The adapter's sink could not show a line the verdict asked for.
    void NoteSinkRefused() { ++m_Stats.sinkRefused; }

    const Counters& Stats() const { return m_Stats; }
    std::size_t Remembered() const { return m_Order.size(); }

    std::string StatusLine() const { return m_Enabled ? "lootann: on" : "lootann: off"; }

    // `lootann stat`; the adapter appends its hook routes.
    std::string StatLine() const
    {
        std::string s = StatusLine();
        s += std::string(" route=") + SinkName(kShippedSink);
        s += " seen=" + std::to_string(m_Stats.seen);
        s += " announced=" + std::to_string(m_Stats.announced);
        s += " held-rarity=" + std::to_string(m_Stats.heldRarity);
        s += " held-no-rarity=" + std::to_string(m_Stats.heldNoRarity);
        s += " held-duplicate=" + std::to_string(m_Stats.heldDuplicate);
        s += " held-bag-drop=" + std::to_string(m_Stats.heldBagDrop);
        s += " sink-refused=" + std::to_string(m_Stats.sinkRefused);
        s += " remembered=" + std::to_string(m_Order.size());
        s += " created=" + std::to_string(m_Stats.created);
        s += " create-overflow=" + std::to_string(m_Stats.createOverflow);
        return s;
    }

private:
    bool m_Enabled = false;
    Counters m_Stats;
    std::set<std::string> m_Seen;
    std::deque<std::string> m_Order;
    std::set<ItemKey> m_CreatedNow;    // noted since the last tick
    std::set<ItemKey> m_CreatedBefore; // noted in the frame before
};

} // namespace ForgePact
