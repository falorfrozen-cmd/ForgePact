#pragma once

#include "Common.hpp"

#include <array>

namespace ForgePact {

// The relic ids the game's relic pick draws: `irandom(155)`, so 0..155. Static
// reading of the Sep-17 build (hub docs/models/relic-pick-spec.md).
inline constexpr int kRelicPickIdCount = 156;

// Relic drop pool filter ("Remove owned relics from drop pool", #125):
// a relic the player already owns at maximum level (10/10), worn or in the
// relic tab, never drops again, and another relic drops in its place.
//
// How the game picks a relic (static reading, hub
// docs/models/relic-pick-spec.md):
// - All three routines that place one draw an id and draw again while
//   `GetRelicQuest(id)` answers true. They are `DropRelic` and the two Satanic
//   kill routines, which never pass through `DropRelic`.
// - `GetRelicQuest` is true only for the quest relics.
// - A direct-call scan of the game found no other caller of `GetRelicQuest`.
//
// So the filter answers "quest" for a maxed relic too, and the game's own loop
// draws again. Every other relic keeps the odds the game gives it. An equipped
// relic the game copies in place of the pick is never a maxed one, because the
// copy step skips a relic at 10/10 by itself.
//
// Until ForgePact#125 the filter wrote each maxed relic's `droprate.base`
// around `DropRelic`. No relic pick reads that field, so it held nothing back,
// although its log said "holding back".
//
// This class owns the filter's state and its game-independent decisions:
// - whether the filter is armed;
// - the maxed-relic scan, cached per frame;
// - the stand-down check;
// - the skip count.
//
// ModuleMain.cpp's `Hook_GetRelicQuest` makes the game calls.
//
// `relicfilter 1` only ARMS the filter; it does not install the hook
// immediately. Installing a script hook while character selection is still
// running stalled the runner for about a minute (measured 2026-09-09, on
// `DropRelic`). ModuleMain.cpp's `FrameCallback` installs `GetRelicQuest` once
// its own setup gate has passed and a real player instance exists, then calls
// `ClearPending()`.
class RelicFilterMod {
public:
    static RelicFilterMod& Instance() {
        static RelicFilterMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }
    bool IsPending() const { return m_Pending.load(); }
    void ClearPending() { m_Pending.store(false); }

    // Whether the arm-time scan line is still owed for the latest
    // `relicfilter 1` (#93). ModuleMain.cpp's FrameCallback emits it through
    // RelicFilterReportArmScan once a player exists, after the hook install
    // when one is pending. That call clears it, so there is one line per arm.
    // It is also due on a re-arm with the hook already in, where no install is
    // pending.
    bool IsArmScanDue() const { return m_ArmScanDue.load(); }
    void ClearArmScanDue() { m_ArmScanDue.store(false); }

    // "relicfilter 1" / "relicfilter 0". `alreadyHooked` lets the caller report
    // accurate status text without this class needing to know about the
    // GetRelicQuest trampoline it does not own. Arming starts a fresh count of
    // skips and a fresh scan.
    void SetEnabled(bool enabled, bool alreadyHooked) {
        m_Enabled.store(enabled);
        m_ArmScanDue.store(enabled);
        if (enabled && !alreadyHooked) m_Pending.store(true);
        if (!enabled) m_Pending.store(false);
        if (enabled) {
            m_Skips = 0;
            m_LastSkip = -1;
            m_StoodDown = false;
        }
        m_CacheValid = false;
        Out(std::string("relicfilter -> ") + (enabled ? (alreadyHooked ? "ON" : "ON (armed, applies once you are in-game)") : "OFF"));
    }

    // Returns whether the scan actually RAN, which is not the same question as
    // whether it found anything. An empty set means "this player has no maxed
    // relics" only when this returned true. When it returns false, the set is
    // empty because there was nothing to scan: the filter is off, no player
    // instance exists yet, or the read threw. A caller that cannot tell those
    // apart reports "0 maxed relics" for a scan that never happened, which is
    // exactly how the dead scanner went unnoticed before 2026-09-14.
    //
    // `equippedReport` and `tabReport`, when given, receive what the SDK's
    // equipped-slot read (#93) and relic-tab read (#125) did. Each names the
    // stage it stopped at, so a scan that ran and found nothing can still say
    // whether that place was read. Only the once-per-arm line asks for them.
    bool GetPlayerMaxedRelics(std::unordered_set<int>& outMaxed,
                              HeroSiege::Player::EquippedSlotScanReport* equippedReport = nullptr,
                              HeroSiege::Player::RelicTabScanReport* tabReport = nullptr) const {
        outMaxed.clear();
        if (!m_Enabled.load()) return false;
        try {
            RValue player;
            if (!HhResolveLocalPlayer(player)) return false;
            outMaxed = HeroSiege::Player::GetMaxedRelicIds(g_Yytk, player, equippedReport, tabReport);
            return true;
        } catch (...) { return false; }
    }

