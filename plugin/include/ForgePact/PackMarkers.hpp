#pragma once

#include "Common.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
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
    // Which kind a mixed cluster shows: the rarest pack wins.
    static constexpr int kKindPriority[KindCount] = { 0, 1, 5, 4, 3, 2, 6 };
    struct Marker {
        int64_t id;
        double x, y;
        uint8_t kind;
        bool armed;            // enemyCreatorTimer was seen as a number: initialised, not yet spawned
        uint64_t firstSeen;    // frame the marker was enumerated
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
    // A marker is dropped when a creator has shown no timer for this long
    // after being enumerated: spawners initialise within a second or two, so
    // one that never reports a timer was already spent when we first saw it.
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
    // would be built.
    template <class Probe>
    void OnFrame(uint64_t frame, uint64_t zoneGeneration, Probe mapReadable) {
        if (!Enabled()) return;
        if (zoneGeneration != m_Zone) { m_Zone = zoneGeneration; Clear(); m_Spent.clear(); m_Dirty = true; }
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
    // a monster, so its pack exists and the marker must go now rather than
    // when the rotating check reaches it.
    void MarkSpawned(int64_t creatorId) {
        if (!Enabled()) return;
        // Remembered even when no marker exists yet: a pack born during the
        // zone's first frames must not get a marker from a later enumeration.
        m_Spent.insert({ creatorId, true });
        auto it = m_Index.find(creatorId);
        if (it == m_Index.end()) return;
        Remove(it->second);
        ++m_Spawned;
    }

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
                    // Keep what an earlier enumeration already learned about
                    // this spawner, so a re-enumeration cannot resurrect a
                    // marker or restart its give-up clock.
                    auto old = m_Index.find(id);
                    Marker m{ id, x, y, static_cast<uint8_t>(k), false, frame };
                    if (old != m_Index.end()) { m.armed = m_Markers[old->second].armed; m.firstSeen = m_Markers[old->second].firstSeen; }
                    else if (m_Spent.count(id)) continue;
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
            bool remove = false;
            try {
                RValue idv((double)m.id);
                if (!g_Yytk->CallBuiltin("instance_exists", { idv }).ToBoolean()) remove = true;
                else {
                    RValue t = g_Yytk->CallBuiltin("variable_instance_get", { idv, RValue("enemyCreatorTimer") });
                    if (IsNumber(t)) m.armed = true;
                    else if (m.armed) remove = true;                                   // timer destroyed: the pack was born
                    else if (frame >= m.firstSeen + kUnarmedGiveUpFrames) remove = true; // never armed: spent before we looked
                }
            } catch (...) {}
            if (remove) { m_Spent.insert({ m.id, true }); Remove(m_Cursor); ++m_Removed; }
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
    std::vector<Cluster> m_Clusters;
    bool m_ClustersDirty{ false };
    Style m_Style;
};

} // namespace ForgePact
