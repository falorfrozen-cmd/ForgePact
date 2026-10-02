#pragma once

#include "Common.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ForgePact {

// Hidden loot sleep: a ground item the player's own loot filter hides is put
// to sleep with instance_deactivate_object at the end of the frame it dropped
// in, and shown again while the player holds a key.
//
// Why (measured 2026-09-28, docs/hidden-loot-research.md, Live 1): a ground
// item the filter hides is never drawn, but the runner still walks it every
// frame, and the loot manager's own Begin Step looks at every one. For that
// pile, 2,736 hidden items, each cost about 2.4-5.6 microseconds of frame
// time per frame; asleep, the cost went away, and woken it came back.
//
// What decides: the game, never this class. The game's LootGroundInit runs
// the bound filter and leaves its verdict, `lootFilterVisible`, on the new
// instance. Monster drops (LootGroundCreateFromItem) and bag drops
// (LootGroundDrop) call it (a static reading; LootGroundCreate names it as a
// callee, but its own path was not traced). ModuleMain hooks LootGroundInit;
// the hook calls the game first and then hands this class what the call
// carried (argument 0, argument 1, `self`), still inside the call. The class
// keeps only durable handles of them (Durable): a number or a reference as
// it is; an instance pointer, asked instance_exists while the call still
// holds it live, as its own `id`; anything else as undefined. Those two
// reads are all it does inside the call, and no raw pointer outlives it:
// `self` may be a monster that is dying, freed before the frame's end. At
// the frame's end (EVENT_FRAME, after every step event) the class finds
// which handle is a live Loot_Ground_obj, reads the verdict again there,
// and puts a hidden one to sleep. Not inside the call: the rest of the entry
// point, and whoever called it, may still address the new instance, and a
// deactivated instance is absent to them.
//
// The handles are tried in order (argument 0, argument 1, `self`) and the
// first that is a ground item wins; the stat line counts which slot it was
// (by-arg0, by-arg1, by-self), how many pointers became ids inside the call
// (reduced) and how many did not (dropped, an item struct each time if the
// reading of the arguments holds), and the last call's kinds as passed. A
// handle whose instance is gone by the frame's end answers false to
// instance_exists and is passed over.
//
// Never touched: a verdict that reads visible (quest items, skipLootFilter
// items, whatever the player's filter shows), anything without a verdict,
// anything that is not a Loot_Ground_obj (coins), anything in a persistent
// room. While the key is held, with the game's window in front, every slept
// item is woken and its verdict and the built-in `visible` are written
// visible; on release they are written hidden again and slept. Switching off
// wakes everything with the hidden verdict in place, which is exactly the
// game's own state for a hidden item. A room change forgets every handle
// without a call: the room's end took the instances with it.
//
// If the hook cannot see the game's compiled calls (table-only) or was not
// installed, a pass over the awake ground items every 18 frames (the game's
// own 0.3 s filter refresh) takes its place.
//
// Everything below is header-only and reaches the game only through
// g_Yytk->CallBuiltin; the key state and the foreground test are handed in,
// so tests/hidden_loot_harness.cpp can compile the real class against a
// controlled runner.
class HiddenLootMod {
public:
    static constexpr int kDefaultKey = 164;            // VK_LMENU, Left Alt (owner, 2026-09-28)
    static constexpr uint64_t kPassFrames = 18;        // the fallback pass: Alarm 9's 0.3 s at 60 fps
    static constexpr long kWalkCap = 8192;             // the switch-on walk and a pass, like lootcensus
    static constexpr size_t kPendingCap = 8192;        // drop calls held for one frame at most
    static constexpr const char* kVerdict = "lootFilterVisible";

    enum class Route { None, TableOnly, Both };
    // True while the virtual key is down / while the game's window is the
    // foreground window. Set by ModuleMain; absent, no key is ever held.
    using KeyDown = bool (*)(int vk);
    using GameInFront = bool (*)();

