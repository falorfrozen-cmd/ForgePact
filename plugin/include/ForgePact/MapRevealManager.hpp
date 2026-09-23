#pragma once

#include "Common.hpp"
#include <ForgePact/PackAdmissionQueue.hpp>
#include <ForgePact/AdaptivePopulationBudget.hpp>

namespace ForgePact {

// Auto map-reveal: fill each zone's discovered grid once per zone/instance
// identity, instead of clearing it every N frames for the whole session (the
// original approach produced avoidable GameMaker calls during dense combat).
// Panel command: `reveal 1` / `reveal 0` (the panel's `map_reveal` checkbox).
//
// Two halves, both measured live 2026-09-11 (docs/map-reveal-research.md):
//
// 1. Fog.  `ds_grid_clear(minimapDiscoveredGrid, 1)`.  Measured: this alone
//    already reveals every *static* icon - mechanics, waypoints, dungeon
//    entrances, chests, shrines.  `isDiscovered` does NOT gate those icons
//    (setting it under fog changed nothing; clearing fog with it still 0
//    revealed them), so no per-object sweep is needed or done.
//
// 2. Mob packs.  Monsters were the one thing fog could not reveal, and the
//    reason turned out not to be a draw gate at all: most packs *do not
//    exist yet*.  `Enemy_Creator_*` spawners sit in the room and only give
//    birth once `distance_to_object(Player_obj)` drops under ~1050 px, so an
//    unexplored zone holds a few hundred creators and only the handful of
//    packs the player already walked past.  Measured in one zone: 310
//    spawners, all awake, 208 enemies - and 1273 enemies once the spawners
//    were told the player was adjacent.  So "reveal the whole map" needs the
//    zone populated, not a visibility flag flipped.
//
//    A temporary pass lets ready creators see distance zero with a five-second
//    throughput target, 32-group ceiling and 4-8ms work budget. Storage is reserved
//    before admission. Ready callers never wait for an absent queue head. The pass
//    extends while work is deferred, then returns to a relaxed atomic check.
//    Native events still create the packs and all density copies normally.
class MapRevealManager {
public:
    static MapRevealManager& Instance() {
        static MapRevealManager s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled; }
    bool HasReadableMap() const {
        int64_t room=INT64_MIN,instance=INT64_MIN,grid=INT64_MIN;
        return ReadIdentity(room,instance,grid);
    }
    void SetEnabled(bool enabled) {
        m_Enabled = enabled;
        if (!m_Enabled) { CloseSpawnWindow(); }
        else ResetIdentity();   // re-arm: the current zone gets a pass too
        if (m_Enabled && m_Packs) PreparePopulationCapacity();
        Out(std::string("reveal: ") + (m_Enabled ? "ACIK" : "KAPALI"));
    }
    void Toggle() { SetEnabled(!m_Enabled); }

    // `reveal packs 0|1` - the monster half of the map, drawn as one marker
    // per unspawned spawner (ForgePact::PackMarkers) and costing no monsters.
    // Since 2026-09-22 this is what the panel's nested `map_reveal_packs`
    // checkbox means; the marker state itself lives in PackMarkers, which
    // reads this flag together with IsEnabled().
    bool MarksEnabled() const { return m_Marks; }
    void SetMarks(bool on) { m_Marks = on; Out(std::string("reveal packs (markers): ") + (m_Marks ? "ACIK" : "KAPALI")); }

    // Zone identity changes, counted: PackMarkers clears its list when this
    // moves, without either class including the other.
    uint64_t ZoneGeneration() const { return m_ZoneGeneration; }

