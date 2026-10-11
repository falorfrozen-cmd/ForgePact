#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ForgePact::ChaosPillarTips {

// ---- Always show Chaos Pillar mods: the decision core (ForgePact #7) -------
//
// `pillartips 1` shows every on-screen Chaos Pillar's box of mods without the
// pillar being hovered. The game draws the box itself: each pillar keeps one
// Chaos_Pillar_Tooltip_obj, whose built-in `visible` is the hover gate (the
// pillar's mouse-enter event sets it, mouse-leave clears it), and whose Draw
// GUI draws only while it is true (RUNTIME_DATA_MODELS.md section 14.4,
// docs/chaos-pillar-tips-research.md). The mod sets that flag the way the
// mouse-enter event would, and clears it again for what it showed.
//
// The rule this header decides, once per frame of the HUD draw hook:
//   - off: nothing is read and nothing is written. Switching off returns one
//     hide per tooltip the core showed and still holds, and empties the set.
//   - on: each pillar of the latest census that is on screen, not activated,
//     whose `activated` could be read, and that has a tooltip gets a show
//     write every frame (the game may have hidden it since) and joins the
//     shown set. A shown tooltip whose pillar is now off screen, activated,
//     unreadable, without that tooltip or gone from the census gets one hide
//     write and leaves the set.
//   - a tooltip the core never showed is never written: an activated pillar
//     stays the game's, shown on hover as before.
//   - the census (each pillar's id, x, y, `activated`, tooltip) is taken at
//     most once every kCensusFrames frames, and on the first frame in a new
//     room; a room change empties the shown set without writing, since those
//     instances are gone with the room.
//   - on screen: inside the view grown by kViewMargin of its size on every
//     side, edges included. A view of zero (or unread) width or height
//     decides nothing that frame: no write, and the shown set is kept.
//
// It is game-independent by contract - instance ids as integers, positions
// and the view as numbers, never an instance or an RValue - so
// tests/chaos_pillar_tips_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp reads the room, the census and the view by name, checks
// instance_exists on each tooltip right before it writes `visible`, and
// reports a write it could not make to NoteWriteRefused.

// Frames between two censuses, at most.
inline constexpr unsigned long long kCensusFrames = 30;
// How far past each edge of the view a pillar still counts as on screen, as a
// fraction of the view's width (left and right) or height (top and bottom).
inline constexpr double kViewMargin = 0.25;

// The view in room coordinates: view camera 0's x, y, width and height.
struct View {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
};

// Whether a view can decide anything: a positive width and height. A NaN
// fails both comparisons, so an unread view decides nothing too.
inline bool ViewUsable(const View& v)
{
    return v.w > 0 && v.h > 0;
}

// The on-screen test, shared with the research probe: (x, y) inside the view
// grown by kViewMargin on every side, edges inclusive. False for a view that
// is not usable.
inline bool OnScreen(const View& v, double x, double y)
{
    if (!ViewUsable(v)) return false;
    const double mx = kViewMargin * v.w;
    const double my = kViewMargin * v.h;
    return x >= v.x - mx && x <= v.x + v.w + mx && y >= v.y - my && y <= v.y + v.h + my;
}

// A pillar's `activated` as the adapter read it.
enum class Activation : int { Inactive, Activated, Unreadable };

// One Chaos_Pillar_obj as the census reads it.
struct Pillar {
    std::int64_t id = 0;
    double x = 0;
    double y = 0;
    Activation activation = Activation::Unreadable;
    // Its chaosPillarTooltip, when that holds an instance (the game stores
    // noone before the roll and -1 after Destroy; both are "none").
    std::optional<std::int64_t> tooltip;
};

// One write the adapter carries out: the tooltip's `visible` set to true (a
// show) or false (a hide).
struct Write {
    std::int64_t tooltip = 0;
    bool visible = false;
    bool operator==(const Write& o) const { return tooltip == o.tooltip && visible == o.visible; }
};

struct Counters {
    long long pillars = 0;     // pillars in the latest census
    long long onscreen = 0;    // of those, on screen in the latest decided frame
    long long activated = 0;   // pillars of the latest census reading as activated
    long long unreadable = 0;  // pillars of the latest census whose `activated` could not be read
    long long notooltip = 0;   // pillars of the latest census without a tooltip
    long long showWrites = 0;  // show writes handed to the adapter
    long long hideWrites = 0;  // hide writes handed to the adapter
    long long ticks = 0;       // frames the core ran (BeginFrame while active)
    long long writesRefused = 0; // writes the adapter could not make (instance gone); not retried
};

class Core {
public:
    bool Enabled() const { return m_Enabled; }

    // Whether the adapter's tick has anything to do. False while off, since
    // switching off empties the shown set: the adapter then returns before
    // any builtin call.
    bool Active() const { return m_Enabled || !m_Shown.empty(); }

