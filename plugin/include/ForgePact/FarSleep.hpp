#pragma once

#include "Common.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ForgePact {

// Far sleep: a zone's far scenery props are put to sleep with
// instance_deactivate_object, and woken again as a player comes near.
//
// Why (measured 2026-09-28, docs/far-sleep-research.md): a Hero Siege zone
// holds 3,000-7,500 active instances, and all but a few hundred of them are
// far from the player: trees, bushes, hay, rocks, fences. The game already
// hides far props, but hidden is not asleep. The GameMaker runner still walks
// every active instance several times a frame: the draw walks over every
// layer, the alarm pass, the step upkeep. With 4,282 far props of Act_01_01
// asleep, the frame thread's work fell from about 62% of a 60 fps frame to
// about 47%. A room that ends destroys its sleeping props exactly like its
// awake ones: their Clean Up events run (counted live).
//
// What is touched:
//   - only descendants of the three scenery families below;
//   - never a descendant of a denied parent (shrines, dungeon entrances,
//     piles, chests, quest objects, monsters, walls, blocks);
//   - never an object that, or whose parent up to the family, runs code every
//     frame (a Step or Draw GUI event, asked of the compiled-code table);
//   - never a trap (`Trap_*`);
//   - never in town or in a persistent room, and not before the room has
//     settled with a player in it.
// A prop sleeps farther than the sleep radius from every player and wakes
// inside the (smaller) wake radius of any player. Both radii follow the
// camera view, so a prop is awake well before the game's own visibility pass
// could show it. Solid props (Collision_Prop_obj) also stay awake inside the
// radius where ForgePact keeps monsters hunting, so a monster that moves
// never walks through a sleeping fence.
//
// Cost model: every runner call counts against a per-frame budget, so no
// frame pays for a whole zone at once. The object table is classified once a
// session (two calls an object); a zone is scanned once, a slice a frame; a
// pass over the zone's props runs every ten frames and costs runner calls
// only for props that change state. A player who jumps (a teleport, a warp)
// starts a pass at once with a bigger budget.
//
// Everything below is header-only and reaches the game only through
// g_Yytk->CallBuiltin, so tests/far_sleep_harness.cpp can compile the real
// class against a controlled runner.
class FarSleep {
public:
    // Index order == family id.
    static constexpr const char* kFamilies[] = {
        "Visual_Parent_obj", "Destructible_NoCollision_Parent_obj", "Collision_Prop_obj",
    };
    static constexpr int kFamilyCount = 3;
    static constexpr int kSolidFamily = 2;   // Collision_Prop_obj: blocks movement
    static constexpr const char* kDenied[] = {
        "Shrine_Parent_obj", "Special_Dungeon_Parent_obj", "Pile_Parent_obj", "Chest_Parent_obj",
        "Quest_Object_Parent_obj", "Enemy_Parent_obj", "Wall_Parent_obj", "Block_obj",
    };
    static constexpr int kDeniedCount = 8;
    static constexpr const char* kPlayerObject = "Player_obj";
    static constexpr const char* kMinimapObject = "objMinimap";   // its instance and grid: the zone token

