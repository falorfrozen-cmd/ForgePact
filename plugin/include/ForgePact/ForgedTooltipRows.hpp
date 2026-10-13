#pragma once

#include <cstddef>

namespace ForgePact::ForgedTooltipRows {

// ---- where a forged item's gold rows anchor in the tooltip pass -------------
//
// ModuleMain.cpp draws a forged item's own rows (fp_affix, Headhunter's line,
// a flight charm's `Flight` and the rest) inside its DrawInventoryStatsNew
// detour: at the anchor call it draws the rows at the call's y, moves the
// game's own row down by their height and adds that height to the return.
// The tooltip pass calls DrawInventoryStatsNew once per stat line it knows,
// whether or not the item has that stat (RDM § 16.6, measured), and adds each
// return to its y cursor.
//
// The rows anchor at the first call of the pass that draws a stat row (format
// 2 or 3) and whose stat the item has. An item with no stats at all (its
// itemStatStruct missing or holding no names, Pocket Elf) has no such call,
// so its rows would never draw: it anchors at its first format-2/3 call
// instead. The adapter reads "no stats" once a pass, where it arms the rows,
// and reads a call's stat only when NeedsStatRead says the answer matters.
// An item that has stats anchors exactly where it did before this rule.
//
// Booleans and numbers only, so tests/flight_charms_harness.cpp compiles it
// whole (the `tooltip_rows` scenarios).

// The rows are pending for a pass when there is at least one to draw.
constexpr bool Pending(std::size_t rowCount) { return rowCount > 0; }

// The formats of DrawInventoryStatsNew's stat rows, where the rows may anchor.
constexpr bool IsAnchorFormat(double format) { return format == 2.0 || format == 3.0; }

// Whether the call's stat has to be read: only while the rows are pending, at
// a row format, on an item that has stats.
constexpr bool NeedsStatRead(bool pending, double format, bool itemHasNoStats)
{
    return pending && IsAnchorFormat(format) && !itemHasNoStats;
}

// Whether the rows anchor at this call. The caller ends the pending state when
// it anchors, so a pass anchors at most once.
constexpr bool AnchorsHere(bool pending, double format, bool statPresent, bool itemHasNoStats)
{
    return pending && IsAnchorFormat(format) && (statPresent || itemHasNoStats);
}

} // namespace ForgePact::ForgedTooltipRows
