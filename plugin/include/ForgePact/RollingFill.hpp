#pragma once

#include "Common.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace ForgePact {

// Fill the map as you approach (`fillroll`, issue #183). Off by default.
//
// Map Reveal's "Really spawn every pack on arrival" (`reveal spawn 1`) tells
// every ready spawner in a zone that the player is next to it, so the whole
// zone's packs are born on arrival. #183's frame-thread profile found the
// runtime's own work to be the largest part of a filled zone's frame (47% in
// Act_01_02 at 2x), and that work follows the number of active instances the
// runner walks, about two thirds of them monsters with their shadows and
// health bars (docs/main-thread-offload-research.md).
//
// With this switch on, the fill's answer of 0 goes only to creators within
// Reach() of the local player. A creator further out is told the truth (the
// native distance) and is born as the player comes near, either by the fill,
// whose pass then stays armed for the whole zone visit
// (MapRevealManager::OnFrame), or by the game's own proximity rule. Nothing is
// deactivated, hidden or paused: one value inside a call the game already
// makes is changed for fewer creators.
//
// This class holds the switch, the reach, the per-zone counters and the local
// player's position. The decision itself is WithinReach, a pure function.
// MapRevealManager::MayPopulate asks Admits() after its capacity and readiness
// guards and before its admission request, so a creator held back for
// distance never takes an admission slot, and Hook_distance_to_object reaches
// the rule only through MayPopulate. Like MapRevealManager, it reaches the
// game only through variable_instance_get, plus ModuleMain's
// HhResolveLocalPlayer for the local player (Common.hpp). Co-op: only the
// local player counts.
class RollingFill {
public:
    static constexpr double kDefaultReach = 3000.0;
    static constexpr double kMinReach = 1500.0;
    static constexpr double kMaxReach = 20000.0;
    static constexpr const char* kUsage = "fillroll: usage fillroll 1 | 0 | <reach px, 1500-20000> | stat";

    static RollingFill& Instance() {
        static RollingFill s_Instance;
        return s_Instance;
    }

    // Whether a creator at (cx, cy) is within `reach` of a player at (px, py).
    // A coordinate that is not a finite number is never within reach: the
    // native answer is the safe one.
    static bool WithinReach(double cx, double cy, double px, double py, double reach) {
        if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(px) || !std::isfinite(py) || !std::isfinite(reach)) return false;
        const double dx = cx - px, dy = cy - py;
        return dx * dx + dy * dy <= reach * reach;
    }

    static bool ReachInBounds(double reach) { return std::isfinite(reach) && reach >= kMinReach && reach <= kMaxReach; }

    // The verb's argument. `1` is On at kDefaultReach, `0` is Off, `stat` is
    // Stat, and a number from kMinReach to kMaxReach is On at that reach.
    // Anything else is Usage, and the caller changes nothing. `reach` is
    // written only for On.
    enum class Command { On, Off, Stat, Usage };
    static Command Parse(const std::string& arg, double& reach) {
        if (arg == "stat") return Command::Stat;
        if (arg.empty()) return Command::Usage;
        const char* begin = arg.c_str();
        char* end = nullptr;
        const double n = std::strtod(begin, &end);
        if (end == begin || *end != '\0' || !std::isfinite(n)) return Command::Usage;
        if (n == 0.0) return Command::Off;
        if (n == 1.0) { reach = kDefaultReach; return Command::On; }
        if (!ReachInBounds(n)) return Command::Usage;
        reach = n;
        return Command::On;
    }

    bool IsOn() const { return m_On; }
    double Reach() const { return m_Reach; }
    // Since the last zone change: lies told under the reach, and fill answers
    // withheld for distance. Both count decisions, not creators: a spawner
    // asks again at each of its checks.
    uint64_t Answered() const { return m_Answered; }
    uint64_t HeldBack() const { return m_HeldBack; }
    // The local player's position as this frame's first decision read it, or
    // false when no local player resolved (or none has been read yet).
    bool LastPlayer(double& x, double& y) const {
        if (!m_HavePlayer) return false;
        x = m_PlayerX; y = m_PlayerY;
        return true;
    }

    // MapRevealManager::SetFillRolling is the caller: turning it off also has
    // to re-arm the fill's pass. A reach outside the verb's bounds is never
    // stored; it falls back to the default.
    void Set(bool on, double reach) {
        m_On = on;
        m_Reach = ReachInBounds(reach) ? reach : kDefaultReach;
    }
    // Frame-boundary housekeeping (MapRevealManager::OnFrame): the next
    // decision reads the player again. Nothing here decides anything.
    void BeginFrame() { m_PlayerRead = false; }
    void ResetCounters() { m_Answered = 0; m_HeldBack = 0; }
    void NoteAnswered() { ++m_Answered; }

    // Whether the fill may answer this ready creator 0: true within the
    // reach. False, with the native answer left in place, for a creator
    // beyond it (counted as held back), and for every creator when no local
    // player resolves or a position cannot be read.
    bool Admits(const RValue& creator) {
        double px = 0, py = 0;
        if (!PlayerPosition(px, py)) return false;
        double cx = 0, cy = 0;
        try {
            const RValue x = g_Yytk->CallBuiltin("variable_instance_get", { creator, RValue("x") });
            const RValue y = g_Yytk->CallBuiltin("variable_instance_get", { creator, RValue("y") });
            if (!IsNumber(x) || !IsNumber(y)) return false;
            cx = x.ToDouble(); cy = y.ToDouble();
        } catch (...) { return false; }
        if (WithinReach(cx, cy, px, py, m_Reach)) return true;
        ++m_HeldBack;
        return false;
    }

private:
    RollingFill() = default;
    bool m_On{ false };
    double m_Reach{ kDefaultReach };
    uint64_t m_Answered{ 0 };
    uint64_t m_HeldBack{ 0 };
    bool m_PlayerRead{ false };
    bool m_HavePlayer{ false };
    double m_PlayerX{ 0 };
    double m_PlayerY{ 0 };

    static bool IsNumber(const RValue& v) {
        return v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64;
    }

    // At most one resolve and one x/y read a frame, however many creators
    // ask: the first decision after BeginFrame reads, the rest reuse it,
    // including a failed read.
    bool PlayerPosition(double& x, double& y) {
        if (!m_PlayerRead) {
            m_PlayerRead = true;
            m_HavePlayer = false;
            try {
                RValue player;
                if (HhResolveLocalPlayer(player)) {
                    const RValue px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") });
                    const RValue py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") });
                    if (IsNumber(px) && IsNumber(py) && std::isfinite(px.ToDouble()) && std::isfinite(py.ToDouble())) {
                        m_PlayerX = px.ToDouble();
                        m_PlayerY = py.ToDouble();
                        m_HavePlayer = true;
                    }
                }
            } catch (...) { m_HavePlayer = false; }
        }
        if (!m_HavePlayer) return false;
        x = m_PlayerX; y = m_PlayerY;
        return true;
    }
};

} // namespace ForgePact