    static constexpr uint64_t kSettleFrames = 180;       // room unchanged this long before a scan
    static constexpr uint64_t kPlayerLookFrames = 30;    // a settled room without a player is asked again this often
    static constexpr uint64_t kTopUpFrames = 1800;       // one more scan this long after the first, for late props
    static constexpr uint64_t kSteadyGiveUpFrames = 600; // no steady instance count this long after settling: scan anyway
    static constexpr double kAllInstances = -3.0;        // GameMaker's `all`
    static constexpr uint64_t kPassFrames = 10;          // a sleep/wake pass at most this often
    static constexpr uint64_t kVerifyFrames = 60;        // a verification slice this often
    static constexpr size_t kVerifySlice = 32;
    static constexpr uint64_t kTokenFrames = 30;         // the zone token is read this often
    static constexpr uint64_t kViewPollFrames = 300;     // the camera size is re-read this often
    static constexpr uint32_t kPositionFreshFrames = 60; // a stored position younger than this is trusted
    static constexpr int kCallsPerFrame = 400;           // about half a millisecond of runner calls
    static constexpr int kUrgentCallsPerFrame = 2400;    // after a jump, or while waking everything
    static constexpr double kJumpPx = 400.0;
    static constexpr double kWakeMarginPx = 1000.0;      // wake radius = half the view diagonal + this
    static constexpr double kHysteresisPx = 600.0;       // sleep radius = wake radius + this
    static constexpr double kSolidMarginPx = 400.0;      // solid props: beyond the hunting radius + this
    static constexpr int kObjectGapStop = 256;           // this many missing indices in a row end the object table
    static constexpr int kMaxObjectIndex = 65536;
    static constexpr int kErrorsPerZone = 64;            // more failed calls than this and the zone is left alone

    // True when the named object itself owns an event that runs every frame
    // (Step, Begin/End Step, Draw GUI). Set by ModuleMain from the game's
    // compiled-code table; absent, nothing counts as owning one.
    using FrameEventOwner = bool (*)(const std::string& objectName);
    struct RoomInfo { std::string name; bool persistent = false; bool readable = true; };

    struct Prop {
        RValue handle;   // what instance_find answered (a reference on this runner)
        int64_t id;
        float x, y;
        uint32_t posFrame;
        uint8_t family;
        bool asleep;
        bool gone;
    };
    struct Stats {
        uint64_t sleeps = 0, wakes = 0, scans = 0, passes = 0, urgentPasses = 0, errors = 0;
        uint64_t zonesSkipped = 0, externalWakes = 0, restarts = 0, drains = 0;
    };

    static FarSleep& Instance() { static FarSleep s; return s; }

    bool Enabled() const { return m_Enabled; }
    void SetEnabled(bool on) {
        if (on == m_Enabled) return;
        m_Enabled = on;
        if (!on) { m_Draining = true; return; }   // wake everything asleep, a budget a frame
        // Switched back on before the wake-up finished: the props already
        // known (some still asleep) stay tracked, so none is left behind.
        m_Draining = false;
        m_ResumeAfterDrain = false;
        m_DrainPos = 0;
        if (m_Props.empty()) m_ZoneState = ZoneState::Waiting;
    }
    void SetFrameEventOwner(FrameEventOwner owner) { m_FrameEventOwner = owner; }
    // Research: address props by their plain numeric id instead of the
    // handle instance_find gave (A/B of the two argument kinds).
    void SetPlainIds(bool plain) { m_PlainIds = plain; }
    bool PlainIds() const { return m_PlainIds; }
    // How far from the player ForgePact keeps monsters stepping (its hunt wake
    // radius): 0 = the game's own box only, < 0 = the whole map.
    void SetHuntRadius(double radius) {
        if (radius == m_HuntRadius) return;
        m_HuntRadius = radius;
        m_HaveView = false;   // the radii are worked out again on the next frame
    }