    // `reveal spawn 0|1` - the old "fill the map" pass: every spawner is told
    // the player is adjacent, so the whole zone's packs really exist. Off by
    // default since 2026-09-22 because a zone's worth of living monsters is
    // what the game cannot afford per frame (docs/population-performance-
    // analysis.md); kept as an explicit opt-in (panel: `map_reveal_spawn`).
    bool PacksEnabled() const { return m_Packs; }
    void SetPacks(bool on) {
        const bool was = m_Packs;
        m_Packs = on;
        if (m_Packs && m_Enabled) PreparePopulationCapacity();
        if (!m_Packs) { CloseSpawnWindow(); }
        // REPORTED 2026-09-12 (PR #2 issue 3): turning packs on used to set
        // this flag and nothing else. Tick() returns early while the zone
        // identity is unchanged, so the zone the player is standing in was
        // never armed - the checkbox claimed to apply live and then did
        // nothing until the next zone change or a reveal off/on cycle.
        //
        // Arm the current zone instead of opening the window directly: the
        // readiness gate in TryOpenSpawnWindow is the whole reason the pack
        // pass is safe, and skipping it here would reintroduce the inert-
        // creator bug by a new route.
        if (m_Packs && !was && m_Enabled) { m_PacksPending = true; m_PendingTicks = 0; }
        Out(std::string("reveal spawn (fill the map): ") + (m_Packs ? "ACIK" : "KAPALI"));
    }

    // Cheap pre-filter for Hook_distance_to_object: one relaxed atomic load,
    // false for all but a few seconds per zone. It answers "is a pack pass
    // running at all", NOT "may this creator be lied to" - see MayPopulate.
    bool WantsPackSpawn() const { return m_SpawnWindow.load(std::memory_order_relaxed) > 0; }

    // The authorization, evaluated at the exact point the distance result
    // would be changed, for the ONE creator it would be changed for.
    //
    // REPORTED 2026-09-12 (PR #2 issue 2, second round). The previous fix
    // invalidated a stale window in OnFrame - but OnFrame runs at EVENT_FRAME,
    // which this YYToolkit dispatches from HkPresent, i.e. at the END of the
    // frame. The creators run their step events BEFORE that. So on the first
    // frame in a new zone the distance hook could still consume the previous
    // zone's permission, and the window only closed afterwards - too late for
    // exactly the call that does the damage. A render-time check cannot
    // guarantee anything about a step-time consumer, and no amount of extra
    // identity tracking at Present would have fixed that ordering.
    //
    // So the check moved to the consumer, and to the thing that actually
    // matters. The damage mechanism is specific: answering 0 to a creator
    // that has not finished initialising makes it take its spawn branch once,
    // early, and come out inert. That is a property of the creator in hand,
    // not of the zone, the room key, or the map identity - so ask the
    // creator. A creator reporting a real enemyCreatorTimer is initialised,
    // and lying to it is safe no matter how stale the window is or which zone
    // it was opened for.
    //
    // Readiness and stable identity are read only during the population pass.
    // Queue identities, never retained instances or deferred native calls.
    bool MayPopulate(const RValue& creator) {
        if (m_SpawnWindow.load(std::memory_order_relaxed) <= 0) return false;
        if (!PopulationCapacityAvailable()) { m_CapacityWaiting = true; return false; }
        if (!CreatorIsReady(creator)) return false;
        try {
            RValue id = g_Yytk->CallBuiltin("variable_instance_get", { creator, RValue("id") });
            if (id.m_Kind != VALUE_REAL && id.m_Kind != VALUE_INT32 && id.m_Kind != VALUE_INT64 && id.m_Kind != VALUE_REF) return false;
            const double n = id.ToDouble();
            if (!std::isfinite(n) || n < 0 || n > 9007199254740991.0 || std::floor(n) != n) return false;
            return m_Admission.Request(static_cast<int64_t>(n),[]{return AdaptivePopulationBudget::Instance().ReservePack();});
        } catch (...) { return false; }
    }

    // Whether one creator instance has finished initialising. The window's
    // opening gate looks for a ready creator; MayPopulate asks it
    // of the creator actually being answered.
    static bool CreatorIsReady(const RValue& creator) {
        try {
            RValue t = g_Yytk->CallBuiltin("variable_instance_get", { creator, RValue("enemyCreatorTimer") });
            return (t.m_Kind == VALUE_REAL || t.m_Kind == VALUE_INT32 || t.m_Kind == VALUE_INT64);
        } catch (...) { return false; }
    }