    struct Stats {
        uint64_t inits = 0, slept = 0, visible = 0, noFilterVar = 0, unidentified = 0, gone = 0;
        uint64_t passes = 0, skippedPersistent = 0, errors = 0;
        uint64_t visibleUnwritten = 0;   // the built-in `visible` did not take a write (the verdict did)
        // Which of the call's values identified the item at the frame's end.
        uint64_t byArg0 = 0, byArg1 = 0, bySelf = 0;
        // Instance pointers made an `id` inside the call, and those that were
        // not (instance_exists false, an id that is not a number, a read that threw).
        uint64_t reduced = 0, dropped = 0;
        // The last call's argument 0, argument 1 and `self`, as passed (before
        // Durable): num, ref, obj, undef or other; "-" before the first call.
        const char* kinds[3] = { "-", "-", "-" };
    };
    struct WalkResult { long slept = 0, visible = 0, noFilterVar = 0; };
    struct OffResult { long woken = 0, existAfter = 0; };

    static HiddenLootMod& Instance() { static HiddenLootMod s; return s; }

    bool Enabled() const { return m_Enabled; }
    int Key() const { return m_Key; }
    bool Held() const { return m_Held; }
    Route GetRoute() const { return m_Route; }
    const char* RouteName() const {
        switch (m_Route) {
        case Route::Both: return "both";
        case Route::TableOnly: return "table-only";
        case Route::None: return "none";
        }
        return "none";
    }
    size_t AsleepNow() const { return m_AsleepNow; }
    size_t ShownNow() const { return m_ShownNow; }
    const Stats& StatsRef() const { return m_Stats; }
    int CallsThisFrame() const { return m_Calls; }

    // 0 means no key; 1 and 2 are the mouse buttons the game plays with.
    static bool KeyAllowed(int vk) { return vk == 0 || (vk >= 3 && vk <= 254); }
    // Stored whether or not the mod is on; read only while it is.
    bool SetKey(int vk) {
        if (!KeyAllowed(vk)) return false;
        m_Key = vk;
        return true;
    }
    void SetInput(KeyDown keyDown, GameInFront inFront) { m_KeyDown = keyDown; m_InFront = inFront; }
    void SetRoute(Route route) { m_Route = route; }

    // Switched on: one walk over the ground items already there (capped),
    // putting to sleep what the filter hides. `walk` false skips it (before
    // the game is set up there is no ground to walk).
    template <class RoomProbe>
    WalkResult Enable(int64_t roomKey, RoomProbe roomInfo, bool walk = true) {
        m_Calls = 0;
        m_Enabled = true;
        WalkResult result;
        try {
            if (roomKey != m_RoomKey) EnterRoom(roomKey);
            if (walk && LootIndex()) Walk(roomInfo, &result);
        } catch (...) { ++m_Stats.errors; }
        m_LastPass = 0;
        return result;
    }

    // Switched off: every item it put to sleep is woken, a shown one gets the
    // game's hidden verdict back, and all of them are forgotten.
    OffResult Disable() {
        m_Calls = 0;
        OffResult result;
        std::vector<RValue> woken;
        for (auto& entry : m_Items) {
            Item& item = entry.second;
            try {
                if (item.asleep) {
                    Call("instance_activate_object", { item.handle });
                    woken.push_back(item.handle);
                    ++result.woken;
                } else if (item.shown) {
                    if (!Exists(item.handle)) { ++m_Stats.gone; continue; }
                    WriteVerdict(item.handle, false);
                }
            } catch (...) { ++m_Stats.errors; }
        }
        for (const RValue& handle : woken) {
            try { if (Exists(handle)) ++result.existAfter; } catch (...) { ++m_Stats.errors; }
        }
        Forget();
        m_Pending.clear();
        m_Enabled = false;
        m_Held = false;
        return result;
    }

    // From the LootGroundInit hook, after the game's own trampoline returned
    // but still inside the call: what the call carried, reduced to durable
    // handles (Durable) and kept for the frame's end. Reads only: nothing is
    // deactivated or written here, and a read that throws is caught here,
    // never left to unwind through the game's own call.
    void OnInit(const RValue& arg0, const RValue& arg1, const RValue& self) {
        if (!m_Enabled) return;
        ++m_Stats.inits;
        m_Stats.kinds[0] = KindName(arg0);
        m_Stats.kinds[1] = KindName(arg1);
        m_Stats.kinds[2] = KindName(self);
        if (m_Pending.size() >= kPendingCap) { ++m_Stats.errors; return; }
        try {
            PendingCall call;
            call.candidates[0] = Durable(arg0);
            call.candidates[1] = Durable(arg1);
            call.candidates[2] = Durable(self);
            m_Pending.push_back(std::move(call));
        } catch (...) { ++m_Stats.errors; }
    }