    // Per-frame work from the frame callback. `roomKey` changes on every room
    // change; `roomInfo()` is asked once per room, after it has settled.
    template <class RoomProbe>
    void OnFrame(uint64_t frame, int64_t roomKey, RoomProbe roomInfo) {
        m_Calls = 0;
        m_Frame = frame;
        if (!m_Enabled && !m_Draining) return;
        try {
            if (roomKey != m_RoomKey) {
                m_RoomKey = roomKey;
                m_RoomSince = frame;
                ForgetZone();   // the room's instances, asleep or not, left with it
                m_TokenInstance = -1; m_TokenGrid = -1; m_LastToken = 0;
            }
            if (m_Draining) { Drain(); return; }
            if (!m_Classified) { Classify(); return; }
            if (m_ZoneState != ZoneState::Skipped) CheckZoneToken();
            switch (m_ZoneState) {
            case ZoneState::Waiting:
                if (frame < m_RoomSince + kSettleFrames) return;
                if (!m_RoomChecked) {
                    m_RoomChecked = true;
                    if (!ZoneAllowed(roomInfo())) { m_ZoneState = ZoneState::Skipped; ++m_Stats.zonesSkipped; return; }
                }
                if ((frame - m_RoomSince) % kPlayerLookFrames != 0) return;
                if (!ReadPlayers()) return;
                // A zone keeps adding props for a while after it starts: scan
                // once the instance count holds still between two looks.
                {
                    double all = -1;
                    if (!Number(Call("instance_number", { RValue(kAllInstances) }), all)) all = -1;
                    const double previous = m_LastCount;
                    m_LastCount = all;
                    const bool steady = all >= 0 && previous >= 0 && std::fabs(all - previous) <= (std::max)(8.0, previous * 0.005);
                    // A fight keeps the count moving: after ten more seconds
                    // scan anyway (the top-up scan adds what comes later).
                    if (!steady && frame < m_RoomSince + kSettleFrames + kSteadyGiveUpFrames) return;
                }
                m_ZoneState = ZoneState::Scanning;
                m_ScanObject = 0; m_ScanIndex = 0; m_ScanCount = -1;
                m_Props.clear(); m_Seen.clear();
                m_TopUpDone = false;
                return;
            case ZoneState::Scanning:
                Scan();
                return;
            case ZoneState::Running:
                Run();
                return;
            case ZoneState::Skipped:
                return;
            }
        } catch (...) {
            Error();
        }
    }

    // Diagnostics (`farsleep stat`, modstate).
    size_t Props() const { return m_Props.size(); }
    size_t Asleep() const { return m_AsleepCount; }
    size_t EligibleObjects() const { return m_Leaves.size(); }
    bool Classified() const { return m_Classified; }
    bool Draining() const { return m_Draining; }
    const char* ZoneStateName() const {
        switch (m_ZoneState) {
        case ZoneState::Waiting: return "waiting";
        case ZoneState::Scanning: return "scanning";
        case ZoneState::Running: return "running";
        case ZoneState::Skipped: return "skipped";
        }
        return "?";
    }
    double WakeRadius() const { return m_WakeRadius; }
    double SleepRadius() const { return m_SleepRadius; }
    double SolidWakeRadius() const { return m_SolidWakeRadius; }
    double SolidSleepRadius() const { return m_SolidSleepRadius; }
    const Stats& StatsRef() const { return m_Stats; }
    const std::vector<Prop>& PropList() const { return m_Props; }
    int CallsThisFrame() const { return m_Calls; }
    // The first player as last read: false before any read.
    bool FirstPlayer(double& x, double& y) const {
        if (m_Players.empty()) return false;
        x = m_Players[0].x; y = m_Players[0].y;
        return true;
    }

private:
    enum class ZoneState { Waiting, Scanning, Running, Skipped };
    struct Player { double x, y; };

    FarSleep() = default;

    static bool IsNumber(const RValue& v) {
        return v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64;
    }
    // A number or a typed reference (instances, objects and rooms answer
    // with references on this runner): its numeric value, or false.
    static bool Number(const RValue& v, double& out) {
        if (!IsNumber(v) && v.m_Kind != VALUE_REF) return false;
        out = v.ToDouble();
        return std::isfinite(out);
    }
    RValue Call(const char* name, std::vector<RValue> args) {
        ++m_Calls;
        return g_Yytk->CallBuiltin(name, args);
    }
    // Room for `cost` more runner calls this frame: a step is started only
    // when all of its calls fit.
    bool Budget(int cost = 1) const { return m_Calls + cost <= (m_Urgent ? kUrgentCallsPerFrame : kCallsPerFrame); }
    RValue Handle(const Prop& p) const { return m_PlainIds ? RValue(static_cast<double>(p.id)) : p.handle; }
    void Error() {
        ++m_Stats.errors;
        if (++m_ZoneErrors > kErrorsPerZone && m_ZoneState != ZoneState::Skipped) {
            // Something in this zone does not answer the way the rest of the
            // game does: wake what we put to sleep and leave the zone alone.
            m_ZoneState = ZoneState::Skipped;
            m_Draining = true;
            m_ResumeAfterDrain = m_Enabled;
        }
    }

