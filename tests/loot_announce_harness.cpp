// Behavioral regression harness for the Loot announcements mod's decision
// core (LootAnnounceMod.hpp, `lootann 1|0|stat`, ForgePact #17).
//
// The Python runner injects the REAL ForgePact::LootAnnounceMod header below.
// No game is touched: a ground item is what the adapter in ModuleMain.cpp
// reads off it and hands the core - the rarity code from itemInfoStruct["27"]
// (or kRarityUnread when that read failed), the ground instance id, the
// item's itemType and itemTimeStamp - plus whether its item struct is in the
// creation window: noted by the shared CreateItemNew hook in this frame or the
// one before.
//
// Baseline: with the switch off, a Heroic item (and every other rarity) is
// not announced, nothing is counted, remembered or noted as created. Target:
// with it on, a Heroic, Angelic or Unholy item the game has just built
// announces once; Satanic, Mythic, Common and the rest do not, nor does an
// unreadable rarity; an item whose struct was not built in this frame or the
// last (a bag drop, a re-drop after a pickup) does not; an item whose
// identity (itemType and a real itemTimeStamp, else the ground id) was
// already announced does not; switching off clears the creation window; the
// window and the memory are capped; the counters and the stat line say what
// happened. An identity the adapter could not read is not Identifiable.
// live2_replay replays Live procedure 2's steps 2-6 (aborted), the session
// whose bag drop the old LootGroundDrop window announced.
//
// The Chaos Tower's loot burst (ForgePact #227): a floor takes its fresh drops
// back into a list and puts the same structs down at its end, unbuilt.
// Baseline: an item built long before it lands is held, and off, a listed
// entry is not kept. Target: a recently built entry is kept and its landing
// announces once; a stale entry is not kept; the memory is forgotten on a
// shrink and on off, capped, and leaves rarity and identity deciding; the stat
// line names the four burst counters.
//
// The line's colours (ForgePact #228): the shipped sink is `chatcolour`, which
// puts "<character> found" in the chat line's sender part in the game's grey
// chat text and the item's name alone in its text part, in the online
// announcement's colour for the rarity. Baseline: the red line's text, as
// shipped before, is `<character> found <item>`. Target: Heroic 9764608,
// Angelic 10681599, Unholy 7224024, the sender and every other code 11974842,
// and no colour the core returns is 0 or the 6776816 that greys the text.
//
// The install (Known Limitations item 8: a hook installed at character select
// stalls the runner): the switch arms at launch, and the hooks go in only once
// setup is done and the local player resolves, once per session. Baseline:
// the rule before 2026-10-08, setup alone, written out as its inputs and
// output. Target: no player, no install and `waiting-for-character`; a player,
// one install and `installed`; switched on in game, at once; off, never.
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// PRODUCTION_LOOT_ANNOUNCE_MOD

using ForgePact::LootAnnounceMod;
using Verdict = LootAnnounceMod::Verdict;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string counts(const LootAnnounceMod& m)
{
    const auto& c = m.Stats();
    return "seen=" + std::to_string(c.seen) + " announced=" + std::to_string(c.announced)
        + " held-rarity=" + std::to_string(c.heldRarity) + " held-no-rarity=" + std::to_string(c.heldNoRarity)
        + " held-duplicate=" + std::to_string(c.heldDuplicate) + " held-bag-drop=" + std::to_string(c.heldBagDrop)
        + " sink-refused=" + std::to_string(c.sinkRefused) + " remembered=" + std::to_string(m.Remembered())
        + " created=" + std::to_string(c.created) + " create-overflow=" + std::to_string(c.createOverflow)
        + " burst-listed=" + std::to_string(c.burstListed) + " burst-fresh=" + std::to_string(c.burstFresh)
        + " burst-landed=" + std::to_string(c.burstLanded) + " burst-overflow=" + std::to_string(c.burstOverflow)
        + " burst-kept=" + std::to_string(m.BurstKept());
}

// One ground item as the adapter sees it at the end of the drop's frame: its
// rarity, its ground instance id, its item's itemType and itemTimeStamp as
// text, and the key of the item struct it holds (the struct's own pointer or
// reference value, which the adapter compares and never follows; 0 here means
// "the ground id").
struct Drop {
    int rarity;
    int64_t id;
    std::string stamp;
    std::string type = "4";
    uint64_t key = 0;
};

static LootAnnounceMod::ItemKey keyOf(const Drop& d)
{
    return { LootAnnounceMod::kKeyStruct, d.key ? d.key : (uint64_t)d.id };
}

// The end-of-frame decision for an item that is already on the ground: the
// guard asks the creation window and the burst memory for its key.
static Verdict see(LootAnnounceMod& m, const Drop& d)
{
    return m.Decide(d.rarity, m.RecentlyCreated(keyOf(d)), d.id, d.type, d.stamp, m.InBurstMemory(keyOf(d)));
}

// The adapter's burst read: a new entry of the Controller's loot burst list,
// noted after the tick's decisions and before the window ages.
static void listed(LootAnnounceMod& m, const Drop& d)
{
    m.NoteListed(keyOf(d));
}

