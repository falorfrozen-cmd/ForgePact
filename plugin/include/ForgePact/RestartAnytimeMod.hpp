#pragma once

#include "Common.hpp"

namespace ForgePact {

// Restart zone at any time (issue #8; `restartanytime`). The pause menu's
// Restart refuses while the game counts the player as in combat. Research
// round 3 (docs/restart-always-available-research.md, "## Results" and
// "## Decision") measured what it is refused on: the Restart button's own
// `manualDisable` member, which the game sets true every frame in combat.
// Writing it back to false inside the game's own `UiSetFocus` call - on the
// button that call is handed as its first argument, before the game's body
// runs - let a press reach the Restart activation and restart the zone with
// the player still in combat. `enabled` alone did not, so it is not touched.
//
// So the mod changes one value inside a call the game is already making. It
// never restarts anything itself, and it never restores the value after the
// call: the game recomputes the member every frame, and that recompute is the
// restore (D10). `UiSetFocus` is only called while the cursor is on a
// button, so the write only happens while Restart is hovered.
//
// This header holds only the game-independent part - the names, the decision
// and the counters - so every branch is unit-testable without a game process
// (tests/test_restart_anytime_behavior.py). Reading the button, writing the
// member and installing the hook live in ModuleMain.cpp's
// HookRestartAnytimeSetFocus, which leans on the runtime this header does not
// pull in. The shape is ToggleGuardModel/ToggleGuardMod's (ToggleSkillMod.hpp).

// The call the write happens inside (round 3, T1/T2: handed the Restart
// button every frame it is hovered, and early enough that the press reads the
// value written there).
inline constexpr std::string_view kRestartAnytimeSiteScript = HeroSiege::Scripts::gml_Script_UiSetFocus;

// How the Restart button is recognised: by what it is, a string member of its
// own that the round-2 dump read as "PauseRestart" (the same UI text key the
// static search listed). Never by position, by instance id - the menu is
// rebuilt on every open, and the ids changed between rounds - or by which
// instance happened to draw Restart last.
inline constexpr const char* kRestartButtonIdMember = "uiNodeCallstack";
inline constexpr const char* kRestartButtonIdValue = "PauseRestart";

// The one member written, and the value that means "Restart allowed" (round 3
// T3: manualDisable=false alone unlocked the press; measured as a bool, and
// written in whatever kind it is read in).
inline constexpr const char* kRestartGateMember = "manualDisable";
inline constexpr bool kRestartGateReadyValue = false;

enum class RestartAnytimeDecision { Pass, Write };

class RestartAnytimeModel {
public:
    // Order matters and is deliberate:
    //   1. off is the vanilla path - nothing else is asked;
    //   2. a node that is not the Restart button is never written;
    //   3. a Restart button whose gate member could not be read (absent, or
    //      not a bool or number) is never written - there is no kind to
    //      write it in, and guessing one is how a wrong value lands;
    //   4. a gate already open (out of combat) needs nothing written.
    static constexpr RestartAnytimeDecision Decide(bool enabled, bool isRestartButton, bool memberReadOk,
                                                   bool alreadyReady)
    {
        if (!enabled) return RestartAnytimeDecision::Pass;
        if (!isRestartButton) return RestartAnytimeDecision::Pass;
        if (!memberReadOk) return RestartAnytimeDecision::Pass;
        if (alreadyReady) return RestartAnytimeDecision::Pass;
        return RestartAnytimeDecision::Write;
    }
};

// `restartanytime 1` only ARMS the mod, the ToggleGuardMod shape: installing a
// script hook while character selection is still running stalls the runner
// (guide Known Limitations item 8), so FrameCallback installs the `UiSetFocus`
// hook later, once the setup gate has passed and a player exists, then calls
// ClearPending(). `restartanytime 0` clears the enabled flag only; an
// installed hook stays, and its first statement passes every call straight
// through while the flag is off. An install that did not put the inline
// detour in calls MarkBlind(): the table route alone never sees the game's
// own direct calls, so the mod turns itself off for the session rather than
// reporting ON and doing nothing.
class RestartAnytimeMod {
public:
    static RestartAnytimeMod& Instance() {
        static RestartAnytimeMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }
    bool IsPending() const { return m_Pending.load(); }
    bool IsBlind() const { return m_Blind.load(); }
    void ClearPending() { m_Pending.store(false); }

    // `alreadyHooked` is whether the UiSetFocus trampoline exists yet; this
    // class does not own it (ModuleMain.cpp's g_OrigUiSetFocus). A blind
    // session stays off: turning it on again could only report ON.
    void SetEnabled(bool enabled, bool alreadyHooked) {
        if (enabled && m_Blind.load()) { m_Enabled.store(false); m_Pending.store(false); return; }
        m_Enabled.store(enabled);
        if (enabled && !alreadyHooked) m_Pending.store(true);
        if (!enabled) m_Pending.store(false);
    }

    void MarkBlind() {
        m_Blind.store(true);
        m_Enabled.store(false);
        m_Pending.store(false);
    }

    // written: the Restart button's gate was closed and was written open.
    // passed: the Restart button's gate was already open, so nothing was
    // written. otherNode: UiSetFocus was handed something that is not the
    // Restart button (another button, a node without the identifying member,
    // no argument at all) - never written. unreadable: it was the Restart
    // button, but its gate member could not be read as a bool or number, or
    // the write itself threw - never written.
    void NoteWritten() {
        if (m_Written.fetch_add(1) == 0) m_FirstWritePending.store(true);
    }
    void NotePassed() { m_Passed.fetch_add(1); }
    void NoteOtherNode() { m_OtherNode.fetch_add(1); }
    void NoteUnreadable() { m_Unreadable.fetch_add(1); }

    long Written() const { return m_Written.load(); }
    long Passed() const { return m_Passed.load(); }
    long OtherNode() const { return m_OtherNode.load(); }
    long Unreadable() const { return m_Unreadable.load(); }

    // True exactly once per session, after the first confirmed write, so the
    // log can say the mod actually did something rather than only that it is
    // on.
    bool TakeFirstWrite() { return m_FirstWritePending.exchange(false); }

private:
    RestartAnytimeMod() = default;
    std::atomic<bool> m_Enabled{ false };
    std::atomic<bool> m_Pending{ false };
    std::atomic<bool> m_Blind{ false };
    std::atomic<bool> m_FirstWritePending{ false };
    std::atomic<long> m_Written{ 0 };
    std::atomic<long> m_Passed{ 0 };
    std::atomic<long> m_OtherNode{ 0 };
    std::atomic<long> m_Unreadable{ 0 };
};

} // namespace ForgePact
