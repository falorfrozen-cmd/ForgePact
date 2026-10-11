// Behavioral regression harness for the Always show Chaos Pillar mods core
// (ChaosPillarTips.hpp, `pillartips 1|0|stat`, ForgePact #7).
//
// The Python runner injects the REAL ForgePact::ChaosPillarTips header below.
// No game is touched: a frame is what the adapter in ModuleMain.cpp hands the
// core from the HUD draw hook - the room, a census of Chaos_Pillar_obj (each
// pillar's id, x, y, whether `activated` read true, false or not at all, and
// its chaosPillarTooltip or none) when the core asks for one, and view camera
// 0's x, y, width and height - and what comes back is the list of `visible`
// writes the adapter makes on tooltips.
//
// Baseline: with the switch off the core reads nothing (the adapter returns
// before any builtin call) and writes nothing, frame after frame, with
// pillars on screen; switching off writes exactly one hide per tooltip the
// core showed, then nothing.
//
// Target: switching on installs nothing and writes nothing until the next
// frame. On, each pillar on screen (the view grown by a quarter of its size
// on every side, edges included), not activated, with a readable `activated`
// and a tooltip gets a show write every frame and joins the shown set; one
// off screen, activated, unreadable or without a tooltip gets nothing. A
// shown tooltip whose pillar leaves the screen, is activated, becomes
// unreadable, loses its tooltip or leaves the census gets one hide and leaves
// the set; coming back shows it again. A tooltip the core never showed is
// never written. A view of zero width or height decides nothing. The census
// is taken on the first frame, then at most once every 30 frames, and at once
// in a new room, which empties the shown set without writing. The counters
// and the stat line say what happened.
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// PRODUCTION_CHAOS_PILLAR_TIPS

namespace Tips = ForgePact::ChaosPillarTips;
using Tips::Activation;
using Tips::Pillar;
using Tips::View;
using Tips::Write;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string show(const std::vector<Write>& writes)
{
    std::string s = "[";
    for (const Write& w : writes) s += (s.size() > 1 ? " " : "") + std::string(w.visible ? "show:" : "hide:") + std::to_string(w.tooltip);
    return s + "]";
}

static std::string both(const Tips::Core& c, const std::vector<Write>& writes)
{
    return show(writes) + " " + c.StatLine(0);
}

// A pillar at (x, y) with tooltip `tip`, not activated.
static Pillar pillar(std::int64_t id, double x, double y, std::int64_t tip,
                     Activation a = Activation::Inactive)
{
    Pillar p;
    p.id = id;
    p.x = x;
    p.y = y;
    p.activation = a;
    p.tooltip = tip;
    return p;
}

static Pillar noTooltip(std::int64_t id, double x, double y)
{
    Pillar p = pillar(id, x, y, 0);
    p.tooltip.reset();
    return p;
}

// A 1920x1080 view at the room's origin; (100, 100) is well inside it and
// (5000, 5000) well outside.
static const View kView{ 0, 0, 1920, 1080 };
static const std::int64_t kRoom = 7;

// One frame of the adapter's tick: it asks Active() first and returns at once
// when false (no builtin call: `reads` stays as it was), else BeginFrame with
// the room, a census when asked (counted in `reads`), then Decide with the view.
struct Frame {
    std::vector<Write> writes;
    bool ran = false;
    bool census = false;
};

static Frame frame(Tips::Core& c, const std::vector<Pillar>& pillars, const View& view = kView,
                   std::int64_t room = kRoom)
{
    Frame f;
    if (!c.Active()) return f;
    f.ran = true;
    f.census = c.BeginFrame(room);
    if (f.census) c.TakeCensus(pillars);
    f.writes = c.Decide(view);
    return f;
}

static bool zero(const Tips::Counters& s)
{
    return s.pillars == 0 && s.onscreen == 0 && s.activated == 0 && s.unreadable == 0 && s.notooltip == 0
        && s.showWrites == 0 && s.hideWrites == 0 && s.ticks == 0 && s.writesRefused == 0;
}