    // ---- the object table, once a session -------------------------------
    int ObjectIndex(const char* name) {
        double v = -1;
        try { if (!Number(Call("asset_get_index", { RValue(name) }), v)) v = -1; } catch (...) { v = -1; }
        return v >= 0 ? static_cast<int>(v) : -1;
    }
    void Classify() {
        m_Urgent = false;
        if (m_ClassifyNext == 0 && m_Parent.empty()) {
            for (int f = 0; f < kFamilyCount; ++f) m_FamilyIndex[f] = ObjectIndex(kFamilies[f]);
            for (int d = 0; d < kDeniedCount; ++d) m_DeniedIndex[d] = ObjectIndex(kDenied[d]);
            m_PlayerIndex = ObjectIndex(kPlayerObject);
            m_MinimapIndex = ObjectIndex(kMinimapObject);
        }
        // Pass 1: every object's parent, two calls an object.
        while (!m_ParentsDone && Budget(2)) {
            const int i = m_ClassifyNext++;
            bool exists = false;
            try { exists = Call("object_exists", { RValue(static_cast<double>(i)) }).ToBoolean(); } catch (...) { exists = false; }
            if (!exists) {
                if (++m_Gap >= kObjectGapStop || i >= kMaxObjectIndex) m_ParentsDone = true;
                m_Parent.push_back(-1);
                continue;
            }
            m_Gap = 0;
            double p = -1;
            try { if (!Number(Call("object_get_parent", { RValue(static_cast<double>(i)) }), p)) p = -1; } catch (...) { p = -1; }
            m_Parent.push_back(p >= 0 ? static_cast<int>(p) : -1);
        }
        if (!m_ParentsDone) return;
        // Pass 2: family members, and which of them are ever a parent.
        if (m_Candidates.empty() && !m_CandidatesBuilt) {
            std::vector<uint8_t> isParent(m_Parent.size(), 0);
            for (int p : m_Parent) if (p >= 0 && p < static_cast<int>(isParent.size())) isParent[p] = 1;
            for (int i = 0; i < static_cast<int>(m_Parent.size()); ++i) {
                if (isParent[i]) continue;   // enumerate leaves only: a parent's count includes its children
                int family = -1;
                bool denied = false;
                int depth = 0;
                for (int o = i; o >= 0 && depth < 32; o = m_Parent[o], ++depth) {
                    for (int d = 0; d < kDeniedCount; ++d) if (m_DeniedIndex[d] >= 0 && o == m_DeniedIndex[d]) denied = true;
                    for (int f = 0; f < kFamilyCount; ++f) if (m_FamilyIndex[f] >= 0 && o == m_FamilyIndex[f] && family < 0) family = f;
                    if (o >= static_cast<int>(m_Parent.size())) break;
                }
                if (family >= 0 && !denied) m_Candidates.push_back({ i, static_cast<uint8_t>(family) });
            }
            m_CandidatesBuilt = true;
            m_CandidateNext = 0;
        }
        // Pass 3: names up the chain to the family, for the per-frame-code and
        // trap rules (a name is asked once, whatever shares it).
        while (m_CandidateNext < m_Candidates.size() && Budget(4)) {
            const Candidate c = m_Candidates[m_CandidateNext++];
            bool ok = true;
            int depth = 0;
            for (int o = c.object; o >= 0 && depth < 32; o = m_Parent[o], ++depth) {
                if (o == m_FamilyIndex[c.family]) break;   // the family roots own no per-frame code (checked below too)
                const std::string& name = Name(o);
                if (name.empty() || name.rfind("Trap_", 0) == 0 || (m_FrameEventOwner && m_FrameEventOwner(name))) { ok = false; break; }
            }
            if (ok && m_FrameEventOwner && m_FamilyIndex[c.family] >= 0 && m_FrameEventOwner(Name(m_FamilyIndex[c.family]))) ok = false;
            if (ok) m_Leaves.push_back(c);
        }
        if (m_CandidateNext >= m_Candidates.size()) m_Classified = true;
    }
    const std::string& Name(int object) {
        auto it = m_Names.find(object);
        if (it != m_Names.end()) return it->second;
        std::string name;
        try { RValue v = Call("object_get_name", { RValue(static_cast<double>(object)) }); if (v.m_Kind == VALUE_STRING) name = v.ToString(); } catch (...) {}
        return m_Names.emplace(object, name).first->second;
    }

