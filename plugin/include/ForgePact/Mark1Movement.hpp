#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace ForgePact::Mark1Movement {

// ---- Mark 1: the movement levers (ForgePact #166, phase 2) ----------------------
//
// While the Mark 1 set's bonus is active the walk runs at half speed. Mod live
// 1 picked the route each lever takes (docs/mark1-set-research.md, "Phase 2
// decision"):
//   - speed-route: stat-model. The walk's modal step is affine in the answer
//     the stat dispatcher (ReturnSpecificStat) gives for id 64, movement
//     speed: proportional to that answer plus about 200, every swept point
//     within 0.02%, no cap reached up to an answer of 226.5 (measured,
//     `speed-sweep`). So the walk is halved by answering x' = (x - 200) / 2
//     to the local player's id-64 calls, which `speed-half` measured at a
//     ratio of 0.500. Halving the answer itself would give 0.785. `stat-trace`
//     placed the id-64 call outside every row Mark 1 wraps, so every local
//     id-64 call is answered, and the character sheet and the damage scripts
//     that read id 64 see the halved value too.
//   - penalty-route: none. Neither the stat route nor an element route
//     removed the cast's slowdown, and the owner chose to ship it unchanged:
//     no penalty lever, and the stat line reads `penalty=unavailable`.
//
// The constants are the hub's hs_game_sdk/movement_speed_model.py's (k is its
// STEP_OFFSET, C its STEP_CAP, none here); tests/test_movement_speed_model.py's
// LeverParity pins them against it.
//
// This header decides; it never touches the game. The adapter in
// ModuleMain.cpp runs it inside the plugin's one shared ReturnSpecificStat
// detour (ReturnSpecificStatHook.hpp): for each call it reads whether the
// call's self is the local player and its stat-id argument, asks Wants, and
// only for a call the lever wants asks whether the bonus is active (at the
// point of use) and hands the answer to Answer. It is numbers in and numbers
// out, so tests/mark1_movement_harness.cpp compiles it whole.

inline constexpr std::string_view kSpeedRoute = "stat-model";
inline constexpr std::string_view kPenaltyRoute = "none";
inline constexpr bool kSpeedLever = true;     // a speed lever ships: `speed=` counts its calls
inline constexpr bool kPenaltyLever = false;  // none ships: `penalty=unavailable`

// ReturnSpecificStat's stat id for movement speed (the hub's RDM § 19.10).
inline constexpr double kMovementSpeedStatId = 64.0;
// k: the step is proportional to answer + kStepOffset (measured, fitted 200.0).
inline constexpr double kStepOffset = 200.0;
// C: the answer above which the step stops growing. None was reached.
inline constexpr bool kStepCapped = false;
inline constexpr double kStepCap = 0.0;
// The walk's step while the bonus is active, against the step without it.
inline constexpr double kWalkRatio = 0.5;

// The answer that makes the step kWalkRatio of the step at `answer`:
// kWalkRatio * (min(answer, C) + k) - k, which at a half is (min(x, C) - k) / 2.
inline double HalfWalkAnswer(double answer)
{
    const double counted = kStepCapped && answer > kStepCap ? kStepCap : answer;
    return kWalkRatio * (counted + kStepOffset) - kStepOffset;
}

struct SpeedCounters {
    uint64_t halved = 0;      // calls answered with the half-walk value (`speed=`)
    uint64_t notNumber = 0;   // wanted calls, bonus active, whose answer was not a finite number: left alone
};

class Speed {
public:
    // Is this call the lever's: the local player's own, asking for id 64?
    // Asked before the bonus is, so no other call costs anything more.
    static bool Wants(bool localPlayer, bool haveStatId, double statId)
    {
        return localPlayer && haveStatId && statId == kMovementSpeedStatId;
    }

    // One wanted call's answer, as the caller will read it. While the bonus
    // is inactive it is left as the game gave it. Otherwise a finite number
    // is replaced by HalfWalkAnswer and counted `halved` (the stat line's
    // `speed=`, so a lever that stopped applying shows as a count that stops
    // growing); anything else is left alone, never guessed at, and counted
    // `notNumber`. Each call is answered from its own original, so repeated
    // calls never compound. True when it changed.
    bool Answer(bool active, bool answerIsNumber, double& answer)
    {
        if (!active) return false;
        if (!answerIsNumber || !std::isfinite(answer)) { ++counters_.notNumber; return false; }
        answer = HalfWalkAnswer(answer);
        ++counters_.halved;
        return true;
    }

    const SpeedCounters& Stats() const { return counters_; }

private:
    SpeedCounters counters_;
};

} // namespace ForgePact::Mark1Movement
