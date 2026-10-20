#pragma once
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <hs_game_sdk/scripts.hpp>

// Number guard (#230, `numguard`): caps a number the game's Steam number
// shortener cannot show, so a big hit or DPS sum no longer ends the game.
//
// num_compressSteam hands its one number to SteamCompressNumber in
// PAS_Steam_GMS_x64.dll, which writes the short text into an 8-byte buffer.
// Every write there is range-checked, and a text that does not fit ends the
// process with exit 0xC0000409 (a fail-fast no in-process handler sees). By
// the static reading that happens for any number of 100 trillion or more, a
// million or more below zero, and NaN (hub docs/models/number-compress-spec.md).
// With Modifiers raised (Total Damage x11, Critical Strike Damage x11,
// Critical Strike Chance x6) a training dummy reaches it.
//
// So the detour judges the number it is handed and, past the limit, hands
// the shortener the largest number it can show instead ("99999B"), the
// smallest below zero, or 0 for NaN. Only the text changes: the damage dealt,
// the DPS sums and enemy health are the game's, untouched. The caller's
// value is never written; the cap goes into a substitute argument array, as
// CombatTextHook.hpp does. The original always runs and its result is
// returned.
//
// num_compressSteam runs through a method value (num_compressFunc binds it)
// and has no direct call site, so only the inline detour at the function
// sees it. A table-only or failed install is not armed and says so once.
//
// Arming: Arm() is called only by a Modifiers lever set off vanilla
// (StatsManager's `stat` and `statadd`, SkillSlidersMod's `skillslider`) and,
// in the research build, by `numguard trace on`. With every lever at vanilla
// nothing is hooked. The detour goes in at most once a session.
//
// Included by StatsManager.hpp and SkillSlidersMod.hpp. It includes nothing
// that pulls in the Windows headers and stays portable C++, because
// tests/number_guard_harness.cpp compiles it with fake game types. Out,
// Lower, FirstToken, HookOneScript and SdkShortScriptName are ModuleMain's.
namespace ForgePact::NumberGuard {

// The game's limits (static reading; the hub's number_compress_model pins
// the same two) and the values ForgePact hands the shortener instead.
inline constexpr double kNumGuardPositiveLimit = 1e14;          // smallest positive that fails
inline constexpr double kNumGuardNegativeLimit = -1e6;          // largest negative that fails
inline constexpr double kNumGuardLargestSafe = 99999000000000.0; // shown "99999B"
inline constexpr double kNumGuardSmallestSafe = -999999.0;      // shown "-999999"

enum Route : int { kRouteNone = 0, kRouteNative, kRouteTableOnly, kRouteFailed };
// The research trace's sites: the number as handed to the shortener, and
// CalculateEndDamage's result as the game built it and as returned.
enum TraceSite : int { kSiteCompress = 0, kSiteEndDamageNative, kSiteEndDamageReturned, kSiteCount };
inline constexpr int kTraceLinesMax = 400;

// The whole state, so the harness can start each scenario from a fresh one.
struct State {
    PFUNC_YYGMLScript original = nullptr;
    bool installTried = false;
    int route = kRouteNone;
    // Both builds: how many numbers were capped, and the first one.
    uint64_t clamped = 0;
    bool haveFirst = false;
    double first = 0.0;
    bool firstLineWritten = false;
#ifndef FORGEPACT_RELEASE
    bool forcedOff = false;          // `numguard off`: every value passes, for the session
    bool traceOn = false;
    uint64_t calls = 0;
    double largest = 0.0;            // largest |value| handed in, before any cap
    int traceLines = 0;
    bool traceLimitSaid = false;
    bool traceSeen[kSiteCount] = {};
    double traceMax[kSiteCount] = {};
#endif
};
inline State state;

#ifndef FORGEPACT_RELEASE
// StatsManager's CalculateEndDamage install for the trace. This header comes
// before that class, so ModuleMain's `numguard` dispatch hands it in.
inline bool (*installEndDamageTrace)() = nullptr;
#endif

inline const char* RouteName(int route) {
    switch (route) {
        case kRouteNative: return "native";
        case kRouteTableOnly: return "table-only";
        case kRouteFailed: return "failed";
        default: return "none";
    }
}

inline std::string Num(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char b[48];
    std::snprintf(b, sizeof b, "%.15g", v);
    return b;
}

// True for a value SteamCompressNumber cannot show (+inf and -inf included).
inline bool Fails(double v) {
    return std::isnan(v) || v >= kNumGuardPositiveLimit || v <= kNumGuardNegativeLimit;
}

// What the shortener is handed instead of a value that Fails().
inline double SafeValue(double v) {
    if (std::isnan(v)) return 0.0;
    return v > 0.0 ? kNumGuardLargestSafe : kNumGuardSmallestSafe;
}

inline bool IsNumber(const RValue* v) {
    return v && (v->m_Kind == VALUE_REAL || v->m_Kind == VALUE_INT32 || v->m_Kind == VALUE_INT64);
}

#ifndef FORGEPACT_RELEASE
inline const char* SiteName(int site) {
    switch (site) {
        case kSiteCompress: return "num_compressSteam";
        case kSiteEndDamageNative: return "CalculateEndDamage.native";
        case kSiteEndDamageReturned: return "CalculateEndDamage.returned";
        default: return "?";
    }
}

inline int Decade(double magnitude) {
    return magnitude > 0.0 ? (int)std::floor(std::log10(magnitude)) : -400;
}

// One line when a site's |value| reaches a decade above the largest it has
// seen (its first value included), and one for every non-finite value; at
// most kTraceLinesMax a session. Out() writes and closes out.txt before it
// returns, so a line written before the shortener runs survives a fail-fast.
inline void TraceValue(int site, double v) {
    State& s = state;
    if (!s.traceOn || site < 0 || site >= kSiteCount) return;
    bool write = !std::isfinite(v);
    if (!write) {
        const double magnitude = std::fabs(v);
        write = !s.traceSeen[site] || Decade(magnitude) > Decade(s.traceMax[site]);
        if (!s.traceSeen[site] || magnitude > s.traceMax[site]) s.traceMax[site] = magnitude;
        s.traceSeen[site] = true;
    }
    if (!write) return;
    if (s.traceLines >= kTraceLinesMax) {
        if (!s.traceLimitSaid) { s.traceLimitSaid = true; Out("numguard trace: limit reached"); }
        return;
    }
    ++s.traceLines;
    Out(std::string("numguard trace: ") + SiteName(site) + " " + Num(v));
}

// StatsManager's CalculateEndDamage hook hands its result in here.
inline void TraceResult(int site, const RValue& r) {
    if (!state.traceOn || !IsNumber(&r)) return;
    try { TraceValue(site, r.ToDouble()); } catch (...) {}
}
#endif

inline RValue& Hook(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    State& s = state;
    RValue substitute;
    std::vector<RValue*> A2;
    RValue** use = A;
    bool capped = false;
    double handed = 0.0;
#ifndef FORGEPACT_RELEASE
    ++s.calls;
#endif
    if (A && argc > 0 && IsNumber(A[0])) {
        bool read = false;
        try { handed = A[0]->ToDouble(); read = true; } catch (...) {}
        if (read) {
#ifndef FORGEPACT_RELEASE
            if (!std::isnan(handed) && std::fabs(handed) > s.largest) s.largest = std::fabs(handed);
            // Before the original: the last line before a fail-fast names the value.
            TraceValue(kSiteCompress, handed);
            const bool pass = s.forcedOff;
#else
            const bool pass = false;
#endif
            if (!pass && Fails(handed)) {
                try {
                    substitute = RValue(SafeValue(handed));
                    A2.assign(A, A + argc);
                    A2[0] = &substitute;
                    use = A2.data();
                    capped = true;
                } catch (...) {}
            }
        }
    }
    if (capped) {
        ++s.clamped;
        if (!s.haveFirst) { s.haveFirst = true; s.first = handed; }
    }
    if (!s.original) return R;
    RValue& result = s.original(S, O, R, argc, use);
    if (capped && !s.firstLineWritten) {
        s.firstLineWritten = true;
        std::string line = "numguard: capped " + Num(handed) + " -> " + Num(SafeValue(handed));
        try {
            if (result.m_Kind == VALUE_STRING) line += " (shown \"" + std::string(result.ToString()) + "\")";
        } catch (...) {}
        Out(line);
    }
    return result;
}

// True when the detour is in and native. The one HookOneScript call for
// num_compressSteam in the plugin, hook id `fp_numguard`, tried at most once
// a session; the route it took is kept for the status line.
inline bool Install() {
    State& s = state;
    if (!s.installTried) {
        s.installTried = true;
        bool native = false;
        const bool ok = HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_num_compressSteam),
            "fp_numguard", (void*)Hook, &s.original, &native);
        s.route = (!ok || !s.original) ? kRouteFailed : (native ? kRouteNative : kRouteTableOnly);
        if (s.route != kRouteNative)
            Out(std::string("numguard: num_compressSteam hook is ") + RouteName(s.route)
                + " - numbers past the display limit are not capped");
    }
    return s.route == kRouteNative;
}

