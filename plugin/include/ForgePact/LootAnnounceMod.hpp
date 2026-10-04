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
// `lootann 1` announces in the in-game chat an item of Heroic, Angelic or
// Unholy rarity that the game drops on the ground, the way online play
// announces it. Offline the game shows no such line
// (docs/loot-announcement-research.md).
//
// The rule this header decides, once per ground item the shared
// LootGroundInit detour saw:
//   - off: nothing is announced, counted or remembered.
//   - a bag drop (the item arrived while the LootGroundDrop detour's window
//     was open: the player dropping it from the bag) is not announced.
//   - the rarity is itemInfoStruct["27"] (RUNTIME_DATA_MODELS.md section
//     16.4). Only kAnnouncedRarities announce; anything else, and a rarity
//     that could not be read as a number (kRarityUnread), does not.
//   - each item announces once: the memory is the ground instance id and the
//     item's itemTimeStamp (itemDataHash is not an identity, section 16.5).
//
// It is game-independent by contract - rarity codes, instance ids and time
// stamps as text, never an instance or an RValue - so
// tests/loot_announce_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp reads the item off the ground instance by name, opens the
// bag-drop window around LootGroundDrop's original, and runs the sink the
// verdict asks for.
class LootAnnounceMod {
public:
    // itemInfoStruct["27"]: 9 Heroic, 7 Angelic, 10 Unholy. Heroic sits above
    // Satanic (6); Angelic and Unholy are the two uniques tiers the game's own
    // online rule announces.
    static constexpr int kHeroic = 9;
    static constexpr int kAngelic = 7;
    static constexpr int kUnholy = 10;
    static constexpr std::array<int, 3> kAnnouncedRarities = { kHeroic, kAngelic, kUnholy };
    // The adapter's value for a rarity it could not read as a number.
    static constexpr int kRarityUnread = -1;
    // Items remembered at most; the oldest is forgotten first.
    static constexpr std::size_t kMemoryCap = 4096;

    // How the adapter shows the line (docs/loot-announcement-research.md,
    // "Route"). kShippedSink is the one the mod runs: `server`
    // (ChatAddServerMessage, measured to show a line offline). Live procedure
    // 1 (2026-10-04) found the other three unreachable offline: the ground
    // item carries no announcement method, and NetworkSendChatMessageIngame
    // and GetItemDropMessage refuse a call by name.
    enum class Sink : int { Method, NetSend, ChatAdd, Server };
    static constexpr Sink kShippedSink = Sink::Server;

    enum class Verdict : int { Off, Announce, HeldRarity, HeldNoRarity, HeldDuplicate, HeldBagDrop };

    struct Counters {
        long long seen = 0;          // items decided while on
        long long announced = 0;     // verdicts that asked for a line
        long long heldRarity = 0;    // a rarity that is not announced
        long long heldNoRarity = 0;  // no numeric rarity to read
        long long heldDuplicate = 0; // the same item seen again
        long long heldBagDrop = 0;   // dropped from the bag
        long long sinkRefused = 0;   // a line asked for that the sink could not show
    };

    // The bag-drop window, held open by the LootGroundDrop detour for the
    // duration of the game's original. Nested calls nest.
    class BagDropScope {
    public:
        explicit BagDropScope(LootAnnounceMod& mod) : m_Mod(mod) { m_Mod.BeginBagDrop(); }
        ~BagDropScope() { m_Mod.EndBagDrop(); }
        BagDropScope(const BagDropScope&) = delete;
        BagDropScope& operator=(const BagDropScope&) = delete;

    private:
        LootAnnounceMod& m_Mod;
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

    bool Enabled() const { return m_Enabled; }
    // Switching off keeps the memory and the counters, so an item still lying
    // there is not announced again when the switch comes back on.
    void SetEnabled(bool on) { m_Enabled = on; }

    void BeginBagDrop() { ++m_BagDropDepth; }
    void EndBagDrop()
    {
        if (m_BagDropDepth > 0) --m_BagDropDepth;
    }
    bool BagDropActive() const { return m_BagDropDepth > 0; }

    // One ground item. `isBagDrop` is BagDropActive() as it stood inside the
    // game's LootGroundInit call, which the adapter captures there; the rest
    // is read off the ground instance afterwards.
    Verdict Decide(int rarityCode, bool isBagDrop, int64_t instanceId, const std::string& timeStamp)
    {
        if (!m_Enabled) return Verdict::Off;
        ++m_Stats.seen;
        if (isBagDrop) {
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
        Key key{ instanceId, timeStamp };
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
        return s;
    }

private:
    using Key = std::pair<int64_t, std::string>;

    bool m_Enabled = false;
    int m_BagDropDepth = 0;
    Counters m_Stats;
    std::set<Key> m_Seen;
    std::deque<Key> m_Order;
};

} // namespace ForgePact