    // ---- zones -----------------------------------------------------------
    static bool ZoneAllowed(const RoomInfo& room) {
        if (!room.readable || room.persistent) return false;
        if (room.name.rfind("Town", 0) == 0) return false;
        if (room.name.find("Menu") != std::string::npos) return false;
        // Developer rooms: HS-AFK-Expedition's Stronghold arena lives in
        // Dev_10_rm and walks its own monsters there.
        if (room.name.rfind("Dev_", 0) == 0) return false;
        return true;
    }
    void ForgetZone() {
        m_Props.clear(); m_Seen.clear();
        m_AsleepCount = 0;
        m_ZoneState = ZoneState::Waiting;
        m_RoomChecked = false;
        m_TopUpDone = false;
        m_LastCount = -1;
        m_ZoneErrors = 0;
        m_DrainPos = 0;
        m_PassPos = kNoPass;
        m_VerifyCursor = 0;
        m_LastPass = 0; m_LastVerify = 0; m_LastView = 0;
        m_HaveView = false;
        m_Players.clear(); m_PassPlayers.clear();
        if (m_Draining && m_ResumeAfterDrain) { m_Draining = false; m_ResumeAfterDrain = false; }
    }
    bool ReadPlayers() {
        m_Players.clear();
        if (m_PlayerIndex < 0) return false;
        double n = 0;
        if (!Number(Call("instance_number", { RValue(static_cast<double>(m_PlayerIndex)) }), n) || n < 1) return false;
        for (int i = 0; i < static_cast<int>(n) && i < 8; ++i) {
            const RValue inst = Call("instance_find", { RValue(static_cast<double>(m_PlayerIndex)), RValue(static_cast<double>(i)) });
            double x = 0, y = 0;
            if (!Number(Call("variable_instance_get", { inst, RValue("x") }), x)) continue;
            if (!Number(Call("variable_instance_get", { inst, RValue("y") }), y)) continue;
            m_Players.push_back({ x, y });
        }
        return !m_Players.empty();
    }
    void ReadView() {
        // Half the camera view's diagonal: how far a visible prop can be from
        // the view's centre. A missing camera keeps the 1229x691 measured in
        // the game on 2026-09-28.
        double w = 1229, h = 691;
        try {
            const RValue cam = Call("view_get_camera", { RValue(0.0) });
            double cw = 0, ch = 0;
            if (Number(Call("camera_get_view_width", { cam }), cw) && Number(Call("camera_get_view_height", { cam }), ch)
                && cw > 0 && ch > 0 && cw < 20000 && ch < 20000) { w = cw; h = ch; }
        } catch (...) {}
        m_WakeRadius = 0.5 * std::sqrt(w * w + h * h) + kWakeMarginPx;
        m_SleepRadius = m_WakeRadius + kHysteresisPx;
        // Solid props: awake everywhere monsters are kept hunting (plus a
        // margin), so a moving monster never meets a sleeping fence. A hunt
        // over the whole map keeps every solid prop awake.
        m_SolidWakeRadius = m_HuntRadius < 0 ? std::numeric_limits<double>::infinity()
            : (std::max)(m_WakeRadius, m_HuntRadius + kSolidMarginPx);
        m_SolidSleepRadius = m_SolidWakeRadius + kHysteresisPx;
        m_HaveView = true;
        m_LastView = m_Frame;
    }
    void Scan() {
        m_Urgent = false;
        while (m_ScanObject < m_Leaves.size() && Budget(4)) {
            const Candidate c = m_Leaves[m_ScanObject];
            const RValue obj(static_cast<double>(c.object));
            if (m_ScanCount < 0) {
                double n = 0;
                if (!Number(Call("instance_number", { obj }), n)) n = 0;
                m_ScanCount = static_cast<int>(n);
                m_ScanIndex = 0;
            }
            while (m_ScanIndex < m_ScanCount && Budget(3)) {
                const int i = m_ScanIndex++;
                try {
                    const RValue inst = Call("instance_find", { obj, RValue(static_cast<double>(i)) });
                    double id = -1, x = 0, y = 0;
                    if (!Number(inst, id) || id < 0) continue;
                    const int64_t key = static_cast<int64_t>(id);
                    if (!m_Seen.insert(key).second) continue;
                    if (!Number(Call("variable_instance_get", { inst, RValue("x") }), x)) continue;
                    if (!Number(Call("variable_instance_get", { inst, RValue("y") }), y)) continue;
                    m_Props.push_back({ inst, key, static_cast<float>(x), static_cast<float>(y), static_cast<uint32_t>(m_Frame), c.family, false, false });
                } catch (...) { Error(); }
            }
            if (m_ScanIndex >= m_ScanCount) { ++m_ScanObject; m_ScanCount = -1; }
        }
        if (m_ScanObject >= m_Leaves.size()) {
            m_ZoneState = ZoneState::Running;
            ++m_Stats.scans;
            m_PassPos = kNoPass;
            m_LastPass = 0;
            m_ScanDoneFrame = m_Frame;
        }
    }

