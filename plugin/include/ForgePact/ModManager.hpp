#pragma once

#include "Common.hpp"
#include "Version.hpp"

namespace ForgePact {

// The plugin's own startup bookkeeping - deliberately NOT an Aurie entry
// point.
//
// ModuleInitialize (EXPORTED, resolved by the Aurie loader by symbol name)
// and FrameCallback (registered with YYTK::CreateCallback as a bare function
// pointer) cannot become methods on this class: Aurie's module ABI needs an
// exported free function for the former, and CreateCallback takes a raw
// function pointer for the latter, not a member function. They are the last
// two shared chokepoints in this file, the same category as DoMultiCreate
// and RunCommand - and every other class from this split is already called
// out of their real bodies (MapRevealManager and IpcServer directly;
// StatsManager, RelicFilterMod, DensityManager and DropManager through the
// hooks FrameCallback installs and the commands RunCommand dispatches).
//
// An earlier restored draft of this header registered its own EVENT_FRAME
// callback and re-implemented a two-line frame tick here - that would have
// run *instead of* the real FrameCallback (whichever callback Aurie invoked
// last would have won) and silently dropped every feature not itself: setup
// gating, hotkeys, the stall watchdog, Headhunter/orb-pickup ticks, and the
// research-only co-op/companion/buff ticks. That draft was never wired in,
// so it was dead code, not a live bug - but it is deliberately NOT restored
// here.
//
// What IS safely this class's own is the one piece of startup bookkeeping
// that carries no ABI constraint: creating bp_ipc\ and logging the load
// banner. ModuleInitialize calls Initialize() for exactly that, once the
// YYTK interface is confirmed non-null.
class ModManager {
public:
    static ModManager& Instance() {
        static ModManager s_Instance;
        return s_Instance;
    }

    void Initialize() {
        CreateDirectoryA(IPC_DIR.c_str(), nullptr);
        // The version goes AFTER the marker, never inside it: the panel's
        // plugin_boot_count() counts occurrences of the literal "BloodPact
        // plugin loaded" to notice a new game process, and interpolating the
        // version would break auto-apply after a restart with no error
        // anywhere. An out.txt that does not say which build wrote it makes
        // every bug report about rates or timing ambiguous.
        Out("==== BloodPact plugin loaded ==== v" FORGEPACT_VERSION);
    }

private:
    ModManager() = default;
};

} // namespace ForgePact