// A Modifiers lever was set off vanilla.
inline void Arm() { Install(); }

inline std::string StatusLine() {
    const State& s = state;
    std::string line = std::string("numguard: ") + (s.route == kRouteNative ? "armed" : "not armed")
        + " route=" + RouteName(s.route) + " clamped=" + std::to_string(s.clamped)
        + " first=" + (s.haveFirst ? Num(s.first) : std::string("-"));
#ifndef FORGEPACT_RELEASE
    line += std::string(" forced=") + (s.forcedOff ? "off" : "auto") + " trace=" + (s.traceOn ? "on" : "off")
        + " calls=" + std::to_string(s.calls) + " largest=" + Num(s.largest);
#endif
    return line;
}

// "numguard", "numguard stat"; the research build adds "numguard off",
// "numguard auto" and "numguard trace on|off".
inline void HandleCommand(const std::string& rest) {
    std::string arg;
    std::string sub = Lower(FirstToken(rest, arg));
    while (!sub.empty() && std::isspace((unsigned char)sub.back())) sub.pop_back();
    if (sub.empty() || sub == "stat") { Out(StatusLine()); return; }
#ifdef FORGEPACT_RELEASE
    Out("numguard: " + sub + " is available in the research build only");
#else
    State& s = state;
    if (sub == "off") {
        s.forcedOff = true;
        Out("numguard: forced off - every number reaches the shortener as given, still counted and traced");
        return;
    }
    if (sub == "auto") {
        s.forcedOff = false;
        Out("numguard: forced auto - a number past the display limit is capped while armed");
        return;
    }
    if (sub == "trace") {
        std::string v = Lower(arg);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        while (!v.empty() && std::isspace((unsigned char)v.front())) v.erase(v.begin());
        if (v == "on" || v == "1") {
            s.traceOn = true;
            Arm();
            const bool damage = installEndDamageTrace && installEndDamageTrace();
            Out(std::string("numguard trace -> on (num_compressSteam hook ") + RouteName(s.route)
                + ", CalculateEndDamage " + (damage ? "hooked" : "not hooked") + ")");
            return;
        }
        if (v == "off" || v == "0") {
            s.traceOn = false;
            Out("numguard trace -> off (" + std::to_string(s.traceLines) + " lines written)");
            return;
        }
        Out("numguard trace: usage -> numguard trace on|off");
        return;
    }
    Out("numguard: unknown '" + sub + "' (stat, off, auto, trace on|off)");
#endif
}
} // namespace ForgePact::NumberGuard
