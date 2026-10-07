#pragma once

#include "Common.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ForgePact {

// Pack markers: one minimap icon per `Enemy_Creator_*` spawner that has not
// given birth yet, so the revealed map shows every pack of the zone without
// creating a single monster.
//
// Why this replaced the "fill the map" pass as the default (2026-09-22,
// docs/population-performance-analysis.md): the game walks every LIVING
// monster several times per frame (minimap layer, health-bar instances, the
// 30-frame player-box rebuild), so keeping a whole zone's packs alive costs
// 30-80 ms per frame at 4x density however carefully they were born. A
// spawner's position and kind are known from zone generation, which is all a
// map marker needs; the pack itself is born by the game when the player
// walks near, exactly as in vanilla, and the game's own coloured dots take
// over from the marker at that moment.
//
// Cost model: the spawner list is enumerated once per zone (and again only
// when a creator family grows, or every ~20 s as a net-out fallback), a
// rotating slice of it is re-checked each frame, births are reported by the
// existing create hook, and drawing is one icon per pack cluster inside the
// game's own minimap draw, which arrives about once a second, not per frame
// (measured 2026-09-22).
//
// Look (measured in-game the same day): drawing the game's own icon sheet
// produced nothing visible, plain grey dots read as "very faint", and at 4x
// density the extra spawner copies stacked into blobs. So markers are now
// PNG icons per pack kind (embedded defaults written to bp_ipc\packmarks\,
// a player's own PNG of the same name wins), nearby spawners collapse into
// one icon with a count badge, and opaque outlined dots remain the fallback
// when no icon could be loaded.
//
// Retirement (issue #181): a marker goes when its pack is born or its spawner
// is gone. `kind` is the policy in both builds; the research build can switch
// to the two older ones at runtime (`packmarks retire timer|state|kind`):
// - `timer` (the default before issue #181): a create the hooks attribute to
//   the spawner retires it at once; the rotating check retires it when the
//   spawner is gone, when its `enemyCreatorTimer` was seen as a number and no
//   longer is, or when the timer was never seen as a number for
//   kUnarmedGiveUpFrames after listing ("spent before we looked"). The
//   ancient and miniboss spawners carry no `enemyCreatorTimer` (Live 1), so
//   that third rule gave their markers up a few seconds after arrival
//   although their packs were unborn: the issue's symptom.
// - `state`: the rotating check retires it when the spawner is gone, or when
//   the spawner's own `enemyArray`, read by name, is an array - undefined
//   before and while armed, an array once the pack is born
//   (docs/RUNTIME_DATA_MODELS.md 11.2). Nothing else does: not a timer, and
//   not an attributed create, which runs inside the birth before the
//   spawner's state says born. The question is asked of the spawner itself,
//   where the marker is used, not of anything cached.
// - `kind` (the default, shipped for issue #181): the rotating check retires
//   it when the spawner is gone, or by the kind's own entry in kKindRules:
//   normal and ambush on `enemyArray` as under `state`; the five other kinds
//   on their protected pack state, the store record the spawner's `spawnPack`
//   variable names (a key, not the state), read through the game's getter by
//   name and refused for any key that is not a whole number in 0..262143
//   (RUNTIME_DATA_MODELS 13.7: a bad key faulted the game). A `birth`-mode
//   kind retires when its signal reads born, and one listed already born gets
//   no marker at all. A `packgone`-mode kind (miniboss, legion, champion:
//   built at zone arrival, owner's decision 2026-10-06) keeps its marker
//   after the birth, until the members rule retires it (every recorded
//   member of its pack gone) or its spawner goes. The members rule is on in
//   both builds (owner, 2026-10-06, "Re-enable the check as is"), and it is
//   not yet proven: its alive read never read a member alive in Live 4 (a
//   miniboss pack read gone 5 s after the warp while two of its monsters
//   were on screen, and the normal control read 0/8 alive with monsters on
//   screen), so a marker may go while its pack still lives. The owner
//   accepted that risk; Live procedure 2's slow miniboss fight judges it.
//   The read is counted in both builds (KindStats::goneRead, `packmarks
//   stat`'s ends= and gonerule=), so a session can see whether it turned
//   gone before or after the kill; the research build alone can switch the
//   rule (`packmarks gonerule on|off`), and with it off a `packgone`-mode
//   marker stays until its spawner goes and is never written into the birth
//   memory. A read that cannot be made keeps the marker and is counted (`Unread()`),
//   so "held because unread" shows beside "unborn". Why each kind's signal
//   (Live 4, 2026-10-06, docs/map-reveal-research.md "Live 4 results"): a
//   normal birth moved `enemyArray` to an array and the state 1 -> 3; an
//   ancient birth moved the state 1 -> 3 as the owner saw the pack appear; a
//   colossal chest opening moved its spawners 0 -> 2; the miniboss spawners
//   already read 2 at arrival. Champion and legion were not observed live
//   and ride the miniboss's rule on the static reading.
// The birth memory: a spawner retired by a birth rule is remembered for the
// whole game session by its id and by room, kind and position, recorded under
// every policy and applied only under `kind`, so a revisited zone does not
// mark a pack again that was already created. Both keys stay: Live 4's
// revisit gave every spawner a new id, so only the position key matched.
//
// Everything below is header-only and reaches the game only through
// g_Yytk->CallBuiltin, so tests/pack_markers_harness.cpp can compile the real
// class against a controlled runner.
class PackMarkers {
public:
    enum Kind : uint8_t { Normal = 0, Ambush, Ancient, Champion, ColossalChest, Legion, Miniboss, KindCount };
    // Index order == Kind. The names are resolved by asset_get_index, never
    // by a literal object index.
    static constexpr const char* kCreatorObjects[KindCount] = {
        "Enemy_Creator_obj", "Enemy_Creator_Ambush_obj", "Enemy_Creator_Ancient_obj",
        "Enemy_Creator_Champion_obj", "Enemy_Creator_Colossal_Chest_obj",
        "Enemy_Creator_Legion_obj", "Enemy_Creator_Miniboss_obj",
    };
    // Index order == Kind; the same names as PackMarkerIcons::kIcons, used in
    // the `packmarks` diagnostic lines.
    static constexpr const char* kKindNames[KindCount] = {
        "normal", "ambush", "ancient", "champion", "colossal_chest", "legion", "miniboss",
    };
    // Which kind a mixed cluster shows: the rarest pack wins.
    static constexpr int kKindPriority[KindCount] = { 0, 1, 5, 4, 3, 2, 6 };
    // The retirement policy (see the class comment).
    enum class Retire : uint8_t { Timer = 0, State, Kind };
    static constexpr const char* kRetireNames[3] = { "timer", "state", "kind" };
    // The `kind` policy's table, one entry per kind (index order == Kind), so
    // one kind's rule can change alone. The signal: the spawner's enemyArray
    // turning into an array (RUNTIME_DATA_MODELS 11.2, measured on normal and
    // ambush), or its protected pack state at or above the threshold (2 for
    // all five; measured in Live 4 on ancient, colossal chest and miniboss,
    // champion and legion by static reading only). The mode:
    // `birth` retires the marker when the signal reads born; `packgone` reads
    // and counts it (`held`) but keeps the marker until the pack is gone (the
    // members rule, on in both builds) or its spawner is.
    enum class Signal : uint8_t { EnemyArray = 0, PackState };
    enum class Mode : uint8_t { Birth = 0, PackGone };
    struct KindRule { Signal signal; double threshold; Mode mode; };
    static constexpr KindRule kKindRules[KindCount] = {
        { Signal::EnemyArray, 0.0, Mode::Birth },      // normal
        { Signal::EnemyArray, 0.0, Mode::Birth },      // ambush
        { Signal::PackState,  2.0, Mode::Birth },      // ancient
        { Signal::PackState,  2.0, Mode::PackGone },   // champion
        { Signal::PackState,  2.0, Mode::Birth },      // colossal_chest
        { Signal::PackState,  2.0, Mode::PackGone },   // legion
        { Signal::PackState,  2.0, Mode::PackGone },   // miniboss
    };
    // The protected value store's record count: a key outside 0..262143 never
    // reaches the getter.
    static constexpr double kProtectedRecords = 262144.0;
    // "Room unknown": never recorded, never matched by position (AGENTS.md: a
    // sentinel for "unknown" must not compare equal to a real value).
    static constexpr int64_t kUnknownRoom = INT64_MIN;
    // Session-long memories are bounded the way m_Spent is: cleared whole
    // once they pass this many entries.
    static constexpr size_t kMemoryCap = 65536;
    static constexpr size_t kMembersPerCreator = 64;
    // Why a marker was retired, one counter each per kind and zone.
    enum Reason : uint8_t { ReasonSpawned = 0, ReasonDestroyed, ReasonTimerGone, ReasonGivenUp, ReasonStateBorn, ReasonKindBorn, ReasonPackGone, ReasonCount };
    static constexpr const char* kReasonNames[ReasonCount] = { "spawned", "destroyed", "timergone", "givenup", "stateborn", "kindborn", "packgone" };
    // Per kind, since the zone generation last changed.
    struct KindStats {
        uint64_t listed = 0;                    // distinct spawners that got a marker (or were born at listing, under `kind`)
        uint64_t retired[ReasonCount] = {};     // retirements by rule
        uint64_t attributed = 0;                // creates the hooks attributed to a spawner of this kind, retiring or not
        uint64_t remembered = 0;                // listed spawners the birth memory withheld (under `kind`)
        uint64_t sameid = 0;                    // listed spawners whose id was listed in an earlier visit to the same room
        uint64_t goneRead = 0;                  // `packgone` mode: times a held marker's members read turned gone, retiring or not
        int64_t ageMin = -1, ageMax = -1;       // frames between listing and retirement (-1 = none retired)
        std::unordered_map<int, uint64_t> creates;        // created object index -> attributed creates
        std::unordered_map<int, uint64_t> otherCreates;   // non-enemy object index -> creates by a spawner of this kind
    };
    // `packgone`-mode markers held now whose signal reads born, and those of
    // them with no member recorded.
    struct Held { unsigned held = 0, unlinked = 0; };
    // How a census found one variable on a creator: a number (the timer) or an
    // array (enemyArray), undefined, absent (variable_instance_exists false),
    // or anything else, a read that threw included.
    enum Tally : uint8_t { TallyValue = 0, TallyUndefined, TallyAbsent, TallyOther, TallyCount };
    static constexpr const char* kArrayTallyNames[TallyCount] = { "array", "undefined", "absent", "other" };
    // How a protected value read through its key came out: a number, an
    // unset record, no such variable, a key the guard refused or a read that
    // threw, or another value.
    enum PackBucket : uint8_t { PackNumber = 0, PackUndefined, PackAbsent, PackUnreadable, PackOther, PackBucketCount };
    static constexpr const char* kPackBucketNames[PackBucketCount] = { "number", "undefined", "absent", "unreadable", "other" };
    struct PackRead {
        PackBucket bucket = PackAbsent;
        double value = 0;      // the record's value when bucket == PackNumber
        RValue key;            // what the variable held (undefined when absent)
    };
    struct CensusRow {
        unsigned creators = 0, marked = 0;
        unsigned timer[TallyCount] = {};        // enemyCreatorTimer
        unsigned enemyArray[TallyCount] = {};
        unsigned lost = 0;    // not marked, and enemyArray not an array: a marker the state policy would hold
        unsigned stale = 0;   // marked, and enemyArray an array: a marker the state policy would retire
        // Census(true) only:
        unsigned born = 0;               // spawners whose kind's signal reads born now
        unsigned attributedUnborn = 0;   // spawners with a create attributed this zone whose signal reads unborn
        std::map<int64_t, unsigned> spawnPackValues;   // whole-number pack state -> spawners
        unsigned spawnPack[PackBucketCount] = {};      // the other buckets (PackNumber: whole numbers counted above, others in PackOther)
    };
    // One spawner of `packmarks census <kind>`.
    struct CensusEntry {
        int64_t id = -1;
        double x = 0, y = 0;
        bool marked = false, born = false;
        PackRead pack;
        Tally enemyArray = TallyOther;
        uint64_t attributed = 0;
        unsigned membersAlive = 0, membersRecorded = 0;
    };
    // A creator's recorded pack members, read now (`packmarks creator <id>`).
    struct MemberObject { int object; unsigned alive, recorded; };
    struct MemberCount { unsigned alive = 0, recorded = 0; std::vector<MemberObject> objects; };
    // The creates attributed to one spawner since the zone generation changed.
    struct CreatorCreates { uint64_t attributed = 0; std::map<int, uint64_t> objects; };
    struct Marker {
        int64_t id;
        double x, y;
        uint8_t kind;
        bool armed;            // enemyCreatorTimer was seen as a number: initialised, not yet spawned
        uint64_t firstSeen;    // frame the marker was enumerated
        int64_t room = kUnknownRoom;   // the room key it was listed in
        bool bornSeen = false;         // `kind`, `packgone` mode: the signal read born at its last read
        bool goneSeen = false;         // `kind`, `packgone` mode: the members read said gone at its last read
    };
    struct Cluster { double x, y; uint8_t kind; unsigned count; };
    // Marker look, tunable live with `packmarks ...`.
    struct Style {
        // Icons: one PNG per kind through the provider below (default on).
        bool icons = true;
        double iconScale = 1.0;       // times global.hud_res
        // Nearby spawners (density copies sit 28 px apart) collapse into one
        // marker per cell of this many world pixels; 0 = one marker each.
        double clusterPx = 96.0;
        bool badge = true;            // the count on a collapsed cluster
        // Fallback primitives when an icon is missing: opaque dots on a dark
        // outline disc, coloured by kind (BGR GameMaker colour ints).
        uint32_t colour[KindCount] = { 0xFFFFFF, 0xFFFFFF, 0xFF60FF, 0xFFDC50, 0x3CD2FF, 0x2896FF, 0x4646FF };
        double radius[KindCount] = { 3.0, 3.0, 4.0, 4.0, 4.0, 4.0, 5.0 };   // map pixels, times global.hud_res
        bool filled[KindCount] = { true, true, true, true, true, true, true };
        bool outline = true;
        double outlineExtra = 1.5;
        uint32_t outlineColour = 0x101010;
        double alpha = 1.0;
        // The game's icon sheet path (`packmarks ring 0` with `icons 0`): kept
        // for experiments only; it drew nothing visible on 2026-09-22.
        double subimage[KindCount] = { 8, 8, 16, 12, 12, 8, 22 };
        double scale = 1.0;
        bool ring = true;
        double ringRadius = 3.0;
    };
    // Under the `timer` policy a marker is dropped when a creator has shown no
    // timer for this long after being enumerated: spawners initialise within a
    // second or two, so one that never reports a timer was taken to be spent
    // already when we first saw it (issue #181 questions this for the kinds
    // that may carry no timer at all).
    static constexpr uint64_t kUnarmedGiveUpFrames = 600;
    static constexpr uint64_t kCountPollFrames = 30;
    static constexpr uint64_t kEnumerateMinGapFrames = 60;
    static constexpr uint64_t kReenumerateFrames = 1200;   // ~20 s: the net-out fallback, seven counts plus one walk
    static constexpr size_t kValidatePerFrame = 32;