    // Diagnostics for `reveal stat`.
    long PacksZones() const { return m_ZonesPopulated; }
    int  SpawnWindowLeft() const { return m_SpawnWindow.load(std::memory_order_relaxed); }
    bool PacksPending() const { return m_PacksPending; }
    int  PendingTicks() const { return m_PendingTicks; }
    size_t QueuedPacks() const { return m_Admission.Pending(); }
    size_t UnconfirmedPacks() const { return m_Admission.Unconfirmed(); }
    bool NeedsBirthObservation() const { return m_Enabled && m_Packs && (QueuedPacks() || UnconfirmedPacks()); }
    bool TracksCreator(int64_t id) const { return m_Admission.Tracks(id); }
    uint64_t NativeBirthPacks() const { return m_Admission.NativeBirths(); }
    uint64_t PopulationGeneration() const { return m_PopulationGeneration; }
    void ObserveNativeBirth(int64_t id) {
        // Denial never blocks vanilla near-player spawning. A successful
        // enemy creation by this waiting creator is evidence of a birth,
        // even if its distance polling stopped. It is not a full-pack census.
        if (NeedsBirthObservation() && m_Admission.Tracks(id) && WindowIdentityValid())
            m_Admission.ObserveNativeBirth(id);
    }
    uint64_t AdmittedPacks() const { return m_Admission.Granted(); }
    unsigned PeakPacksPerFrame() const { return m_Admission.PeakPerFrame(); }
    uint64_t PopulationBudgetFrames() const { return m_Admission.BudgetLimitedFrames(); }
    uint64_t LastPackAdmissionMs() const { return m_Admission.LastAdmissionMs(); }

    // Called every frame from the frame callback.  The zone-identity work is
    // throttled to once per ~20 frames so each new map clears quickly without
    // spamming GameMaker calls during dense combat; the spawn window has to
    // count down every frame, so it is handled before that throttle.
    void OnFrame(uint64_t frameCount) {
        if (!m_Enabled) return;
        AdaptivePopulationBudget::Instance().ObserveBacklog(m_Admission.Pending(),DeferredDensityPending());
        AdaptivePopulationBudget::Instance().BeginFrame(frameCount,WantsPackSpawn() || m_PacksPending || DeferredDensityPending()>0);
        int w = m_SpawnWindow.load(std::memory_order_relaxed);
        if (w > 0) {
            // REPORTED 2026-09-12 (PR #2 issue 2): the identity work below is
            // throttled to one tick in 20, so a zone change could hand up to
            // 20 frames of an open window to the *next* zone's creators while
            // it was still loading - answering distance_to_object with 0
            // before a creator has initialised, which is precisely what
            // leaves spawners spent and inert (see TryOpenSpawnWindow).
            //
            // While a window is open, check the room every frame instead. It
            // is one member read, it only runs for the few seconds a window
            // lasts, and being wrong here is the expensive direction.
            // Defence in depth, not the safety property. MayPopulate is what
            // makes a stale window harmless; this just stops one lingering
            // longer than the zone it belongs to. It compares the FULL
            // identity (room + minimap instance + grid), because a replaced
            // or missing minimap with an unchanged room key is a zone change
            // too - reported 2026-09-12 as a second reproduction.
            if (!WindowIdentityValid()) { CloseSpawnWindow(); return; }
            // The old fixed timeout must not discard a dense zone's queue tail.
            if (m_Admission.HasDeferredWork() || m_CapacityWaiting || DeferredDensityPending()>0) w = (std::max)(w, 240);
            m_SpawnWindow.store(w - 1, std::memory_order_relaxed);
            // Freeze the elapsed diagnostic when scheduling ends. Otherwise
            // idle frames keep changing modstate and forcing disk rewrites.
            // Retain the identity and unresolved groups for later native births.
            if(w==1)AdaptivePopulationBudget::Instance().StopPass();
        }
        m_CapacityWaiting = false;
        m_Admission.OnFrame(frameCount);
        if ((frameCount % 20) != 0) return;
        try { Tick(); } catch (...) {}
        // Separate from Tick() on purpose: Tick() returns early once the zone
        // identity is unchanged, but a zone armed for packs still has to be
        // polled for readiness on later ticks.
        if (m_PacksPending) { try { TryOpenSpawnWindow(); } catch (...) {} }
    }

private:
    MapRevealManager() = default;
    bool m_Enabled{ false };
    bool m_Packs{ false };   // the spawn pass: opt-in (`reveal spawn 1`)
    bool m_Marks{ true };    // the markers: what `reveal packs` means now
    uint64_t m_ZoneGeneration{ 0 };
    int64_t m_LastInstance{ INT64_MIN };
    int64_t m_LastGrid{ INT64_MIN };
    int64_t m_LastRoom{ INT64_MIN };
    // Native polling/readiness may outlive the five-second throughput target.
    // Keep a grace window and preserve deferred work rather than silently
    // throwing away groups to make a deadline counter look successful.
    static constexpr int kSpawnWindowFrames = 900;   // ~15 s at 60 fps
    // Tick() runs every 20 frames, so this is ~10 minutes of asking before the
    // zone is written off as one whose creators never initialise.
    static constexpr int kPendingGiveUpTicks = 1800;
    std::atomic<int> m_SpawnWindow{ 0 };
    // The zone identity an open window belongs to. INT64_MIN means "no
    // window / unknown", and is never a value ReadIdentity produces.
    int64_t m_WindowRoom{ INT64_MIN };
    int64_t m_WindowInstance{ INT64_MIN };
    int64_t m_WindowGrid{ INT64_MIN };
    bool m_PacksPending{ false };
    int  m_PendingTicks{ 0 };
    int  m_ReadyProbeCursor{ 0 };
    long m_ZonesPopulated{ 0 };
    PackAdmissionQueue m_Admission;
    uint64_t m_PopulationGeneration=0;
    bool m_CapacityWaiting{ false };