    // ---- sleeping and waking ---------------------------------------------
    double Nearest2(const Prop& p, const std::vector<Player>& players) const {
        double best = std::numeric_limits<double>::infinity();
        for (const Player& pl : players) {
            const double dx = p.x - pl.x, dy = p.y - pl.y;
            best = (std::min)(best, dx * dx + dy * dy);
        }
        return best;
    }
    // A player the last pass knew is now far from where it was then (a
    // teleport, a warp, a very fast dash). A player joining or leaving is not
    // a jump: the next ordinary pass covers it.
    bool Jumped() const {
        if (m_PassPlayers.size() != m_Players.size()) return false;
        for (size_t i = 0; i < m_Players.size(); ++i) {
            const double dx = m_Players[i].x - m_PassPlayers[i].x, dy = m_Players[i].y - m_PassPlayers[i].y;
            if (dx * dx + dy * dy > kJumpPx * kJumpPx) return true;
        }
        return false;
    }
    void Run() {
        if (!m_HaveView || m_Frame >= m_LastView + kViewPollFrames) ReadView();
        // The player is read every frame (a few calls): a jump starts a pass
        // at once, with the bigger budget.
        if (!ReadPlayers()) return;
        const bool jumped = Jumped();
        if (jumped || (m_PassPos == kNoPass && m_Frame >= m_LastPass + kPassFrames)) {
            if (jumped) ++m_Stats.urgentPasses;
            m_Urgent = jumped;
            m_PassPlayers = m_Players;
            m_PassPos = 0;
            m_PassWaking = true;
            m_LastPass = m_Frame;
            ++m_Stats.passes;
        }
        if (m_PassPos != kNoPass) Pass();
        if (m_PassPos == kNoPass && m_Frame >= m_LastVerify + kVerifyFrames && Budget()) Verify();
        // One top-up scan per zone: props the zone made after the first scan
        // are added (the scan skips every id it has already seen).
        if (m_PassPos == kNoPass && !m_TopUpDone && m_ZoneState == ZoneState::Running
            && m_Frame >= m_ScanDoneFrame + kTopUpFrames) {
            m_TopUpDone = true;
            m_ZoneState = ZoneState::Scanning;
            m_ScanObject = 0; m_ScanIndex = 0; m_ScanCount = -1;
        }
    }
    void Pass() {
        // Waking first: a prop that should be awake matters more than one
        // that could sleep.
        while (m_PassWaking && m_PassPos < m_Props.size() && Budget()) {
            Prop& p = m_Props[m_PassPos++];
            if (!p.asleep || p.gone) continue;
            const double w = p.family == kSolidFamily ? m_SolidWakeRadius : m_WakeRadius;
            if (std::isfinite(w) && Nearest2(p, m_PassPlayers) > w * w) continue;
            try {
                Call("instance_activate_object", { Handle(p) });
                p.asleep = false;
                --m_AsleepCount;
                ++m_Stats.wakes;
            } catch (...) { Error(); }
        }
        if (m_PassWaking && m_PassPos >= m_Props.size()) { m_PassWaking = false; m_PassPos = 0; }
        while (!m_PassWaking && m_PassPos < m_Props.size() && Budget(4)) {
            Prop& p = m_Props[m_PassPos++];
            if (p.asleep || p.gone) continue;
            const double r = p.family == kSolidFamily ? m_SolidSleepRadius : m_SleepRadius;
            if (!std::isfinite(r) || Nearest2(p, m_PassPlayers) <= r * r) continue;
            try {
                // A prop can move while awake (the ravens fly): trust a stored
                // position only while it is fresh. A prop broken meanwhile is
                // asked nothing but whether it exists.
                if (m_Frame > p.posFrame + kPositionFreshFrames) {
                    const RValue id = Handle(p);
                    if (!Call("instance_exists", { id }).ToBoolean()) { p.gone = true; continue; }
                    double x = 0, y = 0;
                    if (!Number(Call("variable_instance_get", { id, RValue("x") }), x)
                        || !Number(Call("variable_instance_get", { id, RValue("y") }), y)) { p.gone = true; continue; }
                    p.x = static_cast<float>(x); p.y = static_cast<float>(y); p.posFrame = static_cast<uint32_t>(m_Frame);
                    if (Nearest2(p, m_PassPlayers) <= r * r) continue;
                }
                Call("instance_deactivate_object", { Handle(p) });
                p.asleep = true;
                ++m_AsleepCount;
                ++m_Stats.sleeps;
            } catch (...) { p.gone = true; Error(); }
        }
        if (!m_PassWaking && m_PassPos >= m_Props.size()) { m_PassPos = kNoPass; m_Urgent = false; }
    }
    // A rotating slice checks what the game did meanwhile: a sleeping prop
    // that exists again was woken by someone else (the game's own activation
    // calls); an awake one that no longer exists was destroyed.
    void Verify() {
        m_LastVerify = m_Frame;
        if (m_Props.empty()) return;
        for (size_t n = 0; n < kVerifySlice && n < m_Props.size() && Budget(); ++n) {
            Prop& p = m_Props[m_VerifyCursor];
            m_VerifyCursor = (m_VerifyCursor + 1) % m_Props.size();
            if (p.gone) continue;
            bool exists = false;
            try { exists = Call("instance_exists", { Handle(p) }).ToBoolean(); } catch (...) { Error(); continue; }
            if (p.asleep && exists) { p.asleep = false; --m_AsleepCount; ++m_Stats.externalWakes; p.posFrame = 0; }
            else if (!p.asleep && !exists) p.gone = true;
        }
    }
    // The zone's own identity under an unchanged room key: the minimap
    // object's instance and the grid it keeps (the pair map reveal uses). A
    // restart makes both anew, so a change means the props we know are gone
    // and the zone is scanned again.
    void CheckZoneToken() {
        if (m_Frame < m_LastToken + kTokenFrames) return;
        m_LastToken = m_Frame;
        if (m_MinimapIndex < 0) return;
        double inst = -1, grid = -1;
        try {
            const RValue id = Call("instance_find", { RValue(static_cast<double>(m_MinimapIndex)), RValue(0.0) });
            if (!Number(id, inst) || inst < 0) return;
            if (!Number(Call("variable_instance_get", { id, RValue("minimapDiscoveredGrid") }), grid)) return;
        } catch (...) { return; }
        if (m_TokenInstance < 0) { m_TokenInstance = inst; m_TokenGrid = grid; return; }
        if (inst == m_TokenInstance && grid == m_TokenGrid) return;
        ++m_Stats.restarts;
        ForgetZone();
        m_RoomSince = m_Frame;
        m_TokenInstance = inst; m_TokenGrid = grid;
    }
    // Wake everything this class put to sleep (switched off, or a zone that
    // misbehaved), a budget a frame.
    void Drain() {
        m_Urgent = true;
        for (; m_DrainPos < m_Props.size() && Budget(); ++m_DrainPos) {
            Prop& p = m_Props[m_DrainPos];
            if (!p.asleep || p.gone) continue;
            try { Call("instance_activate_object", { Handle(p) }); ++m_Stats.wakes; } catch (...) { ++m_Stats.errors; }
            p.asleep = false;
            --m_AsleepCount;
        }
        if (m_DrainPos < m_Props.size()) return;
        m_DrainPos = 0;
        m_Draining = false;
        m_Urgent = false;
        ++m_Stats.drains;
        if (m_ResumeAfterDrain) { m_ResumeAfterDrain = false; return; }   // the zone stays Skipped; props stay known
        m_Props.clear(); m_Seen.clear(); m_AsleepCount = 0;
        m_ZoneState = ZoneState::Waiting;
    }