    // Set by ModuleMain: makes the PNG for `kind` available to the game and
    // returns two paths to try in order - relative to GameMaker's working
    // directory (its file sandbox's own form) and absolute (accepted when the
    // game was built without the sandbox). Absent in the test harness, where
    // markers stay primitives.
    using IconProvider = bool (*)(int kind, std::string& gmlPath, std::string& absolutePath);

    static PackMarkers& Instance() { static PackMarkers s; return s; }

    bool Enabled() const { return m_Enabled.load(std::memory_order_relaxed); }
    void SetEnabled(bool on) {
        m_Enabled.store(on, std::memory_order_relaxed);
        if (!on) Clear();
        else m_Dirty = true;
    }
    void SetIconProvider(IconProvider provider) { m_IconProvider = provider; }
    // Forget the loaded icons; the next draw loads them again (a player who
    // replaced a PNG sees it without restarting).
    void ReloadIcons() {
        for (int k = 0; k < KindCount; ++k) {
            if (m_Sprite[k] >= 0) { try { g_Yytk->CallBuiltin("sprite_delete", { RValue((double)m_Sprite[k]) }); } catch (...) {} }
            m_Sprite[k] = -1;
        }
        m_IconsLoaded = 0; m_IconsViaAbsolute = 0; m_IconsTried = false;
    }

