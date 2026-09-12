#pragma once

#include "Common.hpp"

namespace ForgePact {

// Relic drop pool filter: excludes relics already at maximum level (10/10) in
// the player's equipped slots, backpack, or inventory from dropping again.
//
// The `DropRelic` hook itself (`Hook_DropRelic` in ModuleMain.cpp) is a
// shared chokepoint also used by the unrelated "dropmult relic <x>"
// multiplier feature, and stays owned there for now rather than here -
// splitting a single installed hook across two classes mid-migration is a
// bigger, riskier step than one module's worth of work. This class owns only
// the filter-specific state and logic: whether the filter is armed, the
// maxed-relic scan, and the arm/defer lifecycle. `Hook_DropRelic` calls
// `Instance().GetPlayerMaxedRelics(...)` and the two scan helpers below for
// its filtering piece.
//
// `relicfilter 1` only ARMS the filter - it does not install the hook
// immediately. Installing `DropRelic` while character selection is still
// running stalls the runner for about a minute (measured 2026-09-09), which
// is why the panel used to withhold the command entirely and the mod never
// applied after a game restart. ModuleMain.cpp's `FrameCallback` installs the
// hook once its own setup gate has passed and a real player instance exists,
// then calls `ClearPending()`.
class RelicFilterMod {
public:
    static RelicFilterMod& Instance() {
        static RelicFilterMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }
    bool IsPending() const { return m_Pending.load(); }
    void ClearPending() { m_Pending.store(false); }

    // "relicfilter 1" / "relicfilter 0". `alreadyHooked` lets the caller
    // report accurate status text without this class needing to know about
    // the shared DropRelic trampoline it does not own.
    void SetEnabled(bool enabled, bool alreadyHooked) {
        m_Enabled.store(enabled);
        if (enabled && !alreadyHooked) m_Pending.store(true);
        if (!enabled) m_Pending.store(false);
        Out(std::string("relicfilter -> ") + (enabled ? (alreadyHooked ? "ON" : "ON (armed, applies once you are in-game)") : "OFF"));
    }

    void GetPlayerMaxedRelics(std::unordered_set<int>& outMaxed) const {
        outMaxed.clear();
        if (!m_Enabled.load()) return;
        try {
            RValue player;
            if (!HhResolveLocalPlayer(player)) return;
            outMaxed = HeroSiege::Player::GetMaxedRelicIds(g_Yytk, player);
        } catch (...) {}
    }

    // NOTE: an earlier container-walking scan (recursively inspecting struct/
    // array fields for relic-shaped items) lived here and in ModuleMain.cpp as
    // ScanItemForMaxRelic/ScanContainerForMaxRelics, but was never actually
    // called by anything - GetPlayerMaxedRelics above uses the SDK's
    // HeroSiege::Player::GetMaxedRelicIds instead, which superseded it. Dead
    // code, deleted rather than migrated (2026-09 class split cleanup).

private:
    RelicFilterMod() = default;
    std::atomic<bool> m_Enabled{ false };
    // Set when `relicfilter 1` arrives before a player exists; ModuleMain's
    // FrameCallback installs the DropRelic hook later and calls ClearPending().
    std::atomic<bool> m_Pending{ false };
};

} // namespace ForgePact
