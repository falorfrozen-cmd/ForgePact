#pragma once

// Shared includes for the ForgePact:: module classes.
//
// These headers are #included from inside ModuleMain.cpp's translation unit
// (not compiled as a separate object), at a point after ModuleMain's own
// Out()/Lower()/FirstToken()/HookOneScript()/g_Yytk/IPC_DIR/g_Base/
// g_RuntimeFrame/HhResolveLocalPlayer are already defined - see the
// "ForgePact:: module includes" anchor comment in ModuleMain.cpp. The
// ForgePact:: classes below deliberately do NOT redeclare any of those: an
// unqualified call to Out(...)/Lower(...)/etc. from inside `namespace
// ForgePact` falls through to the enclosing (translation-unit-global) scope
// and resolves to ModuleMain's real, tested implementation. An earlier
// version of these headers declared its own separate g_Yytk/Out/Lower/
// IPC_DIR - a second, divergent copy of state ModuleMain already owns - which
// is exactly the duplication this split is meant to remove.
//
// A class here should only add genuinely new state (e.g. an atomic flag) and
// logic, and lean on ModuleMain's existing globals and helpers for everything
// else.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <memory>
#include <filesystem>

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>
#include <hs_game_sdk/hs_game_sdk.hpp>
