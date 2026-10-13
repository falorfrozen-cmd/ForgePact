// Behavioral regression harness for the HS Offline Tracker sensor's core
// (TrackerMod.hpp, `tracker 1|0|stat`, ForgePact #185).
//
// The Python runner injects the REAL ForgePact::TrackerMod header below. No
// game is touched: a counter call is what the adapter in ModuleMain.cpp reads
// off its arguments (each one's kind and value, as TrackerMod::Num), a ground
// item is the rarity, type, id, weapon type and name the adapter read off it
// plus whether its item struct is in the creation window, and a room is the
// name the `room` builtin's room_get_name answered.
//
// Baseline: with the switch off nothing is counted, decided, remembered or
// written, and no install is asked for. Target: the producer's adaptive
// counter rules (GoldLogAdd: argc 1 to 4, the last positive integral
// argument; ExperienceUpdate: argc 2, argument 0, not counted when the call
// answered an explicit false; EnemyAddStatistics: argc 1, a non-negative
// integral id, one kill per call), with saturating sums; the drop decision
// order (off, the creation guard, the tracked rarities 4/6/7/9/10, a
// duplicate, unread fields), Mythic 5 and Common 1 untracked; arming
// (not-armed, waiting-for-character, installed; one install per session);
// room changes and the Satanic zone code; every record kind's envelope and a
// sequence rising by one, with names escaped; one append per 30 frames and a
// heartbeat every 10 s of the frame clock; capped buffer and file that count
// what they drop; the stat line's keys.
//
// The negative control: built with FORGEPACT_TRACKER_CREATION_GUARD=0, the
// same bag drop is recorded (control/guard_off_records_the_bag_drop), so the
// target scenario that holds it is measuring the guard and nothing else.
//
// argv[1], when given, is a directory: the harness writes the journal of the
// record-kinds scenario there through the core's own Win32 writer, and prints
// `FILE <path>`, so the Python runner parses every line as JSON.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// PRODUCTION_TRACKER_MOD

using ForgePact::TrackerMod;
using Num = TrackerMod::Num;
using NumKind = TrackerMod::NumKind;
using Verdict = TrackerMod::Verdict;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static Num R(double v) { return { NumKind::Real, v, 0 }; }
static Num I32(std::int64_t v) { return { NumKind::Int32, 0.0, v }; }
static Num I64(std::int64_t v) { return { NumKind::Int64, 0.0, v }; }
static Num Other() { return { NumKind::Unsupported, 0.0, 0 }; }

static std::string opt(const std::optional<std::int64_t>& v) { return v ? std::to_string(*v) : std::string("none"); }

static std::size_t count(const std::string& hay, const std::string& needle)
{
    std::size_t n = 0;
    for (std::size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + 1)) ++n;
    return n;
}