    // Per-frame maintenance from the frame callback. `zoneGeneration` comes
    // from MapRevealManager (it changes on every zone identity change);
    // `mapReadable()` says the zone's minimap exists, i.e. generation is
    // done - it costs a few runtime calls, so it is asked only when a list
    // would be built. `roomKey` is ModuleMain's CurrentRoomKey() (the key
    // MapRevealManager's identity uses), kUnknownRoom when unreadable: the
    // birth memory's room.
    template <class Probe>
    void OnFrame(uint64_t frame, uint64_t zoneGeneration, Probe mapReadable, int64_t roomKey = kUnknownRoom) {
        if (!Enabled()) return;
        m_Frame = frame;
        m_Room = roomKey;
        if (zoneGeneration != m_Zone) { m_Zone = zoneGeneration; Clear(); m_Spent.clear(); ResetStats(); m_Dirty = true; }
        const bool mayEnumerate = frame >= m_LastEnumerate + kEnumerateMinGapFrames;
        if (m_Dirty && mayEnumerate) { if (mapReadable()) Enumerate(frame); return; }
        if ((frame % kCountPollFrames) == 0 && mayEnumerate && !m_Dirty) {
            // New spawners can appear after generation (a dying monster can
            // leave a legion creator behind). A family growing past its peak
            // re-enumerates at once; a periodic pass catches the rare case of
            // a new spawner netting out against a destroyed one in the same
            // poll. Shrinkage is caught by the rotating check below.
            const bool grew = PollFamilies();
            if (grew || frame >= m_LastEnumerate + kReenumerateFrames) { if (mapReadable()) Enumerate(frame); return; }
        }
        ValidateSlice(frame);
    }

    // From the create hook: `creatorId` (a numeric instance id) just created
    // a monster of object `createdObject`; `creatorObject` is the creator's
    // own object index, which names its kind when it has no marker (-1 when
    // not known). Every such create is counted against the creator's kind.
    // Under `timer` the pack is taken to exist, so the marker goes now rather
    // than when the rotating check reaches it; under `state` and `kind` the
    // create retires nothing (the spawner's own state decides).
    void MarkSpawned(int64_t creatorId, int createdObject = -1, int creatorObject = -1) {
        if (!Enabled()) return;
        const int kind = KindOfCreator(creatorId, creatorObject);
        if (kind >= 0) {
            ++m_Stats[kind].attributed;
            if (createdObject >= 0) ++m_Stats[kind].creates[createdObject];
        } else ++m_UnattributedCreates;
        if (m_CreatorZone.size() >= kMemoryCap && !m_CreatorZone.count(creatorId)) m_CreatorZone.clear();
        CreatorCreates& mine = m_CreatorZone[creatorId];
        ++mine.attributed;
        if (createdObject >= 0) ++mine.objects[createdObject];
        if (GetRetire() != Retire::Timer) return;
        // Remembered even when no marker exists yet: a pack born during the
        // zone's first frames must not get a marker from a later enumeration.
        m_Spent.insert({ creatorId, true });
        auto it = m_Index.find(creatorId);
        if (it == m_Index.end()) return;
        const Marker m = m_Markers[it->second];
        Remove(it->second);
        NoteRetired(m, ReasonSpawned, m_Frame);
        Remember(m);
        ++m_Spawned;
    }
    // From the create hook, after the create returned: `memberId` is the
    // instance a spawner's attributed create made (its pack's member) and
    // `memberObject` that instance's object. Kept per spawner for the whole
    // game session, not per zone (a pack built at zone arrival may be built
    // before the zone's counters reset), at most kMembersPerCreator each.
    // A `packgone`-mode marker retires once at least one member was recorded
    // and none exists any more, when the members rule is on (PackGoneRetires).
    void NoteMember(int64_t creatorId, int64_t memberId, int memberObject = -1) {
        if (!Enabled() || memberId < 0) return;
        if (m_Members.size() >= kMemoryCap && !m_Members.count(creatorId)) m_Members.clear();
        std::vector<std::pair<int64_t, int>>& list = m_Members[creatorId];
        if (list.size() >= kMembersPerCreator) return;
        for (const auto& member : list) if (member.first == memberId) return;
        list.push_back({ memberId, memberObject });
    }
    // From the create hook (research build): a spawner created an object that
    // is not a monster. Counted per kind and zone; retires nothing.
    void NoteOtherCreate(int64_t creatorId, int createdObject, int creatorObject = -1) {
        if (!Enabled()) return;
        const int kind = KindOfCreator(creatorId, creatorObject);
        if (kind < 0) { ++m_UnattributedOther; return; }
        if (createdObject >= 0) ++m_Stats[kind].otherCreates[createdObject];
    }

    // The retirement policy, switchable at any time; a switch keeps every
    // marker and what was learned about it.
    Retire GetRetire() const { return m_Retire.load(std::memory_order_relaxed); }
    void SetRetire(Retire policy) { m_Retire.store(policy, std::memory_order_relaxed); }
    static const char* RetireName(Retire policy) { return kRetireNames[policy == Retire::Kind ? 2 : policy == Retire::State ? 1 : 0]; }
    // Whether the members rule retires a `packgone`-mode marker (see the
    // class comment): on in both builds; only the research build's
    // `packmarks gonerule` switches it. Off, the read is still made and counted.
    bool PackGoneRetires() const { return m_PackGoneRetires.load(std::memory_order_relaxed); }
    void SetPackGoneRetires(bool on) { m_PackGoneRetires.store(on, std::memory_order_relaxed); }