    struct Candidate { int object; uint8_t family; };
    static constexpr size_t kNoPass = static_cast<size_t>(-1);

    bool m_Enabled = false;
    bool m_PlainIds = false;
    bool m_Draining = false;
    bool m_ResumeAfterDrain = false;
    FrameEventOwner m_FrameEventOwner = nullptr;
    double m_HuntRadius = 0.0;
    uint64_t m_Frame = 0;
    int m_Calls = 0;
    bool m_Urgent = false;
    Stats m_Stats;

    // object table
    bool m_Classified = false, m_ParentsDone = false, m_CandidatesBuilt = false;
    int m_ClassifyNext = 0, m_Gap = 0;
    int m_FamilyIndex[kFamilyCount] = { -1, -1, -1 };
    int m_DeniedIndex[kDeniedCount] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int m_PlayerIndex = -1;
    std::vector<int> m_Parent;
    std::vector<Candidate> m_Candidates, m_Leaves;
    size_t m_CandidateNext = 0;
    std::unordered_map<int, std::string> m_Names;

    // zone
    int64_t m_RoomKey = (std::numeric_limits<int64_t>::min)();
    uint64_t m_RoomSince = 0;
    ZoneState m_ZoneState = ZoneState::Waiting;
    bool m_RoomChecked = false;
    bool m_TopUpDone = false;
    double m_LastCount = -1;
    uint64_t m_ScanDoneFrame = 0;
    int m_MinimapIndex = -1;
    double m_TokenInstance = -1, m_TokenGrid = -1;
    uint64_t m_LastToken = 0;
    int m_ZoneErrors = 0;
    size_t m_ScanObject = 0;
    int m_ScanIndex = 0, m_ScanCount = -1;
    std::vector<Prop> m_Props;
    std::unordered_set<int64_t> m_Seen;
    size_t m_AsleepCount = 0;
    std::vector<Player> m_Players, m_PassPlayers;
    size_t m_PassPos = kNoPass;
    bool m_PassWaking = true;
    uint64_t m_LastPass = 0, m_LastVerify = 0, m_LastView = 0;
    size_t m_VerifyCursor = 0, m_DrainPos = 0;
    bool m_HaveView = false;
    double m_WakeRadius = 0, m_SleepRadius = 0, m_SolidWakeRadius = 0, m_SolidSleepRadius = 0;
};

}   // namespace ForgePact
