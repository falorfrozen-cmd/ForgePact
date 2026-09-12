#pragma once

#include "Common.hpp"

namespace ForgePact {

// Monster Density's own configuration: the multiplier value, its fractional
// carry, and the "density <mult>" command.
//
// Deliberately NOT everything density-related: the actual instance-creation
// hooks (HookICD/HookICL), the multi-create logic (DoMultiCreate), the
// zone-revisit spatial-identity guard (DensityPlacementKey and friends), and
// the generation-window gate (DensityWindowActive/OpenDensityWindow) all stay
// in ModuleMain.cpp. They are a single shared chokepoint that Monster
// Density, Special Content's per-object multipliers, and Tyrant's Crown /
// Monster Rarity's enemy-born exclusion all route through - the same kind of
// coupling that kept Hook_DropRelic out of RelicFilterMod. Splitting a shared
// hook mid-migration is a bigger, riskier step than one module's worth of
// work; those functions read Mult/Frac from here instead of moving here.
//
// Mult and Frac are plain public fields rather than accessor methods on
// purpose: DoMultiCreate (the hottest, most failure-sensitive function in
// this file) increments, compares, and compound-assigns these in place, and
// every one of those call sites needed to change from a free global to this
// class without altering its arithmetic. A public field makes that a pure
// rename; a setter/getter redesign would have meant re-deriving each
// expression's shape by hand, in exactly the code most expensive to get
// subtly wrong.
class DensityManager {
public:
    static DensityManager& Instance() {
        static DensityManager s_Instance;
        return s_Instance;
    }

    double Mult{ 1.0 };   // ALL Enemy_Creator* spawners (density). May be fractional: 1.5, 2.5 ...
    double Frac{ 0.0 };   // fractional carry - at 1.5x every second spawner gets one extra copy

    // "density <mult>": clamps to native (>=1.0), installs the create/lifecycle
    // hooks lazily only when a non-vanilla value is requested, and opens a
    // fresh generation window so the new multiplier applies to the current zone.
    void HandleCommand(const std::string& rest) {
        try {
            double d = std::stod(rest);
            if (d < 1.0) d = 1.0;
            if (d > 1.0) {
                InstallCreateHooks();
                InstallDensityLifecycleHooks();
            }
            Mult = d;
            Frac = 0.0;   // kademe degisince birikim sifirlanir
            if (d > 1.0) OpenDensityWindow();
            char db[64]; sprintf_s(db, "%.2g", Mult);
            Out(std::string("density (creator mult) -> ") + db);
        }
        catch (...) { Out("density: bad value"); }
    }

private:
    DensityManager() = default;
};

} // namespace ForgePact