    // One-shot census for `packmarks census`, never run per frame: walks
    // every creator of each kind now and tallies how it carries its timer and
    // its enemyArray, and how that agrees with the markers held. With
    // `packState` (the second instrument) it also reads each creator's
    // protected pack state through its `spawnPack` key and fills `born`,
    // `attributedUnborn` and the spawnPack tally.
    std::array<CensusRow, KindCount> Census(bool packState = false) {
        std::array<CensusRow, KindCount> rows{};
        for (int k = 0; k < KindCount; ++k) {
            const int obj = ObjectIndex(k);
            if (obj < 0) continue;
            int n = 0;
            try { n = static_cast<int>(g_Yytk->CallBuiltin("instance_number", { RValue((double)obj) }).ToDouble()); } catch (...) { continue; }
            CensusRow& row = rows[k];
            for (int i = 0; i < n; ++i) {
                RValue inst;
                try { inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)obj), RValue((double)i) }); } catch (...) { continue; }
                ++row.creators;
                int64_t id = -1;
                const bool isMarked = Identity(inst, id) && m_Index.count(id) != 0;
                if (isMarked) ++row.marked;
                ++row.timer[TallyVariable(inst, "enemyCreatorTimer", false)];
                const Tally packs = TallyVariable(inst, "enemyArray", true);
                ++row.enemyArray[packs];
                const bool born = packs == TallyValue;
                if (!isMarked && !born) ++row.lost;
                if (isMarked && born) ++row.stale;
                if (!packState) continue;
                const PackRead pack = ReadProtected(inst, "spawnPack");
                if (pack.bucket == PackNumber && IsWhole(pack.value)) ++row.spawnPackValues[static_cast<int64_t>(pack.value)];
                else ++row.spawnPack[pack.bucket == PackNumber ? PackOther : pack.bucket];
                const bool kindBorn = BornBy(k, packs, pack);
                if (kindBorn) ++row.born;
                auto creates = m_CreatorZone.find(id);
                if (!kindBorn && creates != m_CreatorZone.end() && creates->second.attributed > 0) ++row.attributedUnborn;
            }
        }
        return rows;
    }
    // The census's spawnPack field: `<value>:<count>` for whole-number states
    // ascending, then undefined, absent, unreadable and other, comma-separated,
    // only the buckets that occur; `none` for a kind with no spawners.
    static std::string SpawnPackTally(const CensusRow& row) {
        std::string out;
        auto add = [&out](const std::string& bucket, unsigned n) { if (n) out += (out.empty() ? "" : ",") + bucket + ":" + std::to_string(n); };
        for (const auto& [value, n] : row.spawnPackValues) add(std::to_string(value), n);
        for (int b = PackUndefined; b < PackBucketCount; ++b) add(kPackBucketNames[b], row.spawnPack[b]);
        return out.empty() ? std::string("none") : out;
    }
    // `<value>` in `packmarks census <kind>` and `packmarks creator`: a whole
    // number, or the bucket's name.
    static std::string PackText(const PackRead& r) {
        if (r.bucket == PackNumber) return IsWhole(r.value) ? std::to_string(static_cast<int64_t>(r.value)) : std::string("other");
        return kPackBucketNames[r.bucket];
    }
    // `packmarks census <kind>`: one entry per spawner of that kind, read now.
    std::vector<CensusEntry> CensusList(int kind) {
        std::vector<CensusEntry> out;
        if (kind < 0 || kind >= KindCount) return out;
        const int obj = ObjectIndex(kind);
        if (obj < 0) return out;
        int n = 0;
        try { n = static_cast<int>(g_Yytk->CallBuiltin("instance_number", { RValue((double)obj) }).ToDouble()); } catch (...) { return out; }
        for (int i = 0; i < n; ++i) {
            RValue inst;
            try { inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)obj), RValue((double)i) }); } catch (...) { continue; }
            CensusEntry e;
            if (!Identity(inst, e.id)) continue;
            try {
                e.x = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
                e.y = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            } catch (...) {}
            e.marked = m_Index.count(e.id) != 0;
            e.enemyArray = TallyVariable(inst, "enemyArray", true);
            e.pack = ReadProtected(inst, "spawnPack");
            e.born = BornBy(kind, e.enemyArray, e.pack);
            auto creates = m_CreatorZone.find(e.id);
            if (creates != m_CreatorZone.end()) e.attributed = creates->second.attributed;
            const MemberCount members = Members(e.id);
            e.membersAlive = members.alive; e.membersRecorded = members.recorded;
            out.push_back(std::move(e));
        }
        return out;
    }
    // A creator's recorded members, each asked of the game now
    // (instance_exists, by name); per object, most recorded first.
    MemberCount Members(int64_t creatorId) const {
        MemberCount out;
        auto it = m_Members.find(creatorId);
        if (it == m_Members.end()) return out;
        std::map<int, MemberObject> byObject;
        for (const auto& [member, object] : it->second) {
            const bool alive = MemberExists(member);
            ++out.recorded; if (alive) ++out.alive;
            MemberObject& o = byObject.emplace(object, MemberObject{ object, 0, 0 }).first->second;
            ++o.recorded; if (alive) ++o.alive;
        }
        for (const auto& [object, o] : byObject) out.objects.push_back(o);
        std::stable_sort(out.objects.begin(), out.objects.end(), [](const MemberObject& a, const MemberObject& b) { return a.recorded > b.recorded; });
        return out;
    }
    // A creator's recorded members as recorded, (instance id, object), in
    // record order (`packmarks creator <id> ids:`, the alive read's control).
    std::vector<std::pair<int64_t, int>> MemberList(int64_t creatorId) const {
        auto it = m_Members.find(creatorId);
        return it == m_Members.end() ? std::vector<std::pair<int64_t, int>>{} : it->second;
    }
    // The members rule's alive read of one id, the exact call PackGone makes.
    static bool MemberReadAlive(int64_t member) { return MemberExists(member); }
    // The creates attributed to one spawner this zone (`packmarks census
    // <kind>`'s attributed=, `packmarks creator`'s attributed= and its objects).
    CreatorCreates CreatesOf(int64_t creatorId) const {
        auto it = m_CreatorZone.find(creatorId);
        return it == m_CreatorZone.end() ? CreatorCreates{} : it->second;
    }
    // One protected value of a creator, read by name: the variable holds a
    // key into the game's protected store; only a whole number in 0..262143
    // is handed to the store's getter (PC_GetVariableGMLWrapper, by name).
    static PackRead ReadProtected(const RValue& inst, const char* name) {
        PackRead r;
        try {
            if (!g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue(name) }).ToBoolean()) { r.bucket = PackAbsent; return r; }
            r.key = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(name) });
        } catch (...) { r.bucket = PackUnreadable; return r; }
        RValue value;
        if (!ReadStore(r.key, value)) { r.bucket = PackUnreadable; return r; }
        if (IsUndefined(value)) r.bucket = PackUndefined;
        else if (IsNumber(value) && std::isfinite(value.ToDouble())) { r.bucket = PackNumber; r.value = value.ToDouble(); }
        else r.bucket = PackOther;
        return r;
    }
    // The key guard: a number kind holding a whole number in 0..262143.
    static bool KeyInRange(const RValue& key) {
        if (!IsNumber(key)) return false;
        try {
            const double d = key.ToDouble();
            return std::isfinite(d) && d >= 0.0 && d < kProtectedRecords && d == std::floor(d);
        } catch (...) { return false; }
    }
    // The creator's kind by its marker, its listing this zone or its object
    // index; -1 when none of them names one (`packmarks creator <id>`).
    int KindOf(int64_t creatorId, int creatorObject = -1) const { return KindOfCreator(creatorId, creatorObject); }

    // A density copy made on its own, as the player walks (rolling density
    // copies): count it into its family's peak, so the growth poll does not
    // re-list the whole zone for every copy. The periodic re-listing gives it
    // its marker (it sits beside its original, in the same cluster).
    void NoteCopy(int objectIndex) {
        if (!Enabled()) return;
        for (int k = 0; k < KindCount; ++k)
            if (m_ObjResolved[k] && m_ObjIdx[k] == objectIndex) { ++m_FamilyPeak[k]; ++m_CopiesNoted; return; }
    }
    uint64_t CopiesNoted() const { return m_CopiesNoted; }

    // From the minimap hook, after the game's own enemy layer drew: the
    // arguments are the ones DrawMinimapDynamic received for the monster
    // family, so the marker lands exactly where the game would put a dot.
    // Measured placement (2026-09-22): x' = 32 + x * scaleX,
    // y' = 32 + (y + yOffset) * scaleY.
    void Draw(const RValue& scaleX, const RValue& scaleY, const RValue& yOffset, const RValue& sprite) {
        if (!Enabled() || m_Markers.empty()) return;
        try {
            if (!IsNumber(scaleX) || !IsNumber(scaleY)) return;
            const double sx = scaleX.ToDouble(), sy = scaleY.ToDouble();
            const double yoff = IsNumber(yOffset) ? yOffset.ToDouble() : 0.0;
            if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(yoff)) return;
            if (m_Style.icons && !m_IconsTried && m_IconProvider) LoadIcons();
            if (m_ClustersDirty) BuildClusters();
            double hud = 1.0;
            try {
                RValue h = g_Yytk->CallBuiltin("variable_global_get", { RValue("hud_res") });
                if (IsNumber(h) && std::isfinite(h.ToDouble()) && h.ToDouble() > 0) hud = h.ToDouble();
            } catch (...) {}
            const bool useIcons = m_Style.icons && m_IconsLoaded > 0;
            const bool spriteOk = IsNumber(sprite) || sprite.m_Kind == VALUE_REF;
            const bool primitives = useIcons || m_Style.ring || !spriteOk;
            // Primitives and badges change the runner's global draw state, and the
            // game's own layer code reads it (draw_get_alpha), so it is read up
            // front on every path and the guard puts colour, alpha and font back
            // when Draw leaves - whichever pass changed them, and even if a draw
            // call throws. (PR #67 review: the sprite path with badges used to
            // leave the badge colour and a guessed alpha behind.)
            struct DrawStateGuard {
                double alpha = 1.0, colour = 16777215.0;
                bool fontApplied = false;   // set only once draw_set_font really ran
                RValue font;                // draw_get_font's answer, put back as is (a real or a reference)
                ~DrawStateGuard() {
                    try { if (fontApplied) g_Yytk->CallBuiltin("draw_set_font", { font }); } catch (...) {}
                    try { g_Yytk->CallBuiltin("draw_set_colour", { RValue(colour) }); } catch (...) {}
                    try { g_Yytk->CallBuiltin("draw_set_alpha", { RValue(alpha) }); } catch (...) {}
                }
            } saved;
            try { RValue a = g_Yytk->CallBuiltin("draw_get_alpha", {}); if (IsNumber(a)) saved.alpha = a.ToDouble(); } catch (...) {}
            try { RValue c = g_Yytk->CallBuiltin("draw_get_colour", {}); if (IsNumber(c)) saved.colour = c.ToDouble(); } catch (...) {}
            if (primitives) g_Yytk->CallBuiltin("draw_set_alpha", { RValue(m_Style.alpha) });
            uint32_t lastColour = 0xFFFFFFFFu;
            auto setColour = [&](uint32_t c) {
                if (c != lastColour) { g_Yytk->CallBuiltin("draw_set_colour", { RValue((double)c) }); lastColour = c; }
            };
            // Pass 1: the dark outline discs under every dot drawn as a primitive.
            if (primitives && m_Style.outline) {
                for (const Cluster& c : m_Clusters) {
                    const uint8_t k = c.kind < KindCount ? c.kind : Normal;
                    if (useIcons && m_Sprite[k] >= 0) continue;
                    setColour(m_Style.outlineColour);
                    g_Yytk->CallBuiltin("draw_circle", { RValue(32.0 + c.x * sx), RValue(32.0 + (c.y + yoff) * sy),
                        RValue((m_Style.radius[k] + m_Style.outlineExtra) * hud), RValue(0.0) });
                }
            }
            // Pass 2: the markers themselves.
            for (const Cluster& c : m_Clusters) {
                const double px = 32.0 + c.x * sx;
                const double py = 32.0 + (c.y + yoff) * sy;
                const uint8_t k = c.kind < KindCount ? c.kind : Normal;
                if (useIcons && m_Sprite[k] >= 0) {
                    const double s = m_Style.iconScale * hud;
                    g_Yytk->CallBuiltin("draw_sprite_ext", { RValue((double)m_Sprite[k]), RValue(0.0), RValue(px), RValue(py),
                        RValue(s), RValue(s), RValue(0.0), RValue(16777215.0), RValue(m_Style.alpha) });
                    ++m_IconDraws;
                } else if (primitives) {
                    setColour(m_Style.colour[k]);
                    g_Yytk->CallBuiltin("draw_circle", { RValue(px), RValue(py), RValue(m_Style.radius[k] * hud), RValue(m_Style.filled[k] ? 0.0 : 1.0) });
                } else {
                    const double s = m_Style.scale * hud;
                    g_Yytk->CallBuiltin("draw_sprite_ext", { sprite, RValue(m_Style.subimage[k]), RValue(px), RValue(py),
                        RValue(s), RValue(s), RValue(0.0), RValue((double)m_Style.colour[k]), RValue(m_Style.alpha) });
                }
                ++m_Draws;
            }
            // Pass 3: the count badge on collapsed clusters, top-right of the marker.
            if (m_Style.badge) {
                bool fontSet = false;
                for (const Cluster& c : m_Clusters) {
                    if (c.count < 2) continue;
                    if (!fontSet) {
                        try {
                            saved.font = g_Yytk->CallBuiltin("draw_get_font", {});
                            RValue smallFont = g_Yytk->CallBuiltin("variable_global_get", { RValue("font_smallest") });
                            // Fonts, like sprites, can come back as asset references on this runner.
                            if ((IsNumber(smallFont) || smallFont.m_Kind == VALUE_REF) && smallFont.ToDouble() >= 0) {
                                g_Yytk->CallBuiltin("draw_set_font", { smallFont });
                                saved.fontApplied = true;
                            }
                        } catch (...) {}
                        if (!primitives) g_Yytk->CallBuiltin("draw_set_alpha", { RValue(m_Style.alpha) });
                        fontSet = true;
                    }
                    const uint8_t k = c.kind < KindCount ? c.kind : Normal;
                    const double r = (useIcons ? 8.0 * m_Style.iconScale : m_Style.radius[k]) * hud;
                    const double bx = 32.0 + c.x * sx + r * 0.6, by = 32.0 + (c.y + yoff) * sy - r * 1.4;
                    const std::string text = std::to_string(c.count);
                    setColour(0x101010);
                    g_Yytk->CallBuiltin("draw_text", { RValue(bx + 1.0), RValue(by + 1.0), RValue(text.c_str()) });
                    setColour(0xFFFFFF);
                    g_Yytk->CallBuiltin("draw_text", { RValue(bx), RValue(by), RValue(text.c_str()) });
                    ++m_BadgeDraws;
                }
            }
            // `saved` restores colour, alpha and font as it goes out of scope here.
        } catch (...) { ++m_DrawErrors; }
    }

    // Diagnostics (modstate / `packmarks stat`).
    size_t Count() const { return m_Markers.size(); }
    size_t Clusters() const { return m_Clusters.size(); }
    int IconsLoaded() const { return m_IconsLoaded; }
    bool IconsTried() const { return m_IconsTried; }
    int IconsViaAbsolute() const { return m_IconsViaAbsolute; }
    int IconLastKind() const { return m_IconLastKind; }        // RValue kind sprite_add answered with last (-1 = never)
    double IconLastValue() const { return m_IconLastValue; }   // and its numeric value
    uint64_t IconLoadThrows() const { return m_IconLoadThrows; }
    uint64_t Spawned() const { return m_Spawned; }
    uint64_t Enumerations() const { return m_Enumerations; }
    uint64_t Draws() const { return m_Draws; }
    uint64_t IconDraws() const { return m_IconDraws; }
    uint64_t BadgeDraws() const { return m_BadgeDraws; }
    uint64_t DrawErrors() const { return m_DrawErrors; }
    uint64_t Removed() const { return m_Removed; }
    Style& StyleRef() { return m_Style; }
    void StyleChanged() { m_ClustersDirty = true; }
    const std::vector<Marker>& Markers() const { return m_Markers; }
    const std::vector<Cluster>& ClusterList() const { return m_Clusters; }
    // Markers held now, per kind (`packmarks stat`'s kinds= field).
    std::array<size_t, KindCount> KindCounts() const {
        std::array<size_t, KindCount> counts{};
        for (const Marker& m : m_Markers) ++counts[m.kind < KindCount ? m.kind : Normal];
        return counts;
    }
    // Retirement accounting (`packmarks why`), per kind since the zone
    // generation last changed.
    const KindStats& Stats(int kind) const { return m_Stats[kind >= 0 && kind < KindCount ? kind : Normal]; }
    // The `n` objects created most often by spawners of `kind`, most first
    // (ties by lower object index): (object index, creates).
    std::vector<std::pair<int, uint64_t>> TopCreates(int kind, size_t n = 4) const { return Top(Stats(kind).creates, n); }
    // The same for the non-enemy objects (`packmarks why <kind> other creates:`).
    std::vector<std::pair<int, uint64_t>> TopOtherCreates(int kind, size_t n = 4) const { return Top(Stats(kind).otherCreates, n); }
    uint64_t UnattributedCreates() const { return m_UnattributedCreates; }   // creates whose creator's kind was unknown
    uint64_t UnattributedOther() const { return m_UnattributedOther; }       // non-enemy creates whose creator's kind was unknown
    uint64_t Frame() const { return m_Frame; }                               // the last frame OnFrame ran
    // Birth-signal reads since the game started that could not be made
    // (`packmarks stat`'s unread=): under `state` an enemyArray read that
    // threw; under `kind` also an absent enemyArray or spawnPack where the
    // kind needs it, or a key the guard refused. Never reset by a zone change.
    uint64_t Unread() const { return m_Unread; }
    // `packgone`-mode markers held now whose signal read born, per kind, and
    // those of them with no member recorded (`packmarks why`'s held=, unlinked=).
    std::array<Held, KindCount> HeldNow() const {
        std::array<Held, KindCount> out{};
        for (const Marker& m : m_Markers) {
            const uint8_t k = m.kind < KindCount ? m.kind : Normal;
            if (kKindRules[k].mode != Mode::PackGone || !m.bornSeen) continue;
            ++out[k].held;
            if (!HasMembers(m.id)) ++out[k].unlinked;
        }
        return out;
    }
    // The birth memory's size: ids, and room/kind/position keys.
    size_t MemoryIds() const { return m_MemoryIds.size(); }
    size_t MemoryPositions() const { return m_MemoryPositions.size(); }
    size_t MemberCreators() const { return m_Members.size(); }

    void Clear() {
        m_Markers.clear(); m_Index.clear(); m_Clusters.clear(); m_ClustersDirty = false; m_Cursor = 0;
        for (long& peak : m_FamilyPeak) peak = 0;
    }