    // The maxed set for one frame. A roll asks GetRelicQuest once per draw, and
    // with many maxed relics one roll draws many times. So the scan runs at
    // most once per frame, and every draw in that frame reuses it. The set
    // includes the research-only test ids (`relicfilter testmaxed`).
    // `scanRan` reports GetPlayerMaxedRelics' own answer.
    const std::unordered_set<int>& MaxedForFrame(uint64_t frame, bool& scanRan) {
        if (!m_CacheValid || m_CacheFrame != frame) {
            m_CacheRan = GetPlayerMaxedRelics(m_CacheSet);
            if (m_CacheRan) {
                for (int id : m_TestMaxed) m_CacheSet.insert(id);
            }
            m_CacheFrame = frame;
            m_CacheValid = true;
        }
        scanRan = m_CacheRan;
        return m_CacheSet;
    }

    // The GetRelicQuest answer (#125): true when the game says the relic is a
    // quest relic, or when the player owns it at 10/10 while another relic is
    // still left for the game's draw-again loop. It is game-independent, so
    // the behaviour harness and the hub's relic-pick model test can pin it.
    static bool QuestAnswer(bool gameAnswer, int id, const std::unordered_set<int>& maxed, bool relicLeft) {
        if (gameAnswer) return true;
        return relicLeft && maxed.count(id) != 0;
    }

    // Whether the draw-again loop can still end once every maxed relic also
    // reads as a quest relic: some id in 0..kRelicPickIdCount-1 is neither.
    // `isQuest(id)` is the game's own answer. When nothing is left, the filter
    // stands down; answering "quest" then would never let the loop end.
    template <class IsQuest>
    static bool AnyRelicLeft(const std::unordered_set<int>& maxed, IsQuest isQuest) {
        for (int id = 0; id < kRelicPickIdCount; ++id) {
            if (!isQuest(id) && maxed.count(id) == 0) return true;
        }
        return false;
    }

    // The game's own GetRelicQuest answer for `id`, probed once per id and kept.
    // The answer is fixed game data, and the stand-down check needs it for
    // every id. `probe(id)` calls the original script. An id outside the pick
    // range is asked every time and never kept.
    template <class Probe>
    bool IsQuestCached(int id, Probe probe) {
        if (id < 0 || id >= kRelicPickIdCount) return probe(id);
        int8_t& known = m_Quest[static_cast<size_t>(id)];
        if (known < 0) known = probe(id) ? 1 : 0;
        return known == 1;
    }

    // One maxed relic skipped: the game draws again. Returns the count since
    // armed.
    long NoteSkip(int id) {
        m_LastSkip = id;
        return ++m_Skips;
    }
    long Skips() const { return m_Skips; }
    int LastSkip() const { return m_LastSkip; }

    // Records whether the filter stood down on the latest maxed draw, because
    // nothing else was left. Returns true only when that changed, so the log
    // says it once per change.
    bool NoteStandDown(bool stoodDown) {
        if (stoodDown == m_StoodDown) return false;
        m_StoodDown = stoodDown;
        return true;
    }
    bool StoodDown() const { return m_StoodDown; }

    // Research build only (`relicfilter testmaxed`): ids treated as maxed on
    // top of the scan's. A live check can then prove the lever without raising
    // a real relic to 10/10 in the player's save.
    void SetTestMaxed(std::unordered_set<int> ids) {
        m_TestMaxed = std::move(ids);
        m_CacheValid = false;
    }
    const std::unordered_set<int>& TestMaxed() const { return m_TestMaxed; }

private:
    RelicFilterMod() { m_Quest.fill(-1); }
    std::atomic<bool> m_Enabled{ false };
    // Set when `relicfilter 1` arrives before a player exists; ModuleMain's
    // FrameCallback installs the GetRelicQuest hook later and calls ClearPending().
    std::atomic<bool> m_Pending{ false };
    // Set by every `relicfilter 1`, cleared by the arm-time report or `0`.
    std::atomic<bool> m_ArmScanDue{ false };

    // Everything below is touched only on the game thread: the hook, the frame
    // callback and PollCommands, which runs inside the frame callback.
    std::array<int8_t, kRelicPickIdCount> m_Quest{};
    std::unordered_set<int> m_CacheSet;
    uint64_t m_CacheFrame = 0;
    bool m_CacheValid = false;
    bool m_CacheRan = false;
    std::unordered_set<int> m_TestMaxed;
    long m_Skips = 0;
    int m_LastSkip = -1;
    bool m_StoodDown = false;
};

} // namespace ForgePact