    // Per-frame work from the frame callback. `roomKey` changes on every room
    // change; `roomInfo()` (name, persistence, readable) is asked once a room,
    // the first time something could be put to sleep there.
    template <class RoomProbe>
    void OnFrame(uint64_t frame, int64_t roomKey, RoomProbe roomInfo) {
        m_Calls = 0;
        if (!m_Enabled) return;
        try {
            if (roomKey != m_RoomKey) EnterRoom(roomKey);
            // The key counts only while one is assigned and the game's own
            // window is in front: a key held in another window shows nothing.
            bool held = false;
            if (m_Key != 0 && m_InFront && m_KeyDown && m_InFront()) held = m_KeyDown(m_Key);
            if (held && !m_Held) { m_Held = true; ShowAll(); }
            else if (!held && m_Held) { m_Held = false; HideAll(); }
            if (!m_Pending.empty()) ProcessPending(roomInfo);
            if (m_Route != Route::Both && frame >= m_LastPass + kPassFrames) {
                m_LastPass = frame;
                ++m_Stats.passes;
                if (LootIndex()) Walk(roomInfo, nullptr);
            }
        } catch (...) { ++m_Stats.errors; }
    }

private:
    struct Item {
        RValue handle;
        bool asleep = false;
        bool shown = false;
        bool everSlept = false;
    };
    struct PendingCall { RValue candidates[3]; };

    HiddenLootMod() = default;

    static uint32_t Kind(const RValue& v) { return static_cast<uint32_t>(v.m_Kind) & 0x0FFFFFFFU; }
    // A number or a typed reference: its numeric value, or false.
    static bool Number(const RValue& v, double& out) {
        const uint32_t kind = Kind(v);
        if (kind != VALUE_REAL && kind != VALUE_INT32 && kind != VALUE_INT64 && kind != VALUE_REF) return false;
        try { out = v.ToDouble(); } catch (...) { return false; }
        return std::isfinite(out);
    }
    RValue Call(const char* name, std::vector<RValue> args) {
        ++m_Calls;
        return g_Yytk->CallBuiltin(name, args);
    }
    bool Exists(const RValue& handle) { return Call("instance_exists", { handle }).ToBoolean(); }

    // A value's kind as the stat line names it.
    static const char* KindName(const RValue& v) {
        const uint32_t kind = Kind(v);
        if (kind == VALUE_REAL || kind == VALUE_INT32 || kind == VALUE_INT64) return "num";
        if (kind == VALUE_REF) return "ref";
        if (kind == VALUE_OBJECT) return "obj";
        if (kind == VALUE_UNDEFINED) return "undef";
        return "other";
    }
    // A value the drop call carried, made safe to keep past the call. A
    // number or a reference names an instance by id and is kept as it is,
    // with no read. An instance pointer is only valid while the call holds
    // it (the caller may be freed before the frame's end), so it is asked
    // instance_exists now and, if it is one, replaced by its own `id`, which
    // this runner answers as a reference. Anything else (an item struct,
    // which instance_exists answers false, an id that is not a number, a
    // kind no instance has) is kept as undefined. The kind decides how a
    // value is kept, never whether the frame's end looks at it: every kind
    // an instance arrives as is kept or resolved. Two reads at most.
    RValue Durable(const RValue& v) {
        const uint32_t kind = Kind(v);
        if (kind == VALUE_REAL || kind == VALUE_INT32 || kind == VALUE_INT64 || kind == VALUE_REF) return v;
        if (kind != VALUE_OBJECT) return RValue();
        try {
            if (Call("instance_exists", { v }).ToBoolean()) {
                RValue own = Call("variable_instance_get", { v, RValue("id") });
                double n = -1;
                if (Number(own, n) && n >= 0) { ++m_Stats.reduced; return own; }
            }
        } catch (...) { ++m_Stats.errors; }
        ++m_Stats.dropped;
        return RValue();
    }