int main()
{
    const std::vector<Pillar> two{ pillar(1, 100, 100, 101), pillar(2, 300, 200, 102) };

    // ---- baseline: the switch off ------------------------------------------
    {
        Tips::Core c;
        check("baseline/starts_off", !c.Enabled() && !c.Active() && c.Shown() == 0 && zero(c.Stats()),
              c.StatLine(0));
        check("baseline/fresh_stat_line",
              c.StatLine(0) == "pillartips: off pillars=0 onscreen=0 shown=0 activated=0 unreadable=0 notooltip=0 "
                               "showWrites=0 hideWrites=0 ticks=0 hudCalls=0",
              c.StatLine(0));
    }
    {
        // Off reads nothing: the adapter's tick returns before its first
        // builtin call, frame after frame, with pillars on screen.
        Tips::Core c;
        bool ranAny = false;
        for (int i = 0; i < 100; ++i)
            if (frame(c, two).ran) ranAny = true;
        check("baseline/off_reads_nothing", !ranAny && !c.Active() && zero(c.Stats()), c.StatLine(0));
    }
    {
        // Off writes nothing, even when the core is driven directly.
        Tips::Core c;
        std::vector<Write> all;
        for (int i = 0; i < 100; ++i) {
            if (c.BeginFrame(kRoom)) c.TakeCensus(two);
            const auto w = c.Decide(kView);
            all.insert(all.end(), w.begin(), w.end());
        }
        check("baseline/off_writes_nothing", all.empty() && c.Shown() == 0 && zero(c.Stats()), both(c, all));
    }
    {
        // Switching off while already off writes nothing.
        Tips::Core c;
        const auto w = c.SetEnabled(false);
        check("baseline/off_with_nothing_shown_writes_nothing", w.empty() && !c.Active() && zero(c.Stats()),
              both(c, w));
    }
    {
        // Switching off writes one hide per tooltip the core showed, then
        // nothing: the game's own hover gate is back in charge.
        Tips::Core c;
        c.SetEnabled(true);
        frame(c, two);
        frame(c, two);
        const auto off = c.SetEnabled(false);
        std::set<std::int64_t> hidden;
        bool allHides = true;
        for (const Write& w : off) {
            hidden.insert(w.tooltip);
            if (w.visible) allHides = false;
        }
        bool after = true;
        for (int i = 0; i < 40; ++i)
            if (frame(c, two).ran) after = false;
        const auto again = c.SetEnabled(false);
        check("baseline/switch_off_hides_each_shown_once",
              off.size() == 2 && allHides && hidden == std::set<std::int64_t>{ 101, 102 } && c.Shown() == 0
                  && !c.Active() && after && again.empty() && c.Stats().hideWrites == 2,
              both(c, off));
    }

    // ---- target: the constants and the on-screen test -----------------------
    check("target/constants", Tips::kCensusFrames == 30 && Tips::kViewMargin == 0.25);
    {
        // The view (100, 200, 400, 300) grown by a quarter: x in [0, 600],
        // y in [125, 575], edges included.
        const View v{ 100, 200, 400, 300 };
        check("target/onscreen_edges_inclusive",
              Tips::OnScreen(v, 0, 125) && Tips::OnScreen(v, 600, 575) && Tips::OnScreen(v, 0, 575)
                  && Tips::OnScreen(v, 600, 125) && Tips::OnScreen(v, 300, 350));
        check("target/onscreen_just_outside",
              !Tips::OnScreen(v, -0.5, 350) && !Tips::OnScreen(v, 600.5, 350) && !Tips::OnScreen(v, 300, 124.5)
                  && !Tips::OnScreen(v, 300, 575.5));
        check("target/onscreen_margin_is_a_quarter_of_each_side",
              Tips::OnScreen(v, 50, 350) && !Tips::OnScreen(v, -50, 350) && Tips::OnScreen(v, 300, 150)
                  && !Tips::OnScreen(v, 300, 100));
        check("target/unusable_view_is_never_on_screen",
              !Tips::OnScreen(View{ 0, 0, 0, 1080 }, 0, 0) && !Tips::OnScreen(View{ 0, 0, 1920, 0 }, 0, 0)
                  && !Tips::OnScreen(View{ 0, 0, -1920, 1080 }, -100, 0) && !Tips::ViewUsable(View{ 0, 0, 0, 0 })
                  && Tips::ViewUsable(kView));
    }

    // ---- target: switching on ------------------------------------------------
    {
        Tips::Core c;
        const auto on = c.SetEnabled(true);
        check("target/switch_on_writes_nothing_until_a_frame",
              on.empty() && c.Enabled() && c.Active() && c.Stats().ticks == 0 && c.Shown() == 0, both(c, on));
        const Frame f = frame(c, two);
        check("target/first_frame_takes_a_census", f.ran && f.census && c.Stats().ticks == 1, c.StatLine(0));
        const auto again = c.SetEnabled(true);
        check("target/switch_on_twice_changes_nothing", again.empty() && c.Shown() == 2, both(c, again));
    }

    // ---- target: who is shown ------------------------------------------------
    {
        Tips::Core c;
        c.SetEnabled(true);
        const Frame f = frame(c, { pillar(1, 100, 100, 101) });
        check("target/onscreen_pillar_shown",
              f.writes == std::vector<Write>{ { 101, true } } && c.IsShown(101) && c.Shown() == 1
                  && c.Stats().showWrites == 1,
              both(c, f.writes));
        bool everyFrame = true;
        for (int i = 0; i < 10; ++i) {
            const Frame g = frame(c, { pillar(1, 100, 100, 101) });
            if (g.census || !(g.writes == std::vector<Write>{ { 101, true } })) everyFrame = false;
        }
        check("target/show_written_every_frame", everyFrame && c.Stats().showWrites == 11 && c.Stats().hideWrites == 0,
              c.StatLine(0));
    }
    {
        // One census of six pillars: only the first is shown.
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> mix{
            pillar(1, 100, 100, 101),
            pillar(2, 5000, 5000, 102),
            pillar(3, 200, 100, 103, Activation::Activated),
            pillar(4, 300, 100, 104, Activation::Unreadable),
            noTooltip(5, 400, 100),
            pillar(6, 500, 100, 101), // a second pillar naming the same tooltip: one write
        };
        const Frame f = frame(c, mix);
        check("target/only_onscreen_inactive_readable_with_tooltip_shown",
              f.writes == std::vector<Write>{ { 101, true } } && c.Shown() == 1, both(c, f.writes));
        check("target/offscreen_not_shown", !c.IsShown(102));
        check("target/activated_not_shown", !c.IsShown(103));
        check("target/unreadable_not_shown", !c.IsShown(104));
        check("target/census_counts",
              c.Stats().pillars == 6 && c.Stats().onscreen == 5 && c.Stats().activated == 1
                  && c.Stats().unreadable == 1 && c.Stats().notooltip == 1,
              c.StatLine(0));
    }

    // ---- target: who is hidden, once -----------------------------------------
    {
        // The view moves away: one hide, then nothing; it comes back: shown.
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> one{ pillar(1, 100, 100, 101) };
        frame(c, one);
        const View away{ 4000, 4000, 1920, 1080 };
        const Frame leave = frame(c, one, away);
        const Frame after = frame(c, one, away);
        check("target/leaves_screen_one_hide",
              leave.writes == std::vector<Write>{ { 101, false } } && after.writes.empty() && c.Shown() == 0
                  && c.Stats().hideWrites == 1 && c.Stats().onscreen == 0,
              both(c, leave.writes));
        const Frame back = frame(c, one);
        check("target/back_on_screen_shown_again", back.writes == std::vector<Write>{ { 101, true } } && c.IsShown(101),
              both(c, back.writes));
    }
    {
        // A change the next census sees: activated, unreadable, no tooltip,
        // gone. Each shown tooltip gets one hide, then nothing.
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> before{ pillar(1, 100, 100, 101), pillar(2, 200, 100, 102),
                                          pillar(3, 300, 100, 103), pillar(4, 400, 100, 104),
                                          pillar(5, 500, 100, 105) };
        frame(c, before);
        const std::vector<Pillar> later{ pillar(1, 100, 100, 101, Activation::Activated),
                                         pillar(2, 200, 100, 102, Activation::Unreadable), noTooltip(3, 300, 100),
                                         pillar(5, 500, 100, 105) };
        std::vector<Write> between;
        for (unsigned long long i = 1; i < Tips::kCensusFrames; ++i) {
            const Frame f = frame(c, later);
            for (const Write& w : f.writes)
                if (!w.visible) between.push_back(w);
        }
        const Frame census = frame(c, later);
        const Frame next = frame(c, later);
        std::set<std::int64_t> hidden;
        bool onlyHides = true;
        for (const Write& w : census.writes) {
            if (w.visible && w.tooltip != 105) onlyHides = false;
            if (!w.visible) hidden.insert(w.tooltip);
        }
        check("target/state_change_waits_for_the_census", between.empty() && census.census, show(between));
        check("target/activated_after_shown_one_hide", hidden.count(101) == 1, both(c, census.writes));
        check("target/unreadable_after_shown_one_hide", hidden.count(102) == 1, both(c, census.writes));
        check("target/tooltip_lost_one_hide", hidden.count(103) == 1, both(c, census.writes));
        check("target/gone_from_census_one_hide", hidden.count(104) == 1, both(c, census.writes));
        check("target/each_hide_once_then_nothing",
              onlyHides && hidden.size() == 4 && census.writes.size() == 5 && next.writes == std::vector<Write>{ { 105, true } }
                  && c.Shown() == 1 && c.IsShown(105) && c.Stats().hideWrites == 4,
              both(c, next.writes));
    }
    {
        // A tooltip the core never showed is never written: an activated
        // pillar on screen, an off-screen one, and switching off afterwards.
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> theirs{ pillar(1, 100, 100, 101, Activation::Activated), pillar(2, 5000, 5000, 102),
                                          pillar(3, 200, 100, 103, Activation::Unreadable) };
        std::vector<Write> all;
        for (int i = 0; i < 90; ++i) {
            const Frame f = frame(c, theirs);
            all.insert(all.end(), f.writes.begin(), f.writes.end());
        }
        const auto off = c.SetEnabled(false);
        all.insert(all.end(), off.begin(), off.end());
        check("target/never_touches_a_tooltip_it_did_not_show", all.empty() && c.Stats().showWrites == 0
                                                                     && c.Stats().hideWrites == 0,
              both(c, all));
    }

    // ---- target: a view that decides nothing ---------------------------------
    {
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> one{ pillar(1, 100, 100, 101) };
        frame(c, one);
        const Frame w0 = frame(c, one, View{ 0, 0, 0, 1080 });
        const Frame h0 = frame(c, one, View{ 0, 0, 1920, 0 });
        check("target/zero_view_decides_nothing",
              w0.ran && h0.ran && w0.writes.empty() && h0.writes.empty() && c.IsShown(101) && c.Shown() == 1
                  && c.Stats().hideWrites == 0,
              both(c, h0.writes));
        const Frame ok = frame(c, one);
        check("target/zero_view_then_a_real_one", ok.writes == std::vector<Write>{ { 101, true } }, both(c, ok.writes));
    }

    // ---- target: the census cadence and the room -----------------------------
    {
        Tips::Core c;
        c.SetEnabled(true);
        std::vector<int> censusFrames;
        for (int i = 1; i <= 91; ++i)
            if (frame(c, two).census) censusFrames.push_back(i);
        check("target/census_once_every_30_frames", censusFrames == std::vector<int>{ 1, 31, 61, 91 },
              std::to_string(censusFrames.size()));
    }
    {
        // A census the adapter could not take stays due.
        Tips::Core c;
        c.SetEnabled(true);
        const bool first = c.BeginFrame(kRoom);
        c.Decide(kView);
        const bool second = c.BeginFrame(kRoom);
        c.TakeCensus(two);
        c.Decide(kView);
        const bool third = c.BeginFrame(kRoom);
        check("target/census_due_until_taken", first && second && !third);
    }
    {
        // A new room: a census at once, and the shown set emptied without a
        // write (those instances went with the room).
        Tips::Core c;
        c.SetEnabled(true);
        frame(c, two);
        frame(c, two);
        const std::vector<Pillar> other{ pillar(11, 100, 100, 111) };
        const Frame f = frame(c, other, kView, kRoom + 1);
        check("target/new_room_census_at_once_no_hide",
              f.census && f.writes == std::vector<Write>{ { 111, true } } && c.Shown() == 1 && c.IsShown(111)
                  && c.Stats().hideWrites == 0,
              both(c, f.writes));
        const Frame empty = frame(c, {}, kView, kRoom + 2);
        check("target/room_without_pillars_writes_nothing",
              empty.census && empty.writes.empty() && c.Shown() == 0 && c.Stats().pillars == 0, both(c, empty.writes));
    }
    {
        // Switched off and on again: the first frame takes a fresh census.
        Tips::Core c;
        c.SetEnabled(true);
        frame(c, two);
        frame(c, two);
        c.SetEnabled(false);
        c.SetEnabled(true);
        const Frame f = frame(c, { pillar(1, 100, 100, 101) });
        check("target/switch_on_again_takes_a_census",
              f.census && f.writes == std::vector<Write>{ { 101, true } } && c.Shown() == 1, both(c, f.writes));
    }

    // ---- target: refused writes, and the stat line ---------------------------
    {
        Tips::Core c;
        c.SetEnabled(true);
        c.NoteWriteRefused();
        c.NoteWriteRefused();
        check("target/refused_write_counted", c.Stats().writesRefused == 2);
    }
    {
        Tips::Core c;
        c.SetEnabled(true);
        const std::vector<Pillar> mix{ pillar(1, 100, 100, 101), pillar(2, 5000, 5000, 102),
                                       pillar(3, 200, 100, 103, Activation::Activated),
                                       pillar(4, 300, 100, 104, Activation::Unreadable), noTooltip(5, 400, 100) };
        frame(c, mix);
        frame(c, mix);
        frame(c, mix, View{ 8000, 8000, 1920, 1080 }); // every pillar off screen
        check("target/stat_line",
              c.StatLine(42) == "pillartips: on pillars=5 onscreen=0 shown=0 activated=1 unreadable=1 notooltip=1 "
                                "showWrites=2 hideWrites=1 ticks=3 hudCalls=42",
              c.StatLine(42));
        c.SetEnabled(false);
        check("target/stat_line_after_off",
              c.StatLine(43) == "pillartips: off pillars=5 onscreen=0 shown=0 activated=1 unreadable=1 notooltip=1 "
                                "showWrites=2 hideWrites=1 ticks=3 hudCalls=43",
              c.StatLine(43));
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