static std::vector<std::string> lines(const std::string& batch)
{
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < batch.size()) {
        const std::size_t nl = batch.find('\n', start);
        if (nl == std::string::npos) { out.push_back(batch.substr(start) + "<no newline>"); break; }
        out.push_back(batch.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

// A switched-on, installed core with a fixed identity and clock.
static void on(TrackerMod& m)
{
    m.SetIdentity(4242, "forgepact-test");
    m.SetEnabled(true);
    m.BeginFrame(1700000000000ULL);
}

static TrackerMod::Drop drop(int rarity, std::int64_t type = 3, std::int64_t id = 17, std::int64_t weapon = 0,
                             const std::string& name = "Astral Covenant")
{
    TrackerMod::Drop d;
    d.rarity = rarity;
    d.itemType = type;
    d.itemId = id;
    d.weaponType = weapon;
    d.name = name;
    return d;
}

static TrackerMod::ItemKey key(std::uint64_t v) { return { TrackerMod::kKeyStruct, v }; }

// A ground item at the end of its frame: built through CreateItemNew this
// frame (noted) unless `built` is false, then decided.
static Verdict see(TrackerMod& m, const TrackerMod::Drop& d, std::uint64_t k, bool built, const std::string& identity)
{
    if (built) m.NoteCreated(key(k), true);
    const Verdict v = m.DecideDrop(d, m.RecentlyCreated(key(k)), identity);
    m.AgeCreationWindow();
    return v;
}

int main(int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "";

#if FORGEPACT_TRACKER_CREATION_GUARD == 0
    // ---- the negative control: the guard compiled out --------------------
    {
        TrackerMod m;
        on(m);
        // The bag drop: an existing struct put on the ground, never noted.
        const Verdict v = see(m, drop(9), 77, false, "item:3:bag");
        check("control/guard_off_records_the_bag_drop",
              !TrackerMod::kCreationGuard && v == Verdict::Recorded && m.Stats().dropsWritten == 1
                  && m.Stats().heldBagDrop == 0,
              TrackerMod::VerdictName(v));
    }
    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
#else
    check("control/guard_is_on_in_the_shipped_build", TrackerMod::kCreationGuard);

    // ---- decoding ------------------------------------------------------------
    {
        check("decode/real_integral_accepted",
              TrackerMod::DecodePositive(R(1250.0)) == 1250 && TrackerMod::DecodeNonNegative(R(0.0)) == 0);
        check("decode/real_fraction_refused",
              !TrackerMod::DecodePositive(R(12.5)) && !TrackerMod::DecodeNonNegative(R(0.25)));
        check("decode/real_nan_and_infinity_refused",
              !TrackerMod::DecodePositive(R(std::numeric_limits<double>::quiet_NaN()))
                  && !TrackerMod::DecodePositive(R(std::numeric_limits<double>::infinity()))
                  && !TrackerMod::DecodeNonNegative(R(-std::numeric_limits<double>::infinity())));
        check("decode/real_above_2p53_refused",
              TrackerMod::DecodePositive(R(9007199254740991.0)) == TrackerMod::kMaxExactInteger
                  && !TrackerMod::DecodePositive(R(9007199254740992.0)) && !TrackerMod::DecodePositive(R(1e300)));
        check("decode/int32_and_int64_accepted",
              TrackerMod::DecodePositive(I32(7)) == 7 && TrackerMod::DecodePositive(I64(5000000000LL)) == 5000000000LL);
        check("decode/zero_is_an_id_not_a_delta",
              !TrackerMod::DecodePositive(R(0.0)) && !TrackerMod::DecodePositive(I32(0))
                  && TrackerMod::DecodeNonNegative(I64(0)) == 0);
        check("decode/negative_refused",
              !TrackerMod::DecodePositive(R(-3.0)) && !TrackerMod::DecodePositive(I64(-1))
                  && !TrackerMod::DecodeNonNegative(I32(-1)) && !TrackerMod::DecodeNonNegative(R(-1.0)));
        check("decode/unsupported_kinds_refused",
              !TrackerMod::DecodePositive(Other()) && !TrackerMod::DecodeNonNegative(Other()));
    }

    // ---- the three capture rules ---------------------------------------------
    {
        const Num one[] = { R(250.0) };
        const Num two[] = { R(3.0), R(250.0) };
        const Num flag[] = { R(250.0), Other() };
        const Num four[] = { Other(), I32(40), R(0.5), I64(90) };
        const Num five[] = { R(1.0), R(2.0), R(3.0), R(4.0), R(5.0) };
        const Num none[] = { Other(), R(-5.0), R(0.0) };
        check("gold/argc_1_to_4_the_last_positive_argument",
              TrackerMod::CaptureGold(1, one) == 250 && TrackerMod::CaptureGold(2, two) == 250
                  && TrackerMod::CaptureGold(2, flag) == 250 && TrackerMod::CaptureGold(4, four) == 90,
              opt(TrackerMod::CaptureGold(2, two)) + " " + opt(TrackerMod::CaptureGold(4, four)));
        check("gold/argc_0_and_5_refused", !TrackerMod::CaptureGold(0, one) && !TrackerMod::CaptureGold(5, five));
        check("gold/no_positive_argument_refused", !TrackerMod::CaptureGold(3, none));

        const Num xp[] = { R(42000.0), R(1.0) };
        const Num xpWrong[] = { Other(), R(42000.0) };
        check("xp/argc_exactly_2_argument_0",
              TrackerMod::CaptureXp(2, xp) == 42000 && !TrackerMod::CaptureXp(1, xp) && !TrackerMod::CaptureXp(3, five)
                  && !TrackerMod::CaptureXp(2, xpWrong));

        const Num kill[] = { R(37.0) };
        const Num killZero[] = { I32(0) };
        const Num killBad[] = { R(37.5) };
        const Num killNeg[] = { R(-1.0) };
        check("kill/argc_exactly_1_a_non_negative_integral_id",
              TrackerMod::CaptureKill(1, kill) == 37 && TrackerMod::CaptureKill(1, killZero) == 0
                  && !TrackerMod::CaptureKill(2, two) && !TrackerMod::CaptureKill(0, kill)
                  && !TrackerMod::CaptureKill(1, killBad) && !TrackerMod::CaptureKill(1, killNeg)
                  && !TrackerMod::CaptureKill(1, flag + 1));
    }

    // ---- counting --------------------------------------------------------------
    {
        TrackerMod m;
        m.CountGold(500);
        m.CountXp(100, false);
        m.CountKill(3);
        check("baseline/off_counts_nothing",
              m.Stats().goldCalls == 0 && m.Stats().goldSum == 0 && m.Stats().xpSum == 0 && m.Stats().killsSum == 0);
    }
    {
        TrackerMod m;
        on(m);
        m.CountGold(500);
        m.CountGold(std::nullopt);
        m.CountGold(250);
        check("count/gold_sums_accepted_and_counts_every_call",
              m.Stats().goldCalls == 3 && m.Stats().goldSum == 750, m.StatLine(true, "none"));
        m.CountXp(1000, false);
        m.CountXp(1000, true);
        m.CountXp(std::nullopt, false);
        check("count/xp_explicit_false_is_a_rejection",
              m.Stats().xpCalls == 3 && m.Stats().xpSum == 1000 && m.Stats().xpRejected == 1, m.StatLine(true, "none"));
        m.CountKill(0);
        m.CountKill(12);
        m.CountKill(std::nullopt);
        check("count/one_kill_per_accepted_call", m.Stats().killCalls == 3 && m.Stats().killsSum == 2);
    }
    {
        TrackerMod m;
        on(m);
        const std::int64_t max = (std::numeric_limits<std::int64_t>::max)();
        m.CountGold(max - 5);
        m.CountGold(100);
        m.CountXp(max, false);
        m.CountXp(max, false);
        check("count/sums_saturate_instead_of_wrapping",
              m.Stats().goldSum == max && m.Stats().xpSum == max && TrackerMod::SaturatingAdd(max, 1) == max
                  && TrackerMod::SaturatingAdd(1, 2) == 3,
              m.StatLine(true, "none"));
    }

    // ---- arming ---------------------------------------------------------------
    {
        TrackerMod m;
        check("baseline/off_never_installs",
              !m.ShouldInstall(true, true, false) && std::string(m.InstallState(false)) == "not-armed");
        m.SetEnabled(true);
        check("arming/on_without_a_character_waits",
              !m.ShouldInstall(true, false, false) && !m.ShouldInstall(false, true, false)
                  && std::string(m.InstallState(false)) == "waiting-for-character");
        check("arming/a_character_installs_once",
              m.ShouldInstall(true, true, false) && !m.ShouldInstall(true, true, true)
                  && std::string(m.InstallState(true)) == "installed");
        m.SetEnabled(false);
        check("arming/off_after_an_install_still_says_installed",
              !m.ShouldInstall(true, true, false) && std::string(m.InstallState(true)) == "installed");
        check("arming/looks_for_the_player_every_60_frames",
              TrackerMod::LooksForPlayer(0) && !TrackerMod::LooksForPlayer(1) && !TrackerMod::LooksForPlayer(59)
                  && TrackerMod::LooksForPlayer(60) && TrackerMod::kInstallPollFrames == 60);
    }

    // ---- drops ----------------------------------------------------------------
    {
        TrackerMod m;
        const Verdict v = see(m, drop(9), 1, true, "item:3:a");
        check("baseline/off_records_no_drop",
              v == Verdict::Off && m.Stats().seen == 0 && m.Stats().created == 0 && m.Buffered() == 0);
    }
    {
        TrackerMod m;
        on(m);
        m.NoteRoom("Act_09_03");
        m.TakeBatch();
        bool all = true;
        std::string names;
        std::uint64_t k = 100;
        for (int r : { 4, 6, 7, 9, 10 }) {
            all = all && see(m, drop(r), ++k, true, "item:3:" + std::to_string(r)) == Verdict::Recorded;
            names += std::string(TrackerMod::RarityName(r)) + ",";
        }
        check("drops/the_five_tracked_rarities_are_recorded",
              all && m.Stats().dropsWritten == 5 && names == "Set,Satanic,Angelic,Heroic,Unholy,", names);
        const std::string batch = m.TakeBatch();
        check("drops/each_names_its_rarity",
              count(batch, "\"rarity\":\"Set\"") == 1 && count(batch, "\"rarity\":\"Satanic\"") == 1
                  && count(batch, "\"rarity\":\"Angelic\"") == 1 && count(batch, "\"rarity\":\"Heroic\"") == 1
                  && count(batch, "\"rarity\":\"Unholy\"") == 1,
              batch);
        check("drops/mythic_5_and_common_1_are_untracked",
              see(m, drop(5), ++k, true, "item:3:m") == Verdict::Untracked
                  && see(m, drop(1), ++k, true, "item:3:c") == Verdict::Untracked
                  && m.Stats().dropsUntracked == 2 && m.TakeBatch().empty());
        bool others = true;
        for (int r : { 0, 2, 3, 8, 11, 16, 99 }) others = others && see(m, drop(r), ++k, true, "item:x") == Verdict::Untracked;
        check("drops/every_other_code_is_untracked", others);
        check("drops/an_unread_rarity_is_untracked",
              see(m, drop(TrackerMod::kRarityUnread), ++k, true, "item:3:u") == Verdict::Untracked);
        check("drops/the_same_item_twice_is_a_duplicate",
              see(m, drop(9), ++k, true, "item:3:9") == Verdict::Duplicate && m.Stats().dropsDuplicate == 1);
        check("drops/unread_type_or_id_is_not_written",
              see(m, drop(9, -1), ++k, true, "item:3:t") == Verdict::Unread
                  && see(m, drop(9, 3, -1), ++k, true, "item:3:i") == Verdict::Unread && m.Stats().dropsUnread == 2
                  && m.TakeBatch().empty());
    }
    {
        // The creation guard: a bag drop (never noted) is held, before any
        // rarity is looked at; one noted the frame before still counts; two
        // frames back does not.
        TrackerMod m;
        on(m);
        const Verdict bag = see(m, drop(9), 77, false, "item:3:bag");
        const Verdict bagMythic = see(m, drop(5), 78, false, "item:3:bagm");
        check("drops/a_bag_drop_is_held_first",
              bag == Verdict::HeldBagDrop && bagMythic == Verdict::HeldBagDrop && m.Stats().heldBagDrop == 2
                  && m.Stats().dropsWritten == 0 && m.Stats().dropsUntracked == 0 && m.TakeBatch().empty());
        m.NoteCreated(key(80), true);
        m.AgeCreationWindow();
        const bool previous = m.DecideDrop(drop(7), m.RecentlyCreated(key(80)), "item:3:prev") == Verdict::Recorded;
        m.NoteCreated(key(81), true);
        m.AgeCreationWindow();
        m.AgeCreationWindow();
        const bool stale = m.DecideDrop(drop(7), m.RecentlyCreated(key(81)), "item:3:old") == Verdict::HeldBagDrop;
        check("drops/created_the_frame_before_counts_two_frames_back_does_not", previous && stale);
        m.NoteCreated(key(90), true);
        m.SetEnabled(false);
        m.SetEnabled(true);
        check("drops/off_clears_the_creation_window", !m.RecentlyCreated(key(90)));
        TrackerMod::ItemKey refKey{ TrackerMod::kKeyReference, 80 };
        m.NoteCreated(key(80), true);
        check("drops/the_key_kind_is_part_of_the_key", !m.RecentlyCreated(refKey) && m.RecentlyCreated(key(80)));
    }
    {
        TrackerMod m;
        on(m);
        for (std::uint64_t i = 0; i < TrackerMod::kCreationCap + 5; ++i) m.NoteCreated(key(1000 + i), true);
        check("drops/the_creation_window_is_capped",
              m.Stats().created == (long long)TrackerMod::kCreationCap && m.Stats().createOverflow == 5);
    }
    {
        // Armed at character select, not yet installed: the load builds items,
        // but only an installed tick ages the window, so a creation noted then
        // would still look "recent" at the first decision after the install
        // and let a restored ground item past the guard. Nothing is noted.
        TrackerMod m;
        on(m);
        for (std::uint64_t i = 0; i < 8; ++i) m.NoteCreated(key(500 + i), false);
        const bool noted = m.RecentlyCreated(key(500)) || m.Stats().created != 0 || m.Stats().createOverflow != 0;
        const Verdict first = m.DecideDrop(drop(9), m.RecentlyCreated(key(500)), "item:3:loaded");
        m.AgeCreationWindow();
        const Verdict fresh = see(m, drop(9), 600, true, "item:3:fresh");
        check("drops/a_creation_before_the_install_is_held_at_the_first_decision",
              !noted && first == Verdict::HeldBagDrop && fresh == Verdict::Recorded && m.Stats().created == 1,
              std::string(TrackerMod::VerdictName(first)) + " " + TrackerMod::VerdictName(fresh));
    }
    {
        // LootGroundInit calls are counted, and queued, only on and installed:
        // `init-calls=` tells a drop route that never ran from one that ran.
        TrackerMod m;
        const bool offRefused = !m.AcceptInit(true);
        on(m);
        const bool armedRefused = !m.AcceptInit(false);
        const bool installedTaken = m.AcceptInit(true) && m.AcceptInit(true);
        check("drops/init_calls_are_taken_and_counted_only_on_and_installed",
              offRefused && armedRefused && installedTaken && m.Stats().initCalls == 2,
              std::to_string(m.Stats().initCalls));
    }
    {
        TrackerMod m;
        check("drops/identity_rules_match_loot_announcements",
              TrackerMod::Identity(5, "3", "123.5") == "item:3:123.5" && TrackerMod::Identity(5, "3", "0") == "ground:5:0"
                  && TrackerMod::Identifiable(false, "3", "123.5") && !TrackerMod::Identifiable(true, "", "123.5")
                  && TrackerMod::Identifiable(true, "", "") && !TrackerMod::Identifiable(false, "3", "undefined"));
    }
    {
        TrackerMod m;
        on(m);
        m.NoteRoom("Act_02_01");
        m.TakeBatch();
        see(m, drop(10, 3, 412, 7, "Doom \"Blade\""), 5, true, "item:3:x");
        see(m, drop(4, 1, 9, 0, ""), 6, true, "item:1:y");
        const std::vector<std::string> l = lines(m.TakeBatch());
        const std::string first = l.size() > 0 ? l[0] : "";
        const std::string second = l.size() > 1 ? l[1] : "";
        const std::string id = "\"event_id\":\"fp-4242-" + std::to_string(m.Sequence() - 1) + "\"";
        check("drops/ground_drop_shape",
              l.size() == 2 && first.find("\"kind\":\"ground_drop\"") != std::string::npos && first.find(id) != std::string::npos
                  && first.find("\"source\":\"game_ground_item_create\"") != std::string::npos
                  && first.find("\"room\":\"Act_02_01\"") != std::string::npos
                  && first.find("\"item\":{\"name\":\"Doom \\\"Blade\\\"\",\"rarity\":\"Unholy\",\"item_type\":3,\"item_id\":412,"
                                "\"weapon_type\":7,\"amount\":1}}") != std::string::npos,
              first);
        check("drops/weapon_type_only_above_0_and_no_empty_name",
              second.find("\"item\":{\"rarity\":\"Set\",\"item_type\":1,\"item_id\":9,\"amount\":1}}") != std::string::npos,
              second);
    }

    // ---- the room and the Satanic zone ------------------------------------------
    {
        TrackerMod m;
        check("baseline/off_notes_no_room", !m.NoteRoom("Act_01_01") && m.Buffered() == 0);
        on(m);
        const bool first = m.NoteRoom("Town_01");
        const bool same = m.NoteRoom("Town_01");
        const bool empty = m.NoteRoom("");
        const bool changed = m.NoteRoom("Act_09_03");
        const std::vector<std::string> l = lines(m.TakeBatch());
        check("room/the_first_room_and_each_change_are_recorded",
              first && !same && !empty && changed && l.size() == 2 && l[0].find("\"room\":\"Town_01\"") != std::string::npos
                  && l[1].find("\"kind\":\"room\",") != std::string::npos && l[1].find("\"room\":\"Act_09_03\"}") != std::string::npos
                  && m.Room() == "Act_09_03",
              l.empty() ? "" : l.back());
        check("zone/code_from_an_act_room",
              TrackerMod::SatanicZoneCode("Act_09_03") == "Satanic_9_3" && TrackerMod::SatanicZoneCode("Act_1_12") == "Satanic_1_12"
                  && TrackerMod::SatanicZoneCode("Town_01").empty() && TrackerMod::SatanicZoneCode("Act_x_1").empty()
                  && TrackerMod::IsActZoneRoomName("Act_01_02") && !TrackerMod::IsActZoneRoomName("Act_02_02_02"));
        m.RecordSatanicZone("Satanic_9_3", { 6, 14, 21 }, { 3, 12 }, true);
        const std::vector<std::string> z = lines(m.TakeBatch());
        check("zone/a_satanic_zone_and_a_vitals_record",
              z.size() == 2 && z[0].find("\"kind\":\"satanic_zone\"") != std::string::npos
                  && z[0].find("\"zone\":\"Satanic_9_3\",\"buffs\":[6,14,21],\"debuffs\":[3,12]}") != std::string::npos
                  && z[1].find("\"kind\":\"vitals\"") != std::string::npos && z[1].find("\"satanic_here\":true}") != std::string::npos,
              z.empty() ? "" : z[0]);
    }

    // ---- the journal: envelope, sequence, escaping -------------------------------
    {
        TrackerMod m;
        on(m);
        m.RecordBridgeStatus(true, "", "gold=both xp=both kills=both drops=both");
        m.NoteRoom("Act_09_03");
        m.RecordProgress("Sorak");
        m.RecordSatanicZone("Satanic_9_3", { 1 }, {}, false);
        m.NoteCreated(key(1), true);
        m.DecideDrop(drop(9), true, "item:3:z");
        m.CountGold(10);
        // on() ran frame 1; frames 2 to 30, 20 s later, end on a flush frame:
        // the session_delta and the heartbeat.
        for (int f = 0; f < 29; ++f) m.BeginFrame(1700000000000ULL + 20000ULL + (std::uint64_t)f);
        m.EndFrame();
        m.RecordBridgeStatus(false, "no route installed", "gold=none xp=none kills=none drops=none");
        const std::string batch = m.TakeBatch();
        const std::vector<std::string> l = lines(batch);
        bool envelopes = true, rising = true;
        for (std::size_t i = 0; i < l.size(); ++i) {
            const std::string head = "{\"protocol\":\"hs-offline-tracker/1\",\"v\":1,\"kind\":\"";
            envelopes = envelopes && l[i].rfind(head, 0) == 0 && l[i].find("\"process_id\":4242,\"build_id\":\"forgepact-test\"") != std::string::npos
                && l[i].back() == '}';
            rising = rising && l[i].find("\"sequence\":" + std::to_string(i + 1) + ",") != std::string::npos;
        }
        std::string kinds;
        for (const char* k : { "bridge_status", "room", "progress", "satanic_zone", "vitals", "ground_drop", "session_delta", "heartbeat" })
            if (count(batch, std::string("\"kind\":\"") + k + "\"") == 0) kinds += std::string(k) + " ";
        // Nine records: the eight kinds, and a second bridge_status (blocked).
        check("journal/every_kind_carries_the_envelope", envelopes && kinds.empty() && l.size() == 9, kinds + batch);
        check("journal/the_sequence_rises_by_one_from_1", rising && m.Sequence() == 9);
        check("journal/the_session_delta_names_its_source",
              count(batch, "\"kind\":\"session_delta\",") == 1
                  && count(batch, "\"gold\":10,\"xp\":0,\"kills\":0,\"source\":\"forgepact_tracker\"}") == 1);
        check("journal/the_bridge_status_says_ready_or_blocked_with_a_reason",
              count(batch, "\"state\":\"transport_ready\",\"detail\":\"gold=both xp=both kills=both drops=both\"}") == 1
                  && count(batch, "\"state\":\"blocked\",\"reason\":\"no route installed\",") == 1);
        check("journal/the_heartbeat_has_no_fields", count(batch, "\"build_id\":\"forgepact-test\"}") >= 1);
        check("journal/every_record_is_one_line", batch.back() == '\n' && count(batch, "\n") == l.size());
    }
    {
        TrackerMod m;
        on(m);
        m.RecordProgress("Ann \"the\" \\Brave\\\t\x01");
        m.RecordProgress("\xC3\x9Cnholy \xE2\x98\x83 \xF0\x9F\x92\x8E");
        m.RecordProgress(std::string("bad \xFF\xC3 end"));
        const std::vector<std::string> l = lines(m.TakeBatch());
        check("escape/quotes_backslashes_and_controls",
              l.size() == 3 && l[0].find("\"name\":\"Ann \\\"the\\\" \\\\Brave\\\\\\t\\u0001\"}") != std::string::npos,
              l.empty() ? "" : l[0]);
        check("escape/non_ascii_utf8_passes_through",
              l.size() == 3 && l[1].find("\"name\":\"\xC3\x9Cnholy \xE2\x98\x83 \xF0\x9F\x92\x8E\"}") != std::string::npos,
              l.size() > 1 ? l[1] : "");
        check("escape/invalid_utf8_becomes_the_replacement_character",
              l.size() == 3 && l[2].find("\"name\":\"bad \\ufffd\\ufffd end\"}") != std::string::npos, l.size() > 2 ? l[2] : "");
        m.RecordProgress("");
        check("journal/an_empty_name_writes_nothing", m.Buffered() == 0);
    }

    // ---- batching, the heartbeat and the caps ------------------------------------
    {
        TrackerMod m;
        m.SetIdentity(1, "b");
        m.SetEnabled(true);
        const std::uint64_t t0 = 1700000000000ULL;
        int appends = 0, offFrameAppends = 0;
        for (std::uint64_t f = 1; f <= 600; ++f) {
            m.BeginFrame(t0 + f * 100);
            m.NoteRoom(f % 2 ? "Act_01_01" : "Act_01_02");
            if (m.EndFrame()) {
                ++appends;
                if (f % TrackerMod::kFlushFrames != 0) ++offFrameAppends;
                m.TakeBatch();
                m.NoteAppend(true);
            }
        }
        check("batch/one_append_per_30_frames", appends == 20 && offFrameAppends == 0 && TrackerMod::kFlushFrames == 30,
              std::to_string(appends));
        check("batch/a_heartbeat_every_10_s_of_the_frame_clock", m.Stats().heartbeats == 5,
              std::to_string(m.Stats().heartbeats));
        check("batch/lines_count_what_was_appended", m.Stats().lines == 600 + 5, std::to_string(m.Stats().lines));
    }
    {
        // The clock stands still here, so no heartbeat is due.
        TrackerMod m;
        m.SetIdentity(1, "b");
        m.SetEnabled(true);
        const std::uint64_t t = 1700000000000ULL;
        bool early = false;
        for (int f = 1; f < 30; ++f) { m.BeginFrame(t); early = m.EndFrame() || early; }
        m.BeginFrame(t);   // frame 30
        check("batch/an_empty_flush_frame_appends_nothing", !early && !m.EndFrame() && m.Buffered() == 0);
        m.CountKill(1);
        for (int f = 31; f < 60; ++f) { m.BeginFrame(t); early = m.EndFrame() || early; }
        check("batch/a_nonzero_delta_waits_for_the_flush_frame", !early && m.Buffered() == 0);
        m.BeginFrame(t);   // frame 60
        check("batch/the_flush_frame_writes_it_once", m.EndFrame() && count(m.TakeBatch(), "\"kills\":1,") == 1);
        for (int f = 61; f <= 90; ++f) { m.BeginFrame(t); early = m.EndFrame() || early; }
        check("batch/no_second_delta_without_new_counts", !early);
    }
    {
        TrackerMod m;
        on(m);
        const std::string big(1000, 'x');
        long long accepted = 0;
        for (int i = 0; i < 400; ++i) {
            const std::size_t before = m.Buffered();
            m.RecordProgress(big);
            if (m.Buffered() > before) ++accepted;
        }
        check("cap/the_buffer_counts_and_drops_past_its_cap",
              m.Buffered() <= TrackerMod::kBufferCap && m.Stats().bufferFull == 400 - accepted && m.Stats().bufferFull > 0,
              std::to_string(m.Stats().bufferFull));
        const std::uint64_t sequence = m.Sequence();
        check("cap/a_dropped_record_takes_no_sequence", sequence == (std::uint64_t)accepted);
        m.TakeBatch();
        m.NoteAppend(true);
        m.RecordProgress("after");
        check("cap/the_buffer_takes_records_again_once_appended", m.Stats().bufferFull == 400 - accepted && m.Sequence() == sequence + 1);
    }
    {
        TrackerMod m;
        on(m);
        const std::string big(1000, 'y');
        std::uint64_t appended = 0;
        for (int round = 0; round < 1000 && m.Stats().fileFull == 0; ++round) {
            for (int i = 0; i < 100; ++i) m.RecordProgress(big);
            appended += m.TakeBatch().size();
            m.NoteAppend(true);
        }
        check("cap/the_file_counts_and_drops_past_its_cap",
              m.Stats().fileFull > 0 && appended <= TrackerMod::kFileCap && m.FileBytes() <= TrackerMod::kFileCap,
              std::to_string(appended));
    }
    {
        TrackerMod m;
        on(m);
        m.RecordProgress("lost");
        m.TakeBatch();
        m.NoteAppend(false);
        m.RecordProgress("next");
        check("cap/a_failed_append_is_counted_and_never_retried",
              m.Stats().writeErrors == 1 && m.Stats().lines == 0 && count(m.TakeBatch(), "lost") == 0);
    }
    {
        TrackerMod m;
        on(m);
        m.CountGold(7);
        m.RecordProgress("Sorak");
        const std::string drained = m.Drain();
        check("batch/drain_writes_the_pending_delta_and_everything_buffered",
              count(drained, "\"kind\":\"progress\"") == 1 && count(drained, "\"gold\":7,") == 1 && m.Buffered() == 0);
    }

    // ---- the stat line ------------------------------------------------------------
    {
        TrackerMod m;
        check("stat/fresh_off",
              m.StatLine(false, "none")
                  == "tracker: off install=not-armed gold=not-installed xp=not-installed kills=not-installed drops=not-installed "
                     "gold-calls=0 xp-calls=0 kill-calls=0 gold-sum=0 xp-sum=0 kills-sum=0 drops-written=0 held-bag-drop=0 "
                     "drops-untracked=0 drops-duplicate=0 drops-unread=0 drops-seen=0 init-calls=0 xp-rejected=0 created=0 create-overflow=0 heartbeats=0 "
                     "lines=0 write-errors=0 buffer-full=0 file-full=0 file=none",
              m.StatLine(false, "none"));
        on(m);
        TrackerMod::Routes routes;
        routes.gold = "both";
        routes.xp = "table-only";
        routes.kills = "none";
        routes.drops = TrackerMod::CombineRoutes("both", "table-only");
        m.SetRoutes(routes);
        m.CountGold(5);
        m.CountXp(9, false);
        m.CountKill(1);
        see(m, drop(9), 3, true, "item:3:s");
        see(m, drop(9), 4, false, "item:3:t");
        see(m, drop(5), 5, true, "item:3:u");
        m.AcceptInit(true);
        m.TakeBatch();
        m.NoteAppend(true);
        const std::string s = m.StatLine(true, "session-4242-1700000000000.ndjson");
        check("stat/the_keys_live_procedure_1_reads",
              s.rfind("tracker: on install=installed gold=both xp=table-only kills=none drops=table-only ", 0) == 0
                  && s.find(" gold-calls=1 xp-calls=1 kill-calls=1 gold-sum=5 xp-sum=9 kills-sum=1 drops-written=1 held-bag-drop=1 drops-untracked=1 ")
                         != std::string::npos
                  && s.find(" drops-seen=3 init-calls=1 ") != std::string::npos
                  && s.find(" lines=1 write-errors=0 ") != std::string::npos
                  && s.size() > 39 && s.substr(s.size() - 39) == " file=session-4242-1700000000000.ndjson",
              s);
        check("stat/route_combination",
              std::string(TrackerMod::CombineRoutes("both", "both")) == "both"
                  && std::string(TrackerMod::CombineRoutes("both", "none")) == "none"
                  && std::string(TrackerMod::CombineRoutes("table-only", "both")) == "table-only"
                  && std::string(TrackerMod::CombineRoutes("not-installed", "both")) == "not-installed"
                  && m.RouteSummary() == "gold=both xp=table-only kills=none drops=table-only");
        check("stat/verdict_names",
              std::string(TrackerMod::VerdictName(Verdict::Off)) == "off"
                  && std::string(TrackerMod::VerdictName(Verdict::Recorded)) == "recorded"
                  && std::string(TrackerMod::VerdictName(Verdict::HeldBagDrop)) == "held-bag-drop"
                  && std::string(TrackerMod::VerdictName(Verdict::Untracked)) == "untracked"
                  && std::string(TrackerMod::VerdictName(Verdict::Duplicate)) == "duplicate"
                  && std::string(TrackerMod::VerdictName(Verdict::Unread)) == "unread");
    }

    // ---- the writer: one session file, whole lines ---------------------------------
#ifdef _WIN32
    if (!outDir.empty()) {
        ForgePact::TrackerJournalFile file;
        TrackerMod m;
        m.SetIdentity(4242, "forgepact-test");
        m.SetEnabled(true);
        m.BeginFrame(1700000000000ULL);
        const bool opened = file.Open(outDir, 4242, 1700000000000ULL);
        m.RecordBridgeStatus(true, "", "gold=both xp=both kills=both drops=both");
        m.NoteRoom("Act_09_03");
        m.RecordProgress("Ann \"the\" \\Brave\\ \xC3\x9Cnholy \xFF");
        m.RecordSatanicZone("Satanic_9_3", { 6, 14 }, { 3 }, true);
        m.NoteCreated(key(1), true);
        m.DecideDrop(drop(10, 3, 412, 7, "Doom \"Blade\" \xE2\x98\x83"), true, "item:3:w");
        m.CountGold(10);
        m.CountXp(20, false);
        m.CountKill(1);
        for (int f = 1; f < 30; ++f) m.BeginFrame(1700000000000ULL + 11000ULL + (std::uint64_t)f);
        m.EndFrame();
        const std::string batch = m.TakeBatch();
        const bool appended = file.Append(batch);
        m.NoteAppend(appended);
        check("writer/opens_the_session_file_and_appends_whole_lines",
              opened && appended && std::string(file.Name()) == "session-4242-1700000000000.ndjson",
              file.Name());
        std::cout << "FILE " << outDir << "\\tracker\\" << file.Name() << std::endl;
    }
#endif

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
#endif
}
