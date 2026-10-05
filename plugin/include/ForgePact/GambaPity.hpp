#pragma once

#include <cstdint>
#include <string>

namespace ForgePact::GambaPity {

// ---- gambapity's decision core (ForgePact #134 phase 3, player build) -------
//
// The Mods -> Quality of Life switch `gambapity` guarantees Goburin's Head
// (the unique charm at repository type 10 / sub 0 / base 98, key
// `charms_goburins_head`) from the gamba machine (Slot_Machine_01_obj) after
// the configured number of spins without it dropping. ModuleMain.cpp's adapter
// counts the two measured events - a spin, one machine-self PickUpGoldCheck
// with a1=-10000; and the prize build, one machine-self CreateDefaultParams
// whose third argument is truthy - and this header decides, per event, what
// the counter does and whether the build is forced to the charm.
//
// It is game-independent by contract - numbers and strings, never an instance,
// an RValue or a game constant - so tests/gamba_pity_harness.cpp compiles it
// whole, the way GambaProbe.hpp is. The adapter supplies the machine-self
// predicate (a bool, decided by the instance-handle rule in ModuleMain.cpp)
// and the charm identifier (type 10 / sub 0 / base 98, in ModuleMain.cpp);
// this core holds no file I/O and no game constants.

// One spin debits exactly 10,000 gold, so the counter - spins - reads as gold.
inline constexpr int64_t kGoldPerSpin = 10000;

class Pity {
public:
    // ---- the state the panel's switch and range set -------------------------
    // `gambapity <count>`: on with `count` as the threshold. `gambapity off`
    // keeps the counter, not reset; only a payout resets it.
    void SetEnabled(bool on) { enabled_ = on; }
    bool Enabled() const { return enabled_; }

    void SetThreshold(int threshold) { threshold_ = threshold; }
    int Threshold() const { return threshold_; }

    // The current spin count: the spins seen since the last reset.
    int Count() const { return count_; }

    // Restore the persisted count (the adapter reads the counter file; this
    // core holds no file I/O).
    void SetCount(int count) { count_ = count > 0 ? count : 0; }

    // The gold equivalent of the count, one spin = kGoldPerSpin gold. Named
    // once here so the status line shares it (the panel's value box shows the
    // spin count, not the gold).
    int64_t GoldEquivalent() const { return static_cast<int64_t>(count_) * kGoldPerSpin; }

    // ---- one pure decision per observed event -------------------------------
    // A spin: one machine-self PickUpGoldCheck with a1=-10000. Counts only
    // while on and machine-self; returns whether it counted. Nothing but a
    // spin moves the counter.
    bool OnSpin(bool machineSelf)
    {
        if (!enabled_ || !machineSelf) return false;
        ++count_;
        return true;
    }

    // The prize build: one machine-self CreateDefaultParams whose third
    // argument is truthy. Once the count has reached the threshold (a positive
    // one), force the charm and reset the counter; before the threshold (or
    // while off, or for another self) run the game's own build. Returns true
    // when the charm should be forced.
    bool OnPrizeRoll(bool machineSelf)
    {
        if (!enabled_ || !machineSelf || threshold_ <= 0 || count_ < threshold_) return false;
        count_ = 0;
        return true;
    }

    // A natural charm drop - the game's own roll gave the charm, not the mod's
    // force - starts the guarantee over.
    void OnNaturalDrop() { count_ = 0; }

    // `gambapity off`: disarmed, the counter kept.
    void Off() { enabled_ = false; }

    // `gambapity status`: one line naming on/off, `count=`, `threshold=` and
    // the gold equivalent. The bytes after `gambapity:` are the implementer's;
    // `count=` and `threshold=` must appear.
    std::string StatusLine() const
    {
        return std::string("gambapity: ") + (enabled_ ? "on" : "off")
            + " count=" + std::to_string(count_)
            + " threshold=" + std::to_string(threshold_)
            + " gold=" + std::to_string(GoldEquivalent());
    }

private:
    bool enabled_ = false;
    int threshold_ = 0;
    int count_ = 0;
};

} // namespace ForgePact::GambaPity