// The game builds the item through CreateItemNew and puts it on the ground in
// the same frame: the shared hook notes its key, then the tick decides.
static Verdict fresh(LootAnnounceMod& m, const Drop& d)
{
    m.NoteCreated(keyOf(d));
    return see(m, d);
}

// One LootAnnounceTick's end: the creation window ages after the batch.
static void tick(LootAnnounceMod& m, int n = 1)
{
    for (int i = 0; i < n; ++i) m.AgeCreationWindow();
}

// The rarity codes of RUNTIME_DATA_MODELS.md section 16.4 (4 and 8 were
// never observed; they are offered too, so "not listed" is tested, not
// assumed).
enum Rarity : int { Common = 1, Superior = 2, Rare = 3, Code4 = 4, Mythic = 5, Satanic = 6, Angelic = 7,
                    Code8 = 8, Heroic = 9, Unholy = 10 };

int main()
{
    // ---- the table: what is announced, named --------------------------------
    {
        const std::set<int> set(LootAnnounceMod::kAnnouncedRarities.begin(), LootAnnounceMod::kAnnouncedRarities.end());
        check("table/announced_rarities_are_heroic_angelic_unholy",
              set == std::set<int>{ 9, 7, 10 } && LootAnnounceMod::kAnnouncedRarities.size() == 3
                  && LootAnnounceMod::kHeroic == 9 && LootAnnounceMod::kAngelic == 7 && LootAnnounceMod::kUnholy == 10);
        bool exact = true;
        for (int code = -2; code <= 12; ++code) {
            const bool want = code == 9 || code == 7 || code == 10;
            if (LootAnnounceMod::IsAnnouncedRarity(code) != want) exact = false;
        }
        check("table/is_announced_rarity_exact", exact);
        check("table/unread_is_not_a_rarity", !LootAnnounceMod::IsAnnouncedRarity(LootAnnounceMod::kRarityUnread)
                                                  && LootAnnounceMod::kRarityUnread < 0);
        // ForgePact #228: the shipped sink is `chatcolour`, the fifth body;
        // the other four keep their names (and `lootannprobe try` its 1..5).
        check("table/shipped_sink_and_names",
              LootAnnounceMod::kShippedSink == LootAnnounceMod::Sink::ChatColour
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::Method)) == "method"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::NetSend)) == "netsend"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::ChatAdd)) == "chatadd"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::Server)) == "server"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::ChatColour)) == "chatcolour",
              LootAnnounceMod::SinkName(LootAnnounceMod::kShippedSink));
    }

    // ---- the line: its two parts and their colours (ForgePact #228) ----------
    // The game's chat line carries two text colours, the sender's and the
    // text's (a static reading), so the character's name and "found" go in the
    // sender part, in the game's grey chat text, and the item's name alone in
    // the text part, in the colour the online announcement gives its rarity.
    // Colours are GameMaker's blue-green-red integers.
    {
        // Baseline: the red `SERVER:` line's text as the mod shipped it before
        // #228 (ChatAddServerMessage, both colours red), written out here.
        const std::string shippedBefore = std::string("Sorak") + " found " + "Headhunter";
        check("baseline/server_text_is_character_found_item",
              shippedBefore == "Sorak found Headhunter"
                  && LootAnnounceMod::ServerText("Sorak", "Headhunter") == shippedBefore
                  && LootAnnounceMod::ServerText("You", "Headhunter") == "You found Headhunter",
              LootAnnounceMod::ServerText("Sorak", "Headhunter"));

        check("colour/heroic", LootAnnounceMod::AnnouncementColour(Heroic) == 9764608,   // #00FF94
              std::to_string(LootAnnounceMod::AnnouncementColour(Heroic)));
        check("colour/angelic", LootAnnounceMod::AnnouncementColour(Angelic) == 10681599, // #FFFCA2
              std::to_string(LootAnnounceMod::AnnouncementColour(Angelic)));
        check("colour/unholy", LootAnnounceMod::AnnouncementColour(Unholy) == 7224024,   // #D83A6E
              std::to_string(LootAnnounceMod::AnnouncementColour(Unholy)));

        // The sender part is the game's grey chat text (#BAB8B6) whatever the
        // rarity.
        bool neutralSender = LootAnnounceMod::kNeutralColour == 11974842;
        for (int code : { (int)Heroic, (int)Angelic, (int)Unholy, (int)Satanic, LootAnnounceMod::kRarityUnread })
            if (LootAnnounceMod::Line("Sorak", "Headhunter", code).senderColour != 11974842) neutralSender = false;
        check("colour/neutral_sender", neutralSender, std::to_string(LootAnnounceMod::kNeutralColour));

        // A rarity that is not announced (the adapter never asks, but the
        // table answers) gets the neutral grey, never the line's own colour.
        bool heldNeutral = true;
        std::string heldDetail;
        for (int code : { (int)Satanic, (int)Mythic, (int)Common, LootAnnounceMod::kRarityUnread }) {
            const int c = LootAnnounceMod::AnnouncementColour(code);
            heldDetail += std::to_string(code) + "->" + std::to_string(c) + " ";
            if (c != 11974842 || LootAnnounceMod::Line("Sorak", "x", code).textColour != 11974842) heldNeutral = false;
        }
        check("colour/held_rarity_is_neutral", heldNeutral, heldDetail);

        // The draw greys the text when the sender colour is exactly 6776816
        // (#F06767); 0 is black, which the measured shape-8 call passed. No
        // colour the core returns is either, for any code.
        bool neverTrigger = true;
        for (int code = -2; code <= 12; ++code) {
            const LootAnnounceMod::ChatLine l = LootAnnounceMod::Line("Sorak", "Headhunter", code);
            for (int c : { l.senderColour, l.textColour, LootAnnounceMod::AnnouncementColour(code) })
                if (c == 6776816 || c == 0) neverTrigger = false;
        }
        check("colour/not_the_grey_text_trigger", neverTrigger);

        const LootAnnounceMod::ChatLine heroic = LootAnnounceMod::Line("Sorak", "Headhunter", Heroic);
        const LootAnnounceMod::ChatLine nameless = LootAnnounceMod::Line("You", "Headhunter", Heroic);
        check("line/sender_is_character_found", heroic.sender == "Sorak found" && nameless.sender == "You found",
              heroic.sender + " | " + nameless.sender);
        const LootAnnounceMod::ChatLine angelic = LootAnnounceMod::Line("Sorak", "Wings of the Seraph", Angelic);
        check("line/body_is_item_name",
              heroic.text == "Headhunter" && heroic.textColour == 9764608
                  && angelic.text == "Wings of the Seraph" && angelic.textColour == 10681599,
              heroic.text + " | " + angelic.text);
    }

    // ---- the install: armed at launch, hooked once a character exists --------
    // The adapter feeds the core g_Setup, whether HhResolveLocalPlayer found
    // the local player, and g_LaInstallTried; the tick looks for the player
    // only on the frames LooksForPlayer picks while the switch is on.
    {
        // Baseline: the rule before 2026-10-08, as the adapter wrote it (the
        // switch on, setup done, not tried yet). A switch already on as the
        // game started installed on the first frame after setup, at character
        // select, with no character loaded: there was no player input at all.
        const auto oldRule = [](bool on, bool setupDone, bool installTried) { return on && setupDone && !installTried; };
        check("baseline/setup_alone_installed_before_this_change",
              oldRule(true, true, false) && !oldRule(false, true, false) && !oldRule(true, false, false)
                  && !oldRule(true, true, true));
    }
    {
        // Target: `lootann 1` from the launch commands at character select. The
        // tick looks every kInstallPollFrames frames and installs nothing until
        // the player resolves; then it installs once.
        LootAnnounceMod m;
        bool tried = false;
        m.SetEnabled(true);
        const std::string armed = m.InstallState(tried);
        int looks = 0;
        bool installed = false;
        for (unsigned long long frame = 0; frame < 600; ++frame) {
            if (!LootAnnounceMod::LooksForPlayer(frame)) continue;
            ++looks;
            if (m.ShouldInstall(true, false, tried)) installed = true;
        }
        const bool beforeSetup = m.ShouldInstall(false, true, tried);
        const std::string waiting = m.InstallState(tried);
        const bool now = m.ShouldInstall(true, true, tried);
        if (now) tried = true;
        const bool again = m.ShouldInstall(true, true, tried);
        check("target/waits_for_a_character_before_installing",
              LootAnnounceMod::kInstallPollFrames == 60 && looks == 10 && !installed && !beforeSetup
                  && armed == "waiting-for-character" && waiting == "waiting-for-character" && now && !again
                  && std::string(m.InstallState(tried)) == "installed",
              "looks=" + std::to_string(looks) + " armed=" + armed + " waiting=" + waiting + " now=" + std::to_string(now)
                  + " again=" + std::to_string(again));
    }
    {
        // Switched on in game, with the player there: at once, as before.
        // Off and on again keeps the one install of the session.
        LootAnnounceMod m;
        m.SetEnabled(true);
        bool tried = false;
        const bool now = m.ShouldInstall(true, true, tried);
        if (now) tried = true;
        m.SetEnabled(false);
        const std::string offAfter = m.InstallState(tried);
        m.SetEnabled(true);
        check("target/a_switch_on_in_game_installs_at_once",
              now && offAfter == "installed" && !m.ShouldInstall(true, true, tried)
                  && std::string(m.InstallState(tried)) == "installed",
              "now=" + std::to_string(now) + " off=" + offAfter);
    }
    {
        // Negative control: off, nothing installs whatever else holds, and the
        // state says it was never armed.
        LootAnnounceMod m;
        bool any = false;
        for (int bits = 0; bits < 8; ++bits)
            if (m.ShouldInstall((bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0)) any = true;
        const std::string neverOn = m.InstallState(false);
        m.SetEnabled(true);
        m.SetEnabled(false);
        check("target/switched_off_never_installs",
              !any && !m.ShouldInstall(true, true, false) && neverOn == "not-armed"
                  && std::string(m.InstallState(false)) == "not-armed",
              neverOn);
    }

    // ---- baseline: the switch off --------------------------------------------
    {
        LootAnnounceMod m;
        check("baseline/starts_off", !m.Enabled() && m.StatusLine() == "lootann: off", m.StatusLine());
        const Verdict v = fresh(m, { Heroic, 100, "1700000000001" });
        check("baseline/off_heroic_not_announced", v == Verdict::Off, LootAnnounceMod::VerdictName(v));
        bool allOff = true;
        for (int code : std::vector<int>{ Common, Superior, Rare, Mythic, Satanic, Angelic, Heroic, Unholy,
                                          LootAnnounceMod::kRarityUnread })
            if (fresh(m, { code, 200 + code, "s" + std::to_string(code) }) != Verdict::Off) allOff = false;
        // An item that was not built just now is off too.
        if (see(m, { Heroic, 300, "bag" }) != Verdict::Off) allOff = false;
        check("baseline/off_every_rarity_not_announced", allOff);
        check("baseline/off_counts_and_remembers_nothing",
              m.Stats().seen == 0 && m.Stats().announced == 0 && m.Remembered() == 0 && m.Stats().created == 0, counts(m));
        // Switched on later, the item seen while off is new to it.
        m.SetEnabled(true);
        check("baseline/item_seen_while_off_is_new_when_on", fresh(m, { Heroic, 100, "1700000000001" }) == Verdict::Announce,
              counts(m));
    }
    {
        // A CreateItemNew return the hook sees while the switch is off is not
        // noted: once on, that item is not recently created.
        LootAnnounceMod m;
        const Drop item{ Heroic, 150, "1700000000150" };
        m.NoteCreated(keyOf(item));
        m.SetEnabled(true);
        const bool recent = m.RecentlyCreated(keyOf(item));
        const Verdict v = see(m, item);
        check("baseline/off_notes_no_creation",
              !recent && v == Verdict::HeldBagDrop && m.Stats().created == 0 && m.Stats().announced == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }

    // ---- target: Heroic, Angelic and Unholy announce once --------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        check("target/on_status", m.Enabled() && m.StatusLine() == "lootann: on", m.StatusLine());
        const Verdict h = fresh(m, { Heroic, 1001, "t1001" });
        const Verdict a = fresh(m, { Angelic, 1002, "t1002" });
        const Verdict u = fresh(m, { Unholy, 1003, "t1003" });
        check("target/heroic_announced", h == Verdict::Announce, LootAnnounceMod::VerdictName(h));
        check("target/angelic_announced", a == Verdict::Announce, LootAnnounceMod::VerdictName(a));
        check("target/unholy_announced", u == Verdict::Announce, LootAnnounceMod::VerdictName(u));
        check("target/three_announced_counted", m.Stats().announced == 3 && m.Stats().seen == 3 && m.Remembered() == 3,
              counts(m));
    }

    // ---- target: everything below Heroic, and an unread rarity, stays quiet --
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Verdict s = fresh(m, { Satanic, 2001, "t2001" });
        const Verdict y = fresh(m, { Mythic, 2002, "t2002" });
        const Verdict c = fresh(m, { Common, 2003, "t2003" });
        check("target/satanic_not_announced", s == Verdict::HeldRarity, LootAnnounceMod::VerdictName(s));
        check("target/mythic_not_announced", y == Verdict::HeldRarity, LootAnnounceMod::VerdictName(y));
        check("target/common_not_announced", c == Verdict::HeldRarity, LootAnnounceMod::VerdictName(c));
        bool rest = true;
        for (int code : std::vector<int>{ Superior, Rare, Code4, Code8, 0, 11, 16 })
            if (fresh(m, { code, 2100 + code, "r" + std::to_string(code) }) != Verdict::HeldRarity) rest = false;
        check("target/other_codes_not_announced", rest, counts(m));
        const Verdict n = fresh(m, { LootAnnounceMod::kRarityUnread, 2004, "t2004" });
        check("target/unread_rarity_not_announced_and_counted",
              n == Verdict::HeldNoRarity && m.Stats().heldNoRarity == 1, counts(m));
        check("target/held_counted_never_announced_never_remembered",
              m.Stats().announced == 0 && m.Stats().heldRarity == 10 && m.Remembered() == 0 && m.Stats().seen == 11,
              counts(m));
    }

    // ---- target: a second sight of the same item -----------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop item{ Heroic, 3001, "1759600000123" };
        const Verdict first = fresh(m, item);
        const Verdict second = fresh(m, item);
        const Verdict third = see(m, item);
        check("target/second_sight_not_announced",
              first == Verdict::Announce && second == Verdict::HeldDuplicate && third == Verdict::HeldDuplicate
                  && m.Stats().announced == 1 && m.Stats().heldDuplicate == 2,
              counts(m));
        // With a real stamp the identity is the item's type and stamp, not the
        // ground instance: another type or another stamp is another item, the
        // same pair under another ground id is the same one.
        const Verdict otherType = fresh(m, { Heroic, 3002, "1759600000123", "5", 3002 });
        const Verdict otherStamp = fresh(m, { Heroic, 3003, "1759600000999", "4", 3003 });
        const Verdict sameIdentity = fresh(m, { Heroic, 3004, "1759600000123", "4", 3004 });
        check("target/identity_is_item_type_and_time_stamp",
              otherType == Verdict::Announce && otherStamp == Verdict::Announce && sameIdentity == Verdict::HeldDuplicate
                  && m.Stats().announced == 3,
              counts(m));
        // No real stamp (empty, 0 or undefined): the ground instance id with the
        // stamp, as before the creation guard.
        bool fallback = true;
        for (const std::string& s : { std::string(""), std::string("0"), std::string("undefined") }) {
            if (fresh(m, { Angelic, 3100, s, "4", 3100 }) != Verdict::Announce) fallback = false;
            if (fresh(m, { Angelic, 3100, s, "4", 3100 }) != Verdict::HeldDuplicate) fallback = false;
            if (fresh(m, { Angelic, 3101, s, "4", 3101 }) != Verdict::Announce) fallback = false;
        }
        check("target/identity_without_a_stamp_is_the_ground_id", fallback && m.Stats().announced == 9, counts(m));
        // What the adapter asks before Decide. A real stamp needs a read
        // itemType; without one the ground id must have been read. Two items
        // read as nothing would otherwise share one identity, which the last
        // line shows the memory cannot tell apart.
        bool identifiable = LootAnnounceMod::Identifiable(true, "4", "1759600000123")
            && LootAnnounceMod::Identifiable(false, "4", "1759600000123")
            && !LootAnnounceMod::Identifiable(true, "", "1759600000123")
            && !LootAnnounceMod::Identifiable(false, "", "1759600000123");
        for (const std::string& s : { std::string(""), std::string("0"), std::string("undefined") })
            identifiable = identifiable && LootAnnounceMod::Identifiable(true, "4", s) && LootAnnounceMod::Identifiable(true, "", s)
                && !LootAnnounceMod::Identifiable(false, "4", s) && !LootAnnounceMod::Identifiable(false, "", s);
        check("target/unread_identity_is_not_identifiable",
              identifiable
                  && LootAnnounceMod::Identity(3200, "", "1759600000123") == LootAnnounceMod::Identity(3201, "", "1759600000123"));
        // Off, then on again: the memory is kept, so a still-lying item is not
        // announced a second time.
        m.SetEnabled(false);
        m.SetEnabled(true);
        check("target/memory_kept_across_off_on", fresh(m, item) == Verdict::HeldDuplicate, counts(m));
    }

    // ---- target: the creation guard -------------------------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 5001, "213292866000" };
        m.NoteCreated(keyOf(d));
        const Verdict v = see(m, d);
        check("target/created_this_tick_announced", v == Verdict::Announce && m.Stats().created == 1,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Built in one frame, on the ground in the next.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 5101, "213292867001" };
        m.NoteCreated(keyOf(d));
        tick(m);
        const Verdict v = see(m, d);
        check("target/created_previous_tick_announced", v == Verdict::Announce,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Angelic, 5201, "213292868002" };
        m.NoteCreated(keyOf(d));
        tick(m, 2);
        const Verdict v = see(m, d);
        check("target/created_two_ticks_ago_held",
              v == Verdict::HeldBagDrop && m.Stats().heldBagDrop == 1 && m.Stats().announced == 0 && m.Remembered() == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Picked up and dropped from the bag seconds later: the same item
        // struct, under a new ground instance.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Verdict first = fresh(m, { Heroic, 5301, "1759600005301", "4", 0xA000 });
        tick(m, 2);
        const Verdict again = see(m, { Heroic, 5302, "1759600005301", "4", 0xA000 });
        check("target/redrop_after_pickup_held",
              first == Verdict::Announce && again == Verdict::HeldBagDrop && m.Stats().announced == 1
                  && m.Stats().heldBagDrop == 1,
              LootAnnounceMod::VerdictName(again) + std::string(" ") + counts(m));
    }
    {
        // A freshly built struct whose identity was already announced: the
        // memory, not the creation window, holds it.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Verdict first = fresh(m, { Heroic, 5401, "1759600005401", "4", 0xB000 });
        tick(m, 2);
        const Verdict v = fresh(m, { Heroic, 5402, "1759600005401", "4", 0xB001 });
        check("target/same_stamp_new_ground_id_held",
              first == Verdict::Announce && v == Verdict::HeldDuplicate && m.Stats().announced == 1
                  && m.Stats().heldDuplicate == 1,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 5501, "t5501" };
        m.NoteCreated(keyOf(d));
        const bool before = m.RecentlyCreated(keyOf(d));
        m.SetEnabled(false);
        m.SetEnabled(true);
        const bool after = m.RecentlyCreated(keyOf(d));
        const Verdict v = see(m, d);
        check("target/off_clears_creation_window", before && !after && v == Verdict::HeldBagDrop,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // The window holds kCreationCap keys; a note past it is counted and
        // not kept. A key noted twice counts once.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const uint64_t cap = (uint64_t)LootAnnounceMod::kCreationCap;
        for (uint64_t i = 0; i <= cap; ++i) m.NoteCreated({ LootAnnounceMod::kKeyStruct, 0x10000 + i });
        m.NoteCreated({ LootAnnounceMod::kKeyStruct, 0x10000 });
        const bool firstKept = m.RecentlyCreated({ LootAnnounceMod::kKeyStruct, 0x10000 });
        const bool lastDropped = !m.RecentlyCreated({ LootAnnounceMod::kKeyStruct, 0x10000 + cap });
        check("target/creation_window_cap",
              LootAnnounceMod::kCreationCap >= 1024 && m.Stats().created == (long long)cap
                  && m.Stats().createOverflow == 1 && firstKept && lastDropped,
              counts(m));
        // After a tick the next frame's window has room again.
        tick(m);
        m.NoteCreated({ LootAnnounceMod::kKeyStruct, 0x10000 + cap });
        check("target/creation_window_cap_is_per_window",
              m.RecentlyCreated({ LootAnnounceMod::kKeyStruct, 0x10000 + cap })
                  && m.RecentlyCreated({ LootAnnounceMod::kKeyStruct, 0x10000 }) && m.Stats().createOverflow == 1,
              counts(m));
    }
    {
        // A struct key and a reference key with the same value are two keys.
        LootAnnounceMod m;
        m.SetEnabled(true);
        m.NoteCreated({ LootAnnounceMod::kKeyStruct, 7 });
        check("target/key_kind_is_part_of_the_key",
              m.RecentlyCreated({ LootAnnounceMod::kKeyStruct, 7 }) && !m.RecentlyCreated({ LootAnnounceMod::kKeyReference, 7 }),
              counts(m));
    }
    {
        // Live procedure 2 (2026-10-04), steps 2-6: three placements, each
        // built and placed in one call; the Heroic picked up and dropped from
        // the bag (the old window announced it); then off and a fourth
        // placement.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Verdict h = fresh(m, { Heroic, 262176, "1759590000001", "4", 0xC001 });
        tick(m);
        const Verdict a = fresh(m, { Angelic, 262177, "1759590000002", "4", 0xC002 });
        tick(m);
        const Verdict s = fresh(m, { Satanic, 262178, "1759590000003", "4", 0xC003 });
        tick(m, 3);
        const Verdict b = see(m, { Heroic, 262179, "1759590000001", "4", 0xC001 });
        tick(m);
        const auto& c = m.Stats();
        const bool counted = c.seen == 4 && c.announced == 2 && c.heldRarity == 1 && c.heldBagDrop == 1;
        const LootAnnounceMod::Counters before = m.Stats();
        m.SetEnabled(false);
        const Verdict o = fresh(m, { Heroic, 262180, "1759590000004", "4", 0xC004 });
        const auto& z = m.Stats();
        const bool unchanged = z.seen == before.seen && z.announced == before.announced && z.heldRarity == before.heldRarity
            && z.heldBagDrop == before.heldBagDrop && z.created == before.created;
        check("target/live2_replay",
              h == Verdict::Announce && a == Verdict::Announce && s == Verdict::HeldRarity && b == Verdict::HeldBagDrop
                  && counted && o == Verdict::Off && unchanged,
              counts(m));
    }

    // ---- the Chaos Tower's loot burst (ForgePact #227) ------------------------
    // A tower floor collects its fresh drops as they land (into the
    // Controller's loot burst list, taking them off the floor) and puts the
    // same item structs on the ground again when the floor ends, without
    // building them anew. The adapter reads the list once per tick, after the
    // batch's decisions and before the window ages, and notes each new entry.
    {
        // Baseline: today's rule. Built at frame T (the first landing, which
        // the floor takes back), on the ground again ten seconds later with
        // nothing remembering the list: the guard holds it as a bag drop.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 6001, "1759700006001", "4", 0xD001 };
        m.NoteCreated(keyOf(d));
        tick(m, 600);
        const Verdict v = see(m, d);
        check("baseline/collected_item_landing_later_is_held",
              v == Verdict::HeldBagDrop && m.Stats().heldBagDrop == 1 && m.Stats().announced == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Baseline: off, a listed entry is neither counted nor kept, even one
        // the hook would have seen built; on again, its landing is held.
        LootAnnounceMod m;
        const Drop d{ Heroic, 6101, "1759700006101", "4", 0xD101 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        m.SetEnabled(true);
        const bool kept = m.InBurstMemory(keyOf(d));
        tick(m, 600);
        const Verdict v = see(m, d);
        check("baseline/off_remembers_no_burst_entry",
              !kept && v == Verdict::HeldBagDrop && m.Stats().burstListed == 0 && m.Stats().burstFresh == 0
                  && m.Stats().burstLanded == 0 && m.Stats().announced == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Target: built and listed at T (the frame it first landed and the
        // floor took it back), on the ground at the floor's end, T+600.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 6201, "1759700006201", "4", 0xD201 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        tick(m, 600);
        const Verdict v = see(m, d);
        check("target/fresh_burst_entry_lands_as_new",
              v == Verdict::Announce && m.Stats().announced == 1 && m.Stats().burstListed == 1
                  && m.Stats().burstFresh == 1 && m.Stats().burstLanded == 1 && m.Stats().heldBagDrop == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Target: an entry whose struct was not built this frame or the last -
        // a bag drop on a tower floor, which the floor collects all the same,
        // or a struct built long before - is listed but not kept.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop bag{ Heroic, 6301, "1759700006301", "4", 0xD301 };
        const Drop old{ Angelic, 6302, "1759700006302", "4", 0xD302 };
        m.NoteCreated(keyOf(old));
        tick(m, 2);
        listed(m, bag);
        listed(m, old);
        const bool kept = m.InBurstMemory(keyOf(bag)) || m.InBurstMemory(keyOf(old));
        tick(m, 600);
        const Verdict vb = see(m, bag);
        const Verdict vo = see(m, old);
        check("target/stale_burst_entry_stays_held",
              !kept && vb == Verdict::HeldBagDrop && vo == Verdict::HeldBagDrop && m.Stats().burstListed == 2
                  && m.Stats().burstFresh == 0 && m.Stats().burstLanded == 0 && m.Stats().announced == 0,
              counts(m));
    }
    {
        // Target: the burst may put one ground item through LootGroundInit
        // twice in its frame; a sighting does not consume the key, and the
        // identity memory keeps it to one line.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Unholy, 6401, "1759700006401", "4", 0xD401 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        tick(m, 600);
        const Verdict first = see(m, d);
        const Verdict second = see(m, d);
        const bool stillKept = m.InBurstMemory(keyOf(d));
        check("target/burst_landing_twice_in_one_frame_one_line",
              first == Verdict::Announce && second == Verdict::HeldDuplicate && stillKept && m.Stats().announced == 1
                  && m.Stats().heldDuplicate == 1 && m.Stats().heldBagDrop == 0,
              counts(m));
    }
    {
        // Target: the adapter tells the core the list shrank or changed (the
        // floor's end emptied it, another floor's list): every kept key goes,
        // and the same struct on the ground again is held.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 6501, "1759700006501", "4", 0xD501 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        tick(m, 600);
        const Verdict first = see(m, d);
        m.ForgetBurst();
        const bool kept = m.InBurstMemory(keyOf(d));
        tick(m, 600);
        const Verdict again = see(m, { Heroic, 6502, "1759700006501", "4", 0xD501 });
        // A kept entry that never landed goes too.
        const Drop other{ Angelic, 6503, "1759700006503", "4", 0xD503 };
        m.NoteCreated(keyOf(other));
        listed(m, other);
        m.ForgetBurst();
        tick(m, 600);
        const Verdict never = see(m, other);
        check("target/burst_memory_forgotten_when_list_shrinks",
              first == Verdict::Announce && !kept && again == Verdict::HeldBagDrop && never == Verdict::HeldBagDrop
                  && m.Stats().announced == 1 && m.Stats().heldBagDrop == 2,
              counts(m));
    }
    {
        // Target: the burst passes the creation guard only; the rarity and the
        // identity still decide.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop satanic{ Satanic, 6601, "1759700006601", "4", 0xD601 };
        const Drop heroic{ Heroic, 6602, "1759700006602", "4", 0xD602 };
        m.NoteCreated(keyOf(satanic));
        listed(m, satanic);
        const Verdict announced = fresh(m, { Heroic, 6603, "1759700006602", "4", 0xD603 });
        m.NoteCreated(keyOf(heroic));
        listed(m, heroic);
        tick(m, 600);
        const Verdict vs = see(m, satanic);
        const Verdict vh = see(m, heroic);
        check("target/burst_rarity_and_identity_rules_still_apply",
              announced == Verdict::Announce && vs == Verdict::HeldRarity && vh == Verdict::HeldDuplicate
                  && m.Stats().announced == 1 && m.Stats().heldRarity == 1 && m.Stats().heldDuplicate == 1
                  && m.Stats().burstLanded == 2,
              counts(m));
    }
    {
        // Target: the memory keeps kBurstCap keys; a note past it is counted
        // and not kept. The creation window holds one frame's notes, so the
        // keys are built over two frames.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const uint64_t cap = (uint64_t)LootAnnounceMod::kBurstCap;
        for (uint64_t i = 0; i < cap; ++i) {
            const LootAnnounceMod::ItemKey k{ LootAnnounceMod::kKeyStruct, 0x100000 + i };
            m.NoteCreated(k);
            m.NoteListed(k);
        }
        tick(m);
        const LootAnnounceMod::ItemKey past{ LootAnnounceMod::kKeyStruct, 0x100000 + cap };
        m.NoteCreated(past);
        m.NoteListed(past);
        const bool firstKept = m.InBurstMemory({ LootAnnounceMod::kKeyStruct, 0x100000 });
        const bool lastKept = m.InBurstMemory({ LootAnnounceMod::kKeyStruct, 0x100000 + cap - 1 });
        const bool pastKept = m.InBurstMemory(past);
        check("target/burst_memory_capped",
              LootAnnounceMod::kBurstCap == 4096 && firstKept && lastKept && !pastKept
                  && m.Stats().burstListed == (long long)cap + 1 && m.Stats().burstFresh == (long long)cap
                  && m.Stats().burstOverflow == 1,
              counts(m));
        // Forgotten, the memory has room again.
        m.ForgetBurst();
        m.NoteListed(past);
        check("target/burst_memory_cap_frees_on_forget", m.InBurstMemory(past) && m.Stats().burstOverflow == 1,
              counts(m));
    }
    {
        // Target: switching off forgets the burst memory, as it clears the
        // creation window; the counters stay.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Heroic, 6801, "1759700006801", "4", 0xD801 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        const bool before = m.InBurstMemory(keyOf(d));
        m.SetEnabled(false);
        m.SetEnabled(true);
        const bool after = m.InBurstMemory(keyOf(d));
        tick(m, 600);
        const Verdict v = see(m, d);
        check("target/off_forgets_burst_memory",
              before && !after && v == Verdict::HeldBagDrop && m.Stats().burstListed == 1 && m.Stats().burstFresh == 1
                  && m.Stats().burstLanded == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Target: built, listed and on the ground in one frame (an entry the
        // floor collected in the frame it was built): the creation window
        // passes it, so the burst memory is not what let it through.
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop d{ Angelic, 6901, "1759700006901", "4", 0xD901 };
        m.NoteCreated(keyOf(d));
        listed(m, d);
        const Verdict v = see(m, d);
        check("target/collected_and_burst_in_one_frame_counts_as_recent",
              v == Verdict::Announce && m.Stats().burstFresh == 1 && m.Stats().burstLanded == 0,
              LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
    }
    {
        // Target: the stat line's four burst fields, after create-overflow.
        LootAnnounceMod m;
        const std::string offLine = m.StatLine();
        m.SetEnabled(true);
        const Drop a{ Heroic, 7001, "1759700007001", "4", 0xE001 };
        const Drop b{ Heroic, 7002, "1759700007002", "4", 0xE002 };
        m.NoteCreated(keyOf(a));
        listed(m, a);
        listed(m, b);
        tick(m, 600);
        see(m, a);
        const std::string onLine = m.StatLine();
        const auto endsWith = [](const std::string& s, const std::string& tail) {
            return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
        };
        check("target/stat_line_burst_fields",
              endsWith(offLine, " create-overflow=0 burst-listed=0 burst-fresh=0 burst-landed=0 burst-overflow=0")
                  && endsWith(onLine, " create-overflow=0 burst-listed=2 burst-fresh=1 burst-landed=1 burst-overflow=0"),
              onLine);
    }

    // ---- the memory is capped ------------------------------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const int64_t total = (int64_t)LootAnnounceMod::kMemoryCap + 1;
        long announced = 0;
        for (int64_t i = 0; i < total; ++i) {
            if (fresh(m, { Angelic, 10000 + i, "c" + std::to_string(i) }) == Verdict::Announce) ++announced;
            tick(m);
        }
        check("memory/capped", m.Remembered() == LootAnnounceMod::kMemoryCap && announced == total, counts(m));
        // The oldest is forgotten first; the newest is still remembered.
        check("memory/oldest_forgotten_first",
              fresh(m, { Angelic, 10000 + total - 1, "c" + std::to_string(total - 1) }) == Verdict::HeldDuplicate
                  && fresh(m, { Angelic, 10000, "c0" }) == Verdict::Announce,
              counts(m));
    }

    // ---- the sink's refusal and the stat line --------------------------------
    {
        LootAnnounceMod m;
        check("stat/fresh_off",
              m.StatLine() == "lootann: off route=chatcolour seen=0 announced=0 held-rarity=0 held-no-rarity=0 "
                              "held-duplicate=0 held-bag-drop=0 sink-refused=0 remembered=0 created=0 create-overflow=0 "
                              "burst-listed=0 burst-fresh=0 burst-landed=0 burst-overflow=0",
              m.StatLine());
        m.SetEnabled(true);
        fresh(m, { Heroic, 1, "a" });
        fresh(m, { Heroic, 1, "a" });
        fresh(m, { Satanic, 2, "b" });
        fresh(m, { LootAnnounceMod::kRarityUnread, 3, "c" });
        see(m, { Angelic, 4, "d" });
        m.NoteSinkRefused();
        check("stat/counts",
              m.StatLine() == "lootann: on route=chatcolour seen=5 announced=1 held-rarity=1 held-no-rarity=1 "
                              "held-duplicate=1 held-bag-drop=1 sink-refused=1 remembered=1 created=3 create-overflow=0 "
                              "burst-listed=0 burst-fresh=0 burst-landed=0 burst-overflow=0",
              m.StatLine());
        check("stat/verdict_names",
              std::string(LootAnnounceMod::VerdictName(Verdict::Off)) == "off"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::Announce)) == "announce"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldRarity)) == "held-rarity"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldNoRarity)) == "held-no-rarity"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldDuplicate)) == "held-duplicate"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldBagDrop)) == "held-bag-drop");
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