    // `pillartips 1|0`. No hook is installed, so switching on only sets the
    // switch; the next frame takes a census. Switching off returns one hide
    // per tooltip still in the shown set, counted, and empties it.
    std::vector<Write> SetEnabled(bool on)
    {
        std::vector<Write> writes;
        if (on == m_Enabled) return writes;
        m_Enabled = on;
        m_HaveCensus = false;
        if (!on) {
            for (std::int64_t tip : m_Shown) writes.push_back({ tip, false });
            m_Stats.hideWrites += static_cast<long long>(writes.size());
            m_Shown.clear();
            m_Census.clear();
        }
        return writes;
    }

    // First call of a frame, with the current room. Counts the tick, empties
    // the shown set without writing on a room change, and answers whether the
    // adapter takes a census this frame. Nothing while inactive.
    bool BeginFrame(std::int64_t room)
    {
        if (!Active()) return false;
        ++m_Stats.ticks;
        ++m_Frame;
        if (!m_Room || *m_Room != room) {
            m_Room = room;
            m_Shown.clear();
            m_Census.clear();
            m_HaveCensus = false;
        }
        return !m_HaveCensus || m_Frame - m_CensusFrame >= kCensusFrames;
    }

    // This frame's census, when BeginFrame asked for one. A census asked for
    // and not taken stays due; the latest one taken keeps deciding.
    void TakeCensus(std::vector<Pillar> pillars)
    {
        m_Census = std::move(pillars);
        m_HaveCensus = true;
        m_CensusFrame = m_Frame;
        m_Stats.pillars = static_cast<long long>(m_Census.size());
        m_Stats.activated = m_Stats.unreadable = m_Stats.notooltip = 0;
        for (const Pillar& p : m_Census) {
            if (p.activation == Activation::Activated) ++m_Stats.activated;
            if (p.activation == Activation::Unreadable) ++m_Stats.unreadable;
            if (!p.tooltip) ++m_Stats.notooltip;
        }
    }

    // This frame's writes, from the latest census and this frame's view: a
    // show for each tooltip that qualifies (once, in census order), then one
    // hide for each shown tooltip that no longer does.
    std::vector<Write> Decide(const View& view)
    {
        std::vector<Write> writes;
        if (!m_Enabled || !ViewUsable(view)) return writes;
        std::set<std::int64_t> now;
        long long onscreen = 0;
        for (const Pillar& p : m_Census) {
            if (!OnScreen(view, p.x, p.y)) continue;
            ++onscreen;
            if (p.activation != Activation::Inactive || !p.tooltip) continue;
            if (now.insert(*p.tooltip).second) writes.push_back({ *p.tooltip, true });
        }
        m_Stats.onscreen = onscreen;
        m_Stats.showWrites += static_cast<long long>(writes.size());
        for (std::int64_t tip : m_Shown) {
            if (now.count(tip)) continue;
            writes.push_back({ tip, false });
            ++m_Stats.hideWrites;
        }
        m_Shown = std::move(now);
        return writes;
    }

    // A write the adapter could not make (its tooltip no longer exists).
    void NoteWriteRefused() { ++m_Stats.writesRefused; }

    const Counters& Stats() const { return m_Stats; }
    std::size_t Shown() const { return m_Shown.size(); }
    bool IsShown(std::int64_t tooltip) const { return m_Shown.count(tooltip) != 0; }

    // `pillartips 1|0|stat`; hudCalls is the adapter's HUD hook call count.
    std::string StatLine(long long hudCalls) const
    {
        std::string s = m_Enabled ? "pillartips: on" : "pillartips: off";
        s += " pillars=" + std::to_string(m_Stats.pillars);
        s += " onscreen=" + std::to_string(m_Stats.onscreen);
        s += " shown=" + std::to_string(m_Shown.size());
        s += " activated=" + std::to_string(m_Stats.activated);
        s += " unreadable=" + std::to_string(m_Stats.unreadable);
        s += " notooltip=" + std::to_string(m_Stats.notooltip);
        s += " showWrites=" + std::to_string(m_Stats.showWrites);
        s += " hideWrites=" + std::to_string(m_Stats.hideWrites);
        s += " ticks=" + std::to_string(m_Stats.ticks);
        s += " hudCalls=" + std::to_string(hudCalls);
        return s;
    }

private:
    bool m_Enabled = false;
    Counters m_Stats;
    std::set<std::int64_t> m_Shown;   // tooltips the core showed and has not hidden
    std::vector<Pillar> m_Census;     // the latest census
    bool m_HaveCensus = false;
    std::optional<std::int64_t> m_Room; // the room of the last frame, none before the first
    unsigned long long m_Frame = 0;       // frames the core ran
    unsigned long long m_CensusFrame = 0; // m_Frame when the latest census was taken
};

} // namespace ForgePact::ChaosPillarTips