    // Shutting the window is always safe - the pack pass re-arms on the next
    // identity change - so everything that means "the zone we opened this for
    // is gone or unverifiable" routes through here rather than each caller
    // remembering two fields.
    void CloseSpawnWindow() {
        ++m_PopulationGeneration;
        AdaptivePopulationBudget::Instance().StopPass();
        m_SpawnWindow.store(0, std::memory_order_relaxed);
        m_Admission.Reset();
        m_CapacityWaiting = false;
        m_PacksPending = false;
        m_PendingTicks = 0;
        m_ReadyProbeCursor = 0;
        m_WindowRoom = INT64_MIN;
        m_WindowInstance = INT64_MIN;
        m_WindowGrid = INT64_MIN;
    }

    // REPORTED 2026-09-12 (PR #2 issue 2): ResetIdentity is what Tick() calls
    // when the minimap object, its grid, or the ds_grid behind it is missing -
    // i.e. exactly while a room is loading. It used to forget the identity and
    // leave an open spawn window counting down into the new zone, so
    // WantsPackSpawn() stayed true through the transition and the readiness
    // gate was bypassed. Losing the map means losing the window too.
    void ResetIdentity() {
        m_LastInstance = INT64_MIN;
        m_LastGrid = INT64_MIN;
        m_LastRoom = INT64_MIN;
        ++m_ZoneGeneration;
        CloseSpawnWindow();
    }