    // Loot_Ground_obj's object index, by its SDK name; asked again until it
    // resolves.
    bool LootIndex() {
        if (m_LootIndex >= 0) return true;
        double v = -1;
        try {
            const RValue index = Call("asset_get_index",
                { RValue(std::string(HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Loot_Ground_obj))) });
            if (!Number(index, v)) v = -1;
        } catch (...) { v = -1; }
        m_LootIndex = v >= 0 ? static_cast<int64_t>(v) : -1;
        return m_LootIndex >= 0;
    }

    // The game's verdict: 1 shown, 0 hidden, -1 it carries none, -2 the read
    // threw. The variable is the game's, so it is asked for before it is read.
    int Verdict(const RValue& handle) {
        try {
            if (!Call("variable_instance_exists", { handle, RValue(kVerdict) }).ToBoolean()) return -1;
            return Call("variable_instance_get", { handle, RValue(kVerdict) }).ToBoolean() ? 1 : 0;
        } catch (...) { return -2; }
    }
    // The verdict, and the built-in `visible` so the change shows at once
    // rather than at the game's next 0.3 s refresh. A refused `visible` is
    // counted and left to that refresh.
    void WriteVerdict(const RValue& handle, bool visible) {
        Call("variable_instance_set", { handle, RValue(kVerdict), RValue(visible) });
        try { Call("variable_instance_set", { handle, RValue("visible"), RValue(visible) }); }
        catch (...) { ++m_Stats.visibleUnwritten; }
    }

    // Of what a drop call carried, is this one a live ground item? Whatever
    // its kind: a number, a reference and an instance pointer are all asked.
    bool Identify(const RValue& candidate, RValue& handle, int64_t& id) {
        if (Kind(candidate) == VALUE_UNDEFINED) return false;   // an argument the call did not have
        try {
            if (!Call("instance_exists", { candidate }).ToBoolean()) return false;
            double object = -1;
            if (!Number(Call("variable_instance_get", { candidate, RValue("object_index") }), object)) return false;
            if (static_cast<int64_t>(object) != m_LootIndex) return false;
            const RValue own = Call("variable_instance_get", { candidate, RValue("id") });
            double n = -1;
            if (!Number(own, n) || n < 0) return false;
            handle = own;
            id = static_cast<int64_t>(n);
            return true;
        } catch (...) { return false; }
    }

    // Asked once a room: does this room let the mod put anything to sleep?
    // Not a persistent room (it keeps its instances), and not one whose
    // identity could not be read.
    template <class Info>
    bool RoomSleeps(const Info& info) {
        if (!m_RoomChecked) {
            m_RoomChecked = true;
            m_RoomAllows = info.readable && !info.persistent;
            if (!info.readable) ++m_Stats.errors;
        }
        return m_RoomAllows;
    }
    template <class RoomProbe>
    bool RoomAllows(RoomProbe& roomInfo) { return m_RoomChecked ? m_RoomAllows : RoomSleeps(roomInfo()); }

    void EnterRoom(int64_t roomKey) {
        m_RoomKey = roomKey;
        m_RoomChecked = false;
        m_RoomAllows = false;
        Forget();
    }
    void Forget() {
        m_Items.clear();
        m_Judged.clear();
        m_AsleepNow = 0;
        m_ShownNow = 0;
    }
    bool Known(int64_t id) const { return m_Items.count(id) != 0 || m_Judged.count(id) != 0; }

    // A ground item seen for the first time: the verdict decides; a hidden
    // one is slept, or shown while the key is held.
    template <class RoomAllowsFn>
    void Judge(const RValue& handle, int64_t id, RoomAllowsFn allows, WalkResult* walk) {
        const int verdict = Verdict(handle);
        if (verdict == -2) { ++m_Stats.errors; return; }
        if (verdict == -1) { ++m_Stats.noFilterVar; if (walk) ++walk->noFilterVar; m_Judged.insert(id); return; }
        if (verdict == 1) { ++m_Stats.visible; if (walk) ++walk->visible; m_Judged.insert(id); return; }
        if (!allows()) { ++m_Stats.skippedPersistent; m_Judged.insert(id); return; }
        Item item;
        item.handle = handle;
        if (m_Held) {
            WriteVerdict(handle, true);
            item.shown = true;
            ++m_ShownNow;
        } else {
            Call("instance_deactivate_object", { handle });
            item.asleep = true;
            item.everSlept = true;
            ++m_AsleepNow;
            ++m_Stats.slept;
            if (walk) ++walk->slept;
        }
        m_Items.emplace(id, std::move(item));
    }

    template <class RoomProbe>
    void ProcessPending(RoomProbe& roomInfo) {
        std::vector<PendingCall> calls;
        calls.swap(m_Pending);
        if (!LootIndex()) { m_Stats.unidentified += calls.size(); return; }
        auto allows = [this, &roomInfo] { return RoomAllows(roomInfo); };
        for (const PendingCall& call : calls) {
            RValue handle;
            int64_t id = -1;
            bool found = false;
            int slot = 0;   // 0 argument 0, 1 argument 1, 2 `self`
            for (const RValue& candidate : call.candidates) {
                if (Identify(candidate, handle, id)) { found = true; break; }
                ++slot;
            }
            if (!found) { ++m_Stats.unidentified; continue; }
            ++(slot == 0 ? m_Stats.byArg0 : slot == 1 ? m_Stats.byArg1 : m_Stats.bySelf);
            if (Known(id)) continue;   // the same item handed over twice
            try { Judge(handle, id, allows, nullptr); } catch (...) { ++m_Stats.errors; }
        }
    }

    // The awake ground items as instance_find answers them, collected whole
    // before anything acts on them: putting one to sleep while walking by
    // index would shift every later index down and skip items.
    template <class RoomProbe>
    void Walk(RoomProbe& roomInfo, WalkResult* walk) {
        const RValue object(static_cast<double>(m_LootIndex));
        double count = 0;
        if (!Number(Call("instance_number", { object }), count)) { ++m_Stats.errors; return; }
        const long toWalk = count < static_cast<double>(kWalkCap) ? static_cast<long>(count) : kWalkCap;
        std::vector<std::pair<RValue, int64_t>> found;
        found.reserve(static_cast<size_t>(toWalk > 0 ? toWalk : 0));
        for (long i = 0; i < toWalk; ++i) {
            try {
                RValue handle = Call("instance_find", { object, RValue(static_cast<double>(i)) });
                double id = -1;
                if (Number(handle, id) && id >= 0) found.push_back({ handle, static_cast<int64_t>(id) });
            } catch (...) { ++m_Stats.errors; }
        }
        auto allows = [this, &roomInfo] { return RoomAllows(roomInfo); };
        for (const auto& entry : found) {
            if (Known(entry.second)) continue;
            try { Judge(entry.first, entry.second, allows, walk); } catch (...) { ++m_Stats.errors; }
        }
    }

    // The key went down: every slept item wakes and is written visible.
    void ShowAll() {
        for (auto it = m_Items.begin(); it != m_Items.end();) {
            Item& item = it->second;
            if (!item.asleep) { ++it; continue; }
            try {
                Call("instance_activate_object", { item.handle });
                item.asleep = false;
                --m_AsleepNow;
                if (!Exists(item.handle)) { ++m_Stats.gone; it = m_Items.erase(it); continue; }
                WriteVerdict(item.handle, true);
                item.shown = true;
                ++m_ShownNow;
            } catch (...) { ++m_Stats.errors; }
            ++it;
        }
    }
    // The key came up: every shown item still there is written hidden and
    // slept again. One picked up meanwhile is forgotten, asked nothing but
    // whether it exists.
    void HideAll() {
        for (auto it = m_Items.begin(); it != m_Items.end();) {
            Item& item = it->second;
            if (!item.shown) { ++it; continue; }
            try {
                if (!Exists(item.handle)) {
                    --m_ShownNow;
                    ++m_Stats.gone;
                    it = m_Items.erase(it);
                    continue;
                }
                WriteVerdict(item.handle, false);
                Call("instance_deactivate_object", { item.handle });
                item.shown = false;
                --m_ShownNow;
                item.asleep = true;
                ++m_AsleepNow;
                if (!item.everSlept) { item.everSlept = true; ++m_Stats.slept; }
            } catch (...) { ++m_Stats.errors; }
            ++it;
        }
    }

    bool m_Enabled = false;
    int m_Key = kDefaultKey;
    bool m_Held = false;
    KeyDown m_KeyDown = nullptr;
    GameInFront m_InFront = nullptr;
    Route m_Route = Route::None;
    Stats m_Stats;
    int m_Calls = 0;
    int64_t m_LootIndex = -1;
    int64_t m_RoomKey = INT64_MIN;
    bool m_RoomChecked = false;
    bool m_RoomAllows = false;
    uint64_t m_LastPass = 0;
    std::vector<PendingCall> m_Pending;
    std::unordered_map<int64_t, Item> m_Items;   // what it slept or shows, by instance id
    std::unordered_set<int64_t> m_Judged;        // seen and left alone (visible, no verdict, persistent room)
    size_t m_AsleepNow = 0, m_ShownNow = 0;
};

}   // namespace ForgePact
