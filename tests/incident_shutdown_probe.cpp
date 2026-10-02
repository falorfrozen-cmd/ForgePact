// The clean-shutdown marker in a DLL of its own, loaded by
// tests/incident_monitor_harness.cpp in a child process. No game, Aurie or
// YYToolkit is involved: what is under test is how a DLL's static destructor
// meets the two ways the game's process ends.
//
// ProbeArm does what ModuleMain.cpp's IncidentMonitorStart does: it arms
// ForgePact::Incident::g_ShutdownMarker with a path and starts the monitor's
// thread on its heap-held singleton (an ExitSafeThread), here running a loop
// that only sleeps. Then the child ends:
//
//   ExitProcess       -> Windows ends the other threads and runs this DLL's
//                        static destructors at DLL_PROCESS_DETACH, so the
//                        marker's second route appends its line,
//                        "==== clean shutdown (detach) ====" (the plugin's
//                        first route, its ExitProcess hook, needs Aurie and
//                        is not here);
//   TerminateProcess  -> no destructor runs, as after a crash, so there is
//                        no marker.
//
// Built with /MD /LD, as the plugin is.

#include <ForgePact/IncidentMonitor.hpp>

namespace {

void ProbeLoop() noexcept
{
    for (;;) Sleep(50);
}

} // namespace

extern "C" __declspec(dllexport) int ProbeArm(const char* path)
{
    ForgePact::Incident::g_ShutdownMarker.Arm(path);
    if (!ForgePact::Incident::g_ShutdownMarker.Armed()) return 0;
    return ForgePact::Incident::Monitor::Instance().Start(&ProbeLoop) ? 1 : 0;
}