    // The `room` global, or INT64_MIN when it cannot be read.
    // `room` is a GameMaker BUILT-IN, not a user global. GetInstanceMember on
    // the global instance never answers for it, so this returned INT64_MIN
    // every time - and ReadIdentity() refuses an unreadable identity, so
    // TryOpenSpawnWindow could never open a window. The pack pass had been
    // dead in every zone, silently, while the fog-clearing half kept working
    // because Tick() stores whatever RoomKey() returns without checking it.
    // Diagnosed live 2026-09-15: zonesPopulated=0 and creatorLies=0 across two
    // zones with 92 ready creators, then `roomprobe` named the failing read.
    //
    // Kept character-identical to ModuleMain.cpp's CurrentRoomKey(), which the
    // eSt tick uses and the behaviour harnesses inject by signature. If you
    // change one, change the other.
    int64_t RoomKey() const {
        RValue v;
        if (!AurieSuccess(g_Yytk->GetBuiltin("room", nullptr, NULL_INDEX, v))) return INT64_MIN;
        try {
            // A REF, not a real, on this runner - so derive from the kind the
            // runtime actually produced instead of assuming a conversion. The
            // hash is masked positive so a valid key can never equal the
            // INT64_MIN "unknown" sentinel this class compares against.
            if (v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64) {
                const int64_t n = v.ToInt64();
                return n == INT64_MIN ? INT64_MIN + 1 : n;
            }
            const std::string s = v.ToString();
            if (s.empty()) return INT64_MIN;
            uint64_t h = 1469598103934665603ull;             // FNV-1a
            for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
            return static_cast<int64_t>(h & 0x7FFFFFFFFFFFFFFFull);
        } catch (...) { return INT64_MIN; }
    }

