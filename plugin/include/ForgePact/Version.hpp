#pragma once

// ForgePact's version, in the one place the plugin reads it from.
//
// A dedicated header rather than a #define buried in ModuleMain.cpp: it gives
// tools/cut_release.py an unambiguous anchor to rewrite, and plugin/include is
// already on the compiler's include path (plugin_build/build.bat), which is how
// MapRevealManager.hpp is found.
//
// Do NOT hand-edit this value. `py tools/cut_release.py <version>` moves every
// site at once and `--check` fails if they disagree.
#define FORGEPACT_VERSION "1.4.4"