private:
    PackMarkers() = default;

    static bool IsNumber(const RValue& v) {
        return v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64;
    }
    // instance_find returns an identity on this runner (real or REF); an
    // object-valued handle still answers variable_instance_get("id").
    static bool Identity(const RValue& inst, int64_t& out) {
        try {
            const bool direct = IsNumber(inst) || inst.m_Kind == VALUE_REF;
            const double v = direct ? inst.ToDouble()
                : g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") }).ToDouble();
            if (!std::isfinite(v) || v < 0 || v > 9007199254740991.0 || std::floor(v) != v) return false;
            out = static_cast<int64_t>(v);
            return true;
        } catch (...) { return false; }
    }
    // The census's reading of one variable on a creator. Absent is asked
    // first, so an undefined value means the variable exists and is unset.
    static Tally TallyVariable(const RValue& inst, const char* name, bool wantArray) {
        try {
            if (!g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue(name) }).ToBoolean()) return TallyAbsent;
            const RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(name) });
            if (IsUndefined(v)) return TallyUndefined;
            if (wantArray ? IsArray(v) : IsNumber(v)) return TallyValue;
            return TallyOther;
        } catch (...) { return TallyOther; }
    }
    static bool IsUndefined(const RValue& v) {
        return (static_cast<uint32_t>(v.m_Kind) & 0x0FFFFFFFu) == static_cast<uint32_t>(VALUE_UNDEFINED);
    }
    static bool IsWhole(double v) { return std::isfinite(v) && std::fabs(v) <= 9007199254740991.0 && std::floor(v) == v; }
    static std::vector<std::pair<int, uint64_t>> Top(const std::unordered_map<int, uint64_t>& counts, size_t n) {
        std::vector<std::pair<int, uint64_t>> top(counts.begin(), counts.end());
        std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        if (top.size() > n) top.resize(n);
        return top;
    }
    // The protected store's record for `key`, through the game's own getter,
    // called by name. False when the guard refuses the key or the call threw;
    // a refused key never reaches the call.
    static bool ReadStore(const RValue& key, RValue& value) {
        if (!KeyInRange(key)) return false;
        try { value = g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_PC_GetVariableGMLWrapper.data(), { key }); }
        catch (...) { return false; }
        return true;
    }
    // A kind's signal as the census found it: enemyArray's tally, or the pack
    // state read through spawnPack.
    static bool BornBy(int kind, Tally enemyArray, const PackRead& pack) {
        const KindRule& rule = kKindRules[kind >= 0 && kind < KindCount ? kind : Normal];
        if (rule.signal == Signal::EnemyArray) return enemyArray == TallyValue;
        return pack.bucket == PackNumber && pack.value >= rule.threshold;
    }
    enum class Read : uint8_t { Born = 0, Unborn, Unread };
    // The `kind` policy's read of one spawner's birth signal, by name on the
    // spawner itself, where the marker is used. A read that cannot be made
    // (an absent variable where the kind needs it, a refused key, a throw)
    // answers Unread and is counted; anything else that is not born is
    // Unborn.
    Read ReadSignal(const RValue& idv, int kind) {
        const KindRule& rule = kKindRules[kind >= 0 && kind < KindCount ? kind : Normal];
        try {
            if (rule.signal == Signal::EnemyArray) {
                const RValue packs = g_Yytk->CallBuiltin("variable_instance_get", { idv, RValue("enemyArray") });
                if (IsArray(packs)) return Read::Born;
                // Undefined is both "not born yet" and what an absent
                // variable answers; only the absent one is unread.
                if (IsUndefined(packs) && !g_Yytk->CallBuiltin("variable_instance_exists", { idv, RValue("enemyArray") }).ToBoolean()) { ++m_Unread; return Read::Unread; }
                return Read::Unborn;
            }
            const RValue key = g_Yytk->CallBuiltin("variable_instance_get", { idv, RValue("spawnPack") });
            RValue state;
            if (!ReadStore(key, state)) { ++m_Unread; return Read::Unread; }
            return IsNumber(state) && state.ToDouble() >= rule.threshold ? Read::Born : Read::Unborn;
        } catch (...) { ++m_Unread; return Read::Unread; }
    }
    // The `kind` policy's rotating check of one live spawner: the reason to
    // retire it, or ReasonCount to keep it.
    Reason KindCheck(Marker& m, const RValue& idv) {
        const int k = m.kind < KindCount ? m.kind : Normal;
        const Read signal = ReadSignal(idv, k);
        if (kKindRules[k].mode == Mode::Birth) return signal == Read::Born ? ReasonKindBorn : ReasonCount;
        // `packgone`: born or not, the marker stays until its pack is gone.
        if (signal == Read::Born) m.bornSeen = true;
        else if (signal == Read::Unborn) m.bornSeen = false;
        // The members read is made and counted whether or not it may retire
        // (goneRead: each turn to gone), so a session can tell a read that
        // fired at the kill from one that fired while the pack was alive.
        const bool gone = PackGone(m.id);
        if (gone && !m.goneSeen) ++m_Stats[k].goneRead;
        m.goneSeen = gone;
        return gone && PackGoneRetires() ? ReasonPackGone : ReasonCount;
    }
    bool HasMembers(int64_t creatorId) const {
        auto it = m_Members.find(creatorId);
        return it != m_Members.end() && !it->second.empty();
    }
    static bool MemberExists(int64_t member) {
        try { return g_Yytk->CallBuiltin("instance_exists", { RValue((double)member) }).ToBoolean(); }
        catch (...) { return true; }   // unknown: never retire on a failed read
    }
    // At least one member recorded and none exists now. Stops at the first
    // living member.
    bool PackGone(int64_t creatorId) const {
        auto it = m_Members.find(creatorId);
        if (it == m_Members.end() || it->second.empty()) return false;
        for (const auto& member : it->second) if (MemberExists(member.first)) return false;
        return true;
    }
    // The birth memory (see the class comment).
    using PositionKey = std::tuple<int64_t, int, int64_t, int64_t>;
    static PositionKey PositionOf(int64_t room, int kind, double x, double y) {
        return PositionKey{ room, kind, static_cast<int64_t>(std::llround(x)), static_cast<int64_t>(std::llround(y)) };
    }
    void Remember(const Marker& m) {
        if (m_MemoryIds.size() >= kMemoryCap) m_MemoryIds.clear();
        m_MemoryIds.insert(m.id);
        if (m.room == kUnknownRoom) return;
        if (m_MemoryPositions.size() >= kMemoryCap) m_MemoryPositions.clear();
        m_MemoryPositions.insert(PositionOf(m.room, m.kind, m.x, m.y));
    }
    bool Remembered(int64_t id, int kind, double x, double y) const {
        if (m_MemoryIds.count(id)) return true;
        return m_Room != kUnknownRoom && m_MemoryPositions.count(PositionOf(m_Room, kind, x, y)) != 0;
    }
    // First sight of an id in this zone: was it listed in an earlier visit
    // to the same room? An unknown room is neither recorded nor matched.
    void NoteSighting(int64_t id, int kind) {
        if (!m_Seen.insert(id).second || m_Room == kUnknownRoom) return;
        const std::pair<int64_t, int64_t> key{ id, m_Room };
        auto it = m_IdRoom.find(key);
        if (it != m_IdRoom.end()) {
            if (it->second != m_Zone) ++m_Stats[kind].sameid;
            it->second = m_Zone;
            return;
        }
        if (m_IdRoom.size() >= kMemoryCap) m_IdRoom.clear();
        m_IdRoom.emplace(key, m_Zone);
    }
    // By the runtime's own is_array, as DungeonChestEstimateTotal reads the
    // same variable.
    static bool IsArray(const RValue& v) {
        if (IsUndefined(v)) return false;
        return g_Yytk->CallBuiltin("is_array", { v }).ToBoolean();
    }
    // The kind of the creator a create was attributed to: its marker's, the
    // kind it was listed under this zone, or its object index's.
    int KindOfCreator(int64_t creatorId, int creatorObject) const {
        auto it = m_Index.find(creatorId);
        if (it != m_Index.end()) return m_Markers[it->second].kind;
        auto listed = m_KindOf.find(creatorId);
        if (listed != m_KindOf.end()) return listed->second;
        if (creatorObject >= 0)
            for (int k = 0; k < KindCount; ++k)
                if (m_ObjResolved[k] && m_ObjIdx[k] == creatorObject) return k;
        return -1;
    }
    void NoteRetired(const Marker& m, Reason reason, uint64_t frame) {
        KindStats& s = m_Stats[m.kind < KindCount ? m.kind : Normal];
        ++s.retired[reason];
        const int64_t age = frame >= m.firstSeen ? static_cast<int64_t>(frame - m.firstSeen) : 0;
        if (s.ageMin < 0 || age < s.ageMin) s.ageMin = age;
        if (age > s.ageMax) s.ageMax = age;
    }
    void ResetStats() {
        for (KindStats& s : m_Stats) s = KindStats{};
        m_KindOf.clear();
        m_UnattributedCreates = 0;
        m_UnattributedOther = 0;
        m_Seen.clear();
        m_RememberedListed.clear();
        m_CreatorZone.clear();
    }
    int ObjectIndex(int kind) {
        if (m_ObjResolved[kind]) return m_ObjIdx[kind];
        m_ObjResolved[kind] = true;
        m_ObjIdx[kind] = -1;
        try {
            RValue r = g_Yytk->CallBuiltin("asset_get_index", { RValue(kCreatorObjects[kind]) });
            const double d = r.ToDouble();
            if (std::isfinite(d) && d >= 0) m_ObjIdx[kind] = static_cast<int>(d);
        } catch (...) {}
        return m_ObjIdx[kind];
    }
    // One sprite_add per kind, relative path first, absolute second. The
    // runner answers with an asset reference (VALUE_REF) on this game, whose
    // runner-side conversion is the index; a real is accepted as well.
    void LoadIcons() {
        m_IconsTried = true;
        for (int k = 0; k < KindCount; ++k) {
            std::string relative, absolute;
            if (m_Sprite[k] >= 0 || !m_IconProvider || !m_IconProvider(k, relative, absolute)) continue;
            for (const std::string* path : { &relative, &absolute }) {
                if (path->empty()) continue;
                try {
                    // Origin 8,8 fits the shipped 16x16 icons; a player's PNG of
                    // another size is re-centred right after.
                    RValue r = g_Yytk->CallBuiltin("sprite_add", { RValue(path->c_str()), RValue(1.0), RValue(0.0), RValue(0.0), RValue(8.0), RValue(8.0) });
                    m_IconLastKind = static_cast<int>(r.m_Kind);
                    const bool usable = IsNumber(r) || r.m_Kind == VALUE_REF;
                    const double idx = usable ? r.ToDouble() : -1.0;
                    m_IconLastValue = idx;
                    if (!std::isfinite(idx) || idx < 0) continue;
                    m_Sprite[k] = static_cast<int>(idx);
                    ++m_IconsLoaded;
                    if (path == &absolute) ++m_IconsViaAbsolute;
                    try {
                        const double w = g_Yytk->CallBuiltin("sprite_get_width", { RValue(idx) }).ToDouble();
                        const double h = g_Yytk->CallBuiltin("sprite_get_height", { RValue(idx) }).ToDouble();
                        if (std::isfinite(w) && std::isfinite(h) && w > 0 && h > 0 && (w != 16 || h != 16))
                            g_Yytk->CallBuiltin("sprite_set_offset", { RValue(idx), RValue(std::floor(w / 2)), RValue(std::floor(h / 2)) });
                    } catch (...) {}
                    break;
                } catch (...) { ++m_IconLoadThrows; }
            }
        }
    }
    void BuildClusters() {
        m_ClustersDirty = false;
        m_Clusters.clear();
        if (m_Style.clusterPx <= 0) {
            m_Clusters.reserve(m_Markers.size());
            for (const Marker& m : m_Markers) m_Clusters.push_back({ m.x, m.y, m.kind, 1 });
            return;
        }
        struct Sum { double x, y; uint8_t kind; unsigned count; };
        std::unordered_map<int64_t, size_t> cells;
        std::vector<Sum> sums;
        for (const Marker& m : m_Markers) {
            const int64_t cx = static_cast<int64_t>(std::llround(m.x / m_Style.clusterPx));
            const int64_t cy = static_cast<int64_t>(std::llround(m.y / m_Style.clusterPx));
            const int64_t key = (cx << 32) ^ (cy & 0xffffffff);
            auto it = cells.find(key);
            if (it == cells.end()) { cells.emplace(key, sums.size()); sums.push_back({ m.x, m.y, m.kind, 1 }); continue; }
            Sum& s = sums[it->second];
            s.x += m.x; s.y += m.y; ++s.count;
            const uint8_t k = m.kind < KindCount ? m.kind : Normal;
            const uint8_t cur = s.kind < KindCount ? s.kind : Normal;
            if (kKindPriority[k] > kKindPriority[cur]) s.kind = m.kind;
        }
        m_Clusters.reserve(sums.size());
        for (const Sum& s : sums) m_Clusters.push_back({ s.x / s.count, s.y / s.count, s.kind, s.count });
    }
    // Seven instance_number calls: true when any family now has more
    // instances than it ever had in this zone.
    bool PollFamilies() {
        bool grew = false;
        for (int k = 0; k < KindCount; ++k) {
            const int obj = ObjectIndex(k);
            if (obj < 0) continue;
            try {
                const long n = static_cast<long>(g_Yytk->CallBuiltin("instance_number", { RValue((double)obj) }).ToDouble());
                if (n > m_FamilyPeak[k]) { m_FamilyPeak[k] = n; grew = true; }
            } catch (...) {}
        }
        return grew;
    }
    void Enumerate(uint64_t frame) {
        m_Dirty = false;
        m_LastEnumerate = frame;
        ++m_Enumerations;
        std::vector<Marker> fresh;
        std::unordered_map<int64_t, size_t> index;
        for (int k = 0; k < KindCount; ++k) {
            const int obj = ObjectIndex(k);
            if (obj < 0) continue;
            int n = 0;
            try { n = static_cast<int>(g_Yytk->CallBuiltin("instance_number", { RValue((double)obj) }).ToDouble()); } catch (...) { continue; }
            if (n > m_FamilyPeak[k]) m_FamilyPeak[k] = n;
            for (int i = 0; i < n; ++i) {
                try {
                    RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)obj), RValue((double)i) });
                    int64_t id = -1;
                    if (!Identity(inst, id) || index.count(id)) continue;
                    const double x = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
                    const double y = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
                    if (!std::isfinite(x) || !std::isfinite(y)) continue;
                    NoteSighting(id, k);
                    // Keep what an earlier enumeration already learned about
                    // this spawner, so a re-enumeration cannot resurrect a
                    // marker or restart its give-up clock.
                    auto old = m_Index.find(id);
                    Marker m{ id, x, y, static_cast<uint8_t>(k), false, frame, m_Room, false };
                    if (old != m_Index.end()) {
                        const Marker& was = m_Markers[old->second];
                        m.armed = was.armed; m.firstSeen = was.firstSeen; m.room = was.room; m.bornSeen = was.bornSeen; m.goneSeen = was.goneSeen;
                    }
                    else if (m_Spent.count(id)) continue;
                    else {
                        if (GetRetire() == Retire::Kind) {
                            // The birth memory: a pack already created in an
                            // earlier visit is not marked again.
                            if (Remembered(id, k, x, y)) {
                                if (m_RememberedListed.insert(id).second) ++m_Stats[k].remembered;
                                m_KindOf.emplace(id, static_cast<uint8_t>(k));
                                continue;
                            }
                            // Listed already born: a `birth`-mode kind gets no
                            // marker (listed and kindborn at age 0); a
                            // `packgone` one keeps its marker, held.
                            if (ReadSignal(RValue((double)id), k) == Read::Born) {
                                if (kKindRules[k].mode == Mode::Birth) {
                                    if (m_KindOf.emplace(id, static_cast<uint8_t>(k)).second) ++m_Stats[k].listed;
                                    if (m_Spent.size() > 65536) m_Spent.clear();
                                    m_Spent.insert({ id, true });
                                    NoteRetired(m, ReasonKindBorn, frame);
                                    Remember(m);
                                    continue;
                                }
                                m.bornSeen = true;
                            }
                        }
                        if (m_KindOf.emplace(id, static_cast<uint8_t>(k)).second) ++m_Stats[k].listed;
                    }
                    index.emplace(id, fresh.size());
                    fresh.push_back(m);
                } catch (...) {}
            }
        }
        m_Markers.swap(fresh);
        m_Index.swap(index);
        m_Cursor = 0;
        m_ClustersDirty = true;
    }
    void ValidateSlice(uint64_t frame) {
        if (m_Markers.empty()) return;
        size_t budget = (std::min)(kValidatePerFrame, m_Markers.size());
        while (budget-- > 0 && !m_Markers.empty()) {
            if (m_Cursor >= m_Markers.size()) m_Cursor = 0;
            Marker& m = m_Markers[m_Cursor];
            Reason reason = ReasonCount;   // ReasonCount = keep
            const Retire policy = GetRetire();
            try {
                RValue idv((double)m.id);
                if (!g_Yytk->CallBuiltin("instance_exists", { idv }).ToBoolean()) reason = ReasonDestroyed;
                else if (policy == Retire::Kind) reason = KindCheck(m, idv);
                else if (policy == Retire::State) {
                    // The spawner's own state, read by name where the marker
                    // is used: enemyArray turns into an array at the birth.
                    // Absent or any other value keeps the marker; a read that
                    // threw is counted as unread.
                    try { if (IsArray(g_Yytk->CallBuiltin("variable_instance_get", { idv, RValue("enemyArray") }))) reason = ReasonStateBorn; }
                    catch (...) { ++m_Unread; }
                } else {
                    RValue t = g_Yytk->CallBuiltin("variable_instance_get", { idv, RValue("enemyCreatorTimer") });
                    if (IsNumber(t)) m.armed = true;
                    else if (m.armed) reason = ReasonTimerGone;                                   // timer destroyed: the pack was born
                    else if (frame >= m.firstSeen + kUnarmedGiveUpFrames) reason = ReasonGivenUp;   // never armed: spent before we looked
                }
            } catch (...) {}
            if (reason != ReasonCount) {
                const Marker gone = m;
                m_Spent.insert({ gone.id, true });
                Remove(m_Cursor);
                NoteRetired(gone, reason, frame);
                // Every birth rule is remembered; a gone spawner or a give-up
                // says nothing about a birth. A `packgone` retirement happens
                // only with the members rule on (both builds, unless the
                // research build's `packmarks gonerule off`; see the class comment).
                if (reason != ReasonDestroyed && reason != ReasonGivenUp) Remember(gone);
                ++m_Removed;
            }
            else ++m_Cursor;
        }
    }
    void Remove(size_t pos) {
        const int64_t id = m_Markers[pos].id;
        const size_t last = m_Markers.size() - 1;
        if (pos != last) {
            m_Markers[pos] = m_Markers[last];
            m_Index[m_Markers[pos].id] = pos;
        }
        m_Markers.pop_back();
        m_Index.erase(id);
        m_ClustersDirty = true;
        if (m_Spent.size() > 65536) m_Spent.clear();
    }

    std::atomic<bool> m_Enabled{ false };
    bool m_Dirty{ false };
    uint64_t m_Zone{ 0 }, m_LastEnumerate{ 0 };
    long m_FamilyPeak[KindCount] = { 0, 0, 0, 0, 0, 0, 0 };
    size_t m_Cursor{ 0 };
    uint64_t m_CopiesNoted{ 0 };
    uint64_t m_Spawned{ 0 }, m_Enumerations{ 0 }, m_Draws{ 0 }, m_IconDraws{ 0 }, m_BadgeDraws{ 0 }, m_DrawErrors{ 0 }, m_Removed{ 0 };
    int m_ObjIdx[KindCount] = { -1, -1, -1, -1, -1, -1, -1 };
    bool m_ObjResolved[KindCount] = { false, false, false, false, false, false, false };
    int m_Sprite[KindCount] = { -1, -1, -1, -1, -1, -1, -1 };
    int m_IconsLoaded{ 0 }, m_IconsViaAbsolute{ 0 }, m_IconLastKind{ -1 };
    double m_IconLastValue{ -1.0 };
    uint64_t m_IconLoadThrows{ 0 };
    bool m_IconsTried{ false };
    IconProvider m_IconProvider{ nullptr };
    std::vector<Marker> m_Markers;
    std::unordered_map<int64_t, size_t> m_Index;
    std::unordered_map<int64_t, bool> m_Spent;   // ids whose pack was born or which were spent; never re-marked this zone
    std::atomic<Retire> m_Retire{ Retire::Kind };
    std::atomic<bool> m_PackGoneRetires{ true };   // on in both builds; `packmarks gonerule` (research) switches it
    uint64_t m_Frame{ 0 };
    KindStats m_Stats[KindCount];
    std::unordered_map<int64_t, uint8_t> m_KindOf;   // every id listed this zone -> its kind
    uint64_t m_UnattributedCreates{ 0 }, m_UnattributedOther{ 0 };
    uint64_t m_Unread{ 0 };                          // for the whole game session
    int64_t m_Room{ kUnknownRoom };                  // the room key OnFrame was last given
    // The birth memory, for the whole game session (bounded by kMemoryCap).
    std::unordered_set<int64_t> m_MemoryIds;
    std::set<PositionKey> m_MemoryPositions;
    std::map<std::pair<int64_t, int64_t>, uint64_t> m_IdRoom;   // (id, room) -> the zone generation it was last listed in
    // Per zone: ids enumerated, ids the memory withheld, creates per spawner.
    std::unordered_set<int64_t> m_Seen, m_RememberedListed;
    std::unordered_map<int64_t, CreatorCreates> m_CreatorZone;
    // Pack members per spawner, for the whole game session: (instance id, object).
    std::unordered_map<int64_t, std::vector<std::pair<int64_t, int>>> m_Members;
    std::vector<Cluster> m_Clusters;
    bool m_ClustersDirty{ false };
    Style m_Style;
};

} // namespace ForgePact