    // The full zone identity: room, the live minimap instance, and its grid.
    // Returns false if any part is missing or unreadable - which is the state
    // during a room load, and is never a valid identity to act on.
    //
    // REPORTED 2026-09-12: the previous version compared only the room key and
    // used INT64_MIN as its "unreadable" sentinel, so a window opened while
    // the room was unreadable stored INT64_MIN and every later failed read
    // compared equal to it. "Unreadable closes the window" only held if the
    // window had been opened with a valid key. Now an unreadable identity is
    // never stored and never matches: ReadIdentity fails, and both callers
    // treat failure as invalid.
    bool ReadIdentity(int64_t& roomOut, int64_t& instOut, int64_t& gridOut) const {
        try {
            RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("objMinimap") });
            RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
            if (id.ToDouble() < 0) return false;
            if (!g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("minimapDiscoveredGrid") }).ToBoolean()) return false;
            RValue grid = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("minimapDiscoveredGrid") });
            const double gid = grid.ToDouble();
            if (gid < 0) return false;
            if (!g_Yytk->CallBuiltin("ds_exists", { RValue(gid), RValue(1.0) }).ToBoolean()) return false;
            const int64_t room = RoomKey();
            if (room == INT64_MIN) return false;
            roomOut = room;
            instOut = static_cast<int64_t>(std::llround(id.ToDouble()));
            gridOut = static_cast<int64_t>(std::llround(gid));
            return true;
        } catch (...) { return false; }
    }

    bool WindowIdentityValid() const {
        int64_t room = INT64_MIN, inst = INT64_MIN, grid = INT64_MIN;
        if (!ReadIdentity(room, inst, grid)) return false;
        return room == m_WindowRoom && inst == m_WindowInstance && grid == m_WindowGrid;
    }

    void Tick() {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("objMinimap") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { ResetIdentity(); return; }
        RValue ex0 = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("minimapDiscoveredGrid") });
        if (!ex0.ToBoolean()) { ResetIdentity(); return; }
        RValue grid = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("minimapDiscoveredGrid") });
        double gid = grid.ToDouble();
        if (gid < 0) { ResetIdentity(); return; }
        RValue ex = g_Yytk->CallBuiltin("ds_exists", { RValue(gid), RValue(1.0) });
        if (!ex.ToBoolean()) { ResetIdentity(); return; }

        const int64_t roomKey = RoomKey();

        const int64_t instanceKey = static_cast<int64_t>(std::llround(id.ToDouble()));
        const int64_t gridKey = static_cast<int64_t>(std::llround(gid));
        if (instanceKey == m_LastInstance && gridKey == m_LastGrid && roomKey == m_LastRoom) return;

        g_Yytk->CallBuiltin("ds_grid_clear", { RValue(gid), RValue(1.0) });
        m_LastInstance = instanceKey;
        m_LastGrid = gridKey;
        m_LastRoom = roomKey;
        ++m_ZoneGeneration;

        // New zone: arm the pack pass, but do NOT start lying yet - see
        // TryOpenSpawnWindow.  The zone-identity change fires while the new
        // room is still loading, and lying that early does real damage.
        //
        // Any window still open belongs to the zone we just left, so it goes
        // first (PR #2 issue 2) - the new zone gets one only once its own
        // creators pass the readiness gate.
        CloseSpawnWindow();
        if (m_Packs) {
            m_PacksPending = true; m_PendingTicks = 0;
            AdaptivePopulationBudget::Instance().BeginPass();
        }
    }

    // MEASURED 2026-09-11, the hard way: opening the window straight from the
    // zone-identity change left two zones with `enemyCreatorTimer` *undefined*
    // on every `Enemy_Creator_obj`, no packs spawned, and - the part that
    // makes it a real bug rather than a miss - **no packs spawning naturally
    // afterwards either**, when walking onto them. The spawners were not
    // merely un-triggered, they were spent: answering `distance_to_object`
    // with 0 before a creator has finished initialising makes it take its
    // spawn branch once, early, and come out inert. The mod would have left
    // zones emptier than vanilla.
    //
    // So readiness is *asked about*, not waited out with a guessed delay:
    // a live creator has to report a real `enemyCreatorTimer` (the healthy
    // zone read 116) before the lie is allowed to start. If the creators in
    // some zone never become ready, the window simply never opens and vanilla
    // behaviour is untouched, which is the right failure direction.
    void TryOpenSpawnWindow() {
        if (++m_PendingTicks > kPendingGiveUpTicks) { m_PacksPending = false; AdaptivePopulationBudget::Instance().StopPass(); return; }
        if (!PreparePopulationCapacity() || !PopulationCapacityAvailable()) return;

        RValue po = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        if (po.ToDouble() < 0) return;
        if (g_Yytk->CallBuiltin("instance_number", { po }).ToDouble() < 1.0) return;   // still loading

        RValue co = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Creator_obj") });
        if (co.ToDouble() < 0) { m_PacksPending = false; AdaptivePopulationBudget::Instance().StopPass(); return; }
        const int n = (int)g_Yytk->CallBuiltin("instance_number", { co }).ToDouble();
        // Minimap readiness can precede creator creation. Keep the bounded
        // pending poll alive; an empty first observation is not a completed zone.
        if (n < 1) { m_ReadyProbeCursor = 0; return; }

        bool ready = false;
        // A single unready first creator must not hold every ready sibling.
        // Rotate a bounded probe rather than scanning an entire dense room.
        for (int checked = 0; checked < (std::min)(n, 32); ++checked) {
            m_ReadyProbeCursor %= n;
            RValue inst = g_Yytk->CallBuiltin("instance_find", { co, RValue(static_cast<double>(m_ReadyProbeCursor++)) });
            if (inst.ToDouble() >= 0 && CreatorIsReady(inst)) { ready = true; break; }
        }
        if (!ready) return;   // creators still initialising - check again next tick

        // Never open a window against an identity we could not read. An
        // unknown identity cannot be invalidated later - it has nothing to
        // compare against - so the pass waits for the next tick instead
        // (reported 2026-09-12).
        int64_t room = INT64_MIN, minimap = INT64_MIN, grid = INT64_MIN;
        if (!ReadIdentity(room, minimap, grid)) return;

        m_WindowRoom = room;
        AdaptivePopulationBudget::Instance().PlanPacks(static_cast<uint64_t>(n)+DeferredDensityPending());
        m_Admission.Reset();
        m_WindowInstance = minimap;
        m_WindowGrid = grid;
        m_SpawnWindow.store(kSpawnWindowFrames, std::memory_order_relaxed);
        FP_POP_BEGIN();
        m_PacksPending = false;
        m_PendingTicks = 0;
        ++m_ZonesPopulated;
    }
};

} // namespace ForgePact
