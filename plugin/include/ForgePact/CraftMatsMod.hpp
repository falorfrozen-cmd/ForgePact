#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ForgePact {

// Crafting from the stash's special tabs (ForgePact issue #14). While the mod
// is on, a recipe at the game's own Crafting Cube also counts the materials and
// socketables held in the shared stash's Materials (-4) and Socketable (-2)
// tabs, and at the craft press only the amount the bag is short moves from
// those tabs into the bag (or the Cube's own grid), where the game consumes it
// as it would from the bag. Owner's decisions (2026-09-22..24,
// docs/crafting-materials-research.md): count AND consume; bag first, then the
// stash, which supplies only the shortfall; move only what the recipe needs;
// ordinary stash grid tabs, the guild stash and the Unique tab are never a
// source.
//
// This header decides; the adapter in ModuleMain.cpp touches the game. What it
// decides, each pinned by tests/craft_mats_harness.cpp:
// - the count: CountInventoryItem's own answer k, plus the two tabs' stash
//   count s only while the mod is on and the call is inside the crafting route
//   (CountAnswer); an unreadable stash leaves k and is named once;
// - the walk: display frames reuse one walk of the two tabs per game frame, the
//   press always walks fresh (MustWalk);
// - the needs: inside CraftFindRecipeItems each count takes the amount of the
//   latest PilipaliDecrypt before it, and of a run of counts after one decode
//   the last is the one the game used; a count with no decode before it makes
//   the needs unreadable (BeginFind/OnDecode/OnFindCount/Needs);
// - the press gate: the game's own press when no stash count was added, a
//   refusal when the needs cannot be paired (PressStep), then Plan - the
//   shortfall per input, need - k capped by a fresh stash count;
// - the split across entries, whole entries first and then a partial from the
//   last (Split);
// - the move outcome from both sides' re-reads (OnMoveReport), and the craft
//   only when every take is confirmed (MayCraft);
// - the consume check after the game's own press (OnConsume);
// - the stash save only after a confirmed move (SaveDue), and the lines.
//
// It is game-independent on purpose: it names no runtime interface, builtin,
// log call or runtime value type (test_craft_mats_contract.py checks the
// spellings), so tests/craft_mats_harness.cpp compiles it whole with no
// runtime stub.

// Where a take comes from: the two special tabs, and that is the design.
// Nothing an adapter can pass names an ordinary stash grid tab, the guild
// stash or any other container, so no input can make the core plan a take
// from one.
enum class CraftMatsSource : int { StashMaterialTab = 1, StashSocketTab = 2 };

// Where a moved unit lands: onto the bag's existing stack of it, as a new
// stack in the bag, or into the Crafting Cube's own grid when the bag cannot
// take a new stack (the owner's "in case it's full").
enum class CraftMatsDestination : int { BagStack = 1, BagNew, Cube };

// Whether the press's stash save ran: not asked (nothing moved), dispatched,
// or asked and not dispatched.
enum class CraftMatsSave : int { No = 0, Yes, Failed };

// What the press hook does before the game's own DoCraftResult: run it
// untouched, refuse it (return before it), or plan the takes.
enum class CraftMatsPressStep : int { Vanilla = 1, Refuse, Plan };

// One stash entry of the two special tabs, as one walk read it: its material
// ("class=<c> b=<b>"), its key in the stash map, its count `o`, its tab, and
// where its cell array is - the Socketable tab's row, or -1 for the Materials
// tab, whose cell array is the whole tab.
struct CraftMatsEntry {
    std::string     material;
    std::string     key;
    int64_t         count = 0;
    CraftMatsSource from = CraftMatsSource::StashMaterialTab;
    int             row = -1;
};

// One walk of the two special tabs. A walk that could not be read answers -1
// for every material, never 0: "unknown" must not compare equal to "empty".
struct CraftMatsWalk {
    bool                        readable = false;
    std::vector<CraftMatsEntry> entries;

    int64_t Count(const std::string& material) const {
        if (!readable) return -1;
        int64_t n = 0;
        for (const CraftMatsEntry& e : entries) if (e.material == material) n += e.count;
        return n;
    }
};

// One take from one entry, as Split planned it: whole (the entry empties) or
// a partial (the entry keeps the rest).
struct CraftMatsEntryTake {
    CraftMatsEntry entry;
    int64_t        amount = 0;
    bool           whole = false;
};

// One material a craft needs, as the adapter read it for this press. A count
// that could not be read is negative (-1), never 0: "unknown" must not compare
// equal to "empty".
struct CraftMatsNeed {
    std::string material;    // the game's own id for the material type, as text
    int64_t     need = 0;    // how many the recipe needs; 0 or less is no need
    int64_t     bag = -1;    // held in the character's materials tab
    int64_t     stash = -1;  // held in the stash's material tab
};

struct CraftMatsTake {
    std::string     material;
    int64_t         amount = 0;
    CraftMatsSource from = CraftMatsSource::StashMaterialTab;
};

// Why a press or a take was not served, or a count was left alone. Each is
// named once per session.
enum class CraftMatsRefusal : int {
    None = 0,
    Unreadable,       // a count or amount the decision needed could not be read; nothing was taken
    NotTaken,         // the game declined a take and the stash tab is unchanged; nothing was taken
    StashUnreadable,  // the two tabs could not be read for a count; the game's own count stood
    Count
};

struct CraftMatsPlan {
    bool                       refused = false;
    CraftMatsRefusal           reason = CraftMatsRefusal::None;
    std::vector<CraftMatsTake> takes;   // empty: take nothing, let the game craft as it would
};

// How one take went, as the adapter saw it: the game's own success answer and
// the stash tab's count re-read before and after. -1 = that read was not made.
struct CraftMatsTakeReport {
    std::string material;
    int64_t     asked = 0;
    bool        success = false;
    int64_t     stashBefore = -1;
    int64_t     stashAfter = -1;
};

// Taken: success, and the re-read shows exactly `asked` gone. NotTaken: no
// success and the tab is unchanged. Loss: anything else - the tab changed
// without the game's success answer (a possible loss), or a success the
// re-read cannot confirm (a possible duplicate or loss); it turns the mod off
// for the session.
enum class CraftMatsOutcome : int { Taken = 1, NotTaken, Loss };

// What one CraftFindRecipeItems call's counts and decodes amount to, per
// input: material, need = the decoded amount, bag = the game's own count k;
// `stash` is -1 here, since the press reads it fresh. `stashAdded` is whether
// any count inside that call had the stash added to it; `self` is the recipe
// row the call ran on, as the adapter numbers it.
struct CraftMatsNeeds {
    bool                       readable = false;
    bool                       stashAdded = false;
    long long                  self = -1;
    std::vector<CraftMatsNeed> rows;
};

// One move of `asked` units from one stash entry, re-read on both sides after
// it (and after any undo). The source: the entry's `o` before and after, and
// whether the entry left the stash map and its cell emptied - both, for a
// whole take, and neither for a partial. The destination: the bag stack's `o`
// before and after, or 0 before and the new unit's `o` after. -1 = that read
// could not be made.
struct CraftMatsMoveReport {
    int64_t asked = 0;
    int64_t sourceBefore = -1;
    int64_t sourceAfter = -1;
    bool    sourceEntryGone = false;
    bool    sourceCellGone = false;
    int64_t destBefore = -1;
    int64_t destAfter = -1;
};

// A confirmed move, for the per-press line.
struct CraftMatsMoved {
    std::string          material;
    CraftMatsSource      from = CraftMatsSource::StashMaterialTab;
    CraftMatsDestination to = CraftMatsDestination::BagStack;
    int64_t              units = 0;
};

// Threading: every caller runs on the game thread (a hook body inside the
// game's own call, or the IPC poll from FrameCallback), so nothing is locked.
// The flags and counters are atomic only so a stat read never sees a torn value.
class CraftMatsMod {
public:
    static CraftMatsMod& Instance() {
        static CraftMatsMod s_Instance;
        return s_Instance;
    }

    // Public so the harness can build a fresh instance per scenario; the
    // plugin only ever uses Instance().
    CraftMatsMod() = default;

    bool IsEnabled() const { return m_Enabled.load(); }
    bool OffThisSession() const { return m_OffThisSession.load(); }

    // Turning on is refused (false) once a loss has turned the mod off for the
    // session; turning off is always allowed.
    bool SetEnabled(bool on) {
        if (on && m_OffThisSession.load()) return false;
        m_Enabled.store(on);
        return true;
    }

    // The takes one press needs. Off: nothing, whatever it is fed. On: rows of
    // one material are summed (their counts must agree - two reads of one
    // material in one press that disagree are unreadable), a need the bag
    // covers takes nothing, and a shortfall takes what the stash tab holds of
    // it, up to the shortfall. A count the decision needs that could not be
    // read refuses the whole press: no partial plan is made from a press the
    // core could not fully read.
    CraftMatsPlan Plan(const std::vector<CraftMatsNeed>& needs) {
        CraftMatsPlan plan;
        if (!IsEnabled()) { m_WhileOff.fetch_add(1); return plan; }
        m_Plans.fetch_add(1);

        std::vector<CraftMatsNeed> rows;
        for (const CraftMatsNeed& n : needs) {
            if (n.need <= 0) continue;
            CraftMatsNeed* same = nullptr;
            for (CraftMatsNeed& r : rows) if (r.material == n.material) { same = &r; break; }
            if (!same) { rows.push_back(n); continue; }
            if (same->bag != n.bag || same->stash != n.stash) return Refuse(CraftMatsRefusal::Unreadable);
            same->need += n.need;
        }
        for (const CraftMatsNeed& r : rows) {
            if (r.bag < 0) return Refuse(CraftMatsRefusal::Unreadable);
            if (r.bag < r.need && r.stash < 0) return Refuse(CraftMatsRefusal::Unreadable);
        }
        for (const CraftMatsNeed& r : rows) {
            if (r.bag >= r.need) continue;
            const int64_t shortfall = r.need - r.bag;
            const int64_t amount = shortfall < r.stash ? shortfall : r.stash;
            if (amount <= 0) continue;
            CraftMatsTake t;
            t.material = r.material;
            t.amount = amount;
            plan.takes.push_back(t);
        }
        if (!plan.takes.empty()) m_PlansWithTakes.fetch_add(1);
        return plan;
    }

    // One take, after the game's own routine ran and the tab was re-read. While
    // off nothing is taken, so a stray report changes nothing.
    CraftMatsOutcome OnTakeReport(const CraftMatsTakeReport& r) {
        if (!IsEnabled()) return CraftMatsOutcome::NotTaken;
        const bool readBoth = r.stashBefore >= 0 && r.stashAfter >= 0;
        if (r.success && readBoth && r.asked > 0 && r.stashAfter == r.stashBefore - r.asked) {
            m_Taken.fetch_add(1);
            m_TakenUnits.fetch_add(r.asked);
            if (!m_FirstTakeSeen) { m_FirstTakeSeen = true; m_FirstTakeDue = true; }
            return CraftMatsOutcome::Taken;
        }
        if (!r.success && readBoth && r.stashAfter == r.stashBefore) {
            Note(CraftMatsRefusal::NotTaken);
            return CraftMatsOutcome::NotTaken;
        }
        Lose(r.success
            ? "the game answered success but the stash tab's re-read could not confirm the take"
            : "the stash tab changed, or could not be read, without the game answering success");
        return CraftMatsOutcome::Loss;
    }

    // ---- the count ---------------------------------------------------------------

    // "class=<c> b=<b>": how a material is named in the needs and on the lines -
    // the item's itemType and its definition's base id `b`.
    static std::string Material(int64_t cls, int64_t base) {
        return "class=" + std::to_string(cls) + " b=" + std::to_string(base);
    }

    static const char* SourceName(CraftMatsSource s) {
        return s == CraftMatsSource::StashSocketTab ? "socketable" : "materials";
    }

    // CountInventoryItem's answer, given the game's own count and the two tabs'
    // stash count of the same material. Off, or outside every crafting-route
    // frame: the game's own count, whatever the stash holds. On and inside the
    // route: the two added. An unreadable stash (-1) leaves the game's count and
    // is named once - "unknown" never adds, and never reads as "empty".
    int64_t CountAnswer(bool inRoute, int64_t game, int64_t stash) {
        if (!IsEnabled() || !inRoute) return game;
        if (stash < 0) { Note(CraftMatsRefusal::StashUnreadable); return game; }
        if (stash == 0) return game;
        m_CountsRaised.fetch_add(1);
        return game + stash;
    }

    // Whether a count needs a fresh walk of the two tabs: always at the press
    // (inside CraftFindRecipeItems or DoCraftResult), and on a display frame only
    // when no walk was kept in this game frame - the availability display runs
    // every frame while the Cube is open, the press once.
    bool MustWalk(uint64_t frame, bool press) const {
        return press || !m_WalkKept || m_WalkFrame != frame;
    }
    void KeepWalk(uint64_t frame, const CraftMatsWalk& walk) {
        m_Walk = walk;
        m_WalkFrame = frame;
        m_WalkKept = true;
        m_Walks.fetch_add(1);
    }
    const CraftMatsWalk& KeptWalk() const { return m_Walk; }

    // ---- the needs -----------------------------------------------------------------
    //
    // Recorded inside CraftFindRecipeItems only (the adapter's depth gate). The
    // next call's entry discards the record; the next DoCraftResult uses it.

    void BeginFind(long long self) {
        m_Events.clear();
        m_FindSelf = self;
        m_FindOpen = true;
        m_FindUsed = false;
    }

    // PilipaliDecrypt's answer inside the call; -1 when it was not a whole,
    // non-negative amount.
    void OnDecode(int64_t amount) {
        if (!m_FindOpen) return;
        FindEvent e;
        e.decode = true;
        e.amount = amount;
        m_Events.push_back(e);
    }

    // One bag count inside the call: its material, the game's own count (-1 when
    // it was not a whole number), and whether the stash was added to it.
    void OnFindCount(const std::string& material, int64_t game, bool stashAdded) {
        if (!m_FindOpen) return;
        FindEvent e;
        e.material = material;
        e.game = game;
        e.stashAdded = stashAdded;
        m_Events.push_back(e);
    }

    // The pairing rule, from a static reading of CraftFindRecipeItems (research
    // doc, `## Ship design`): each input's amount is decoded before that input
    // is counted, and an input that accepts several bases is counted one base at
    // a time after its one decode, so a count's amount is the latest decode
    // before it, and of a run of counts after one decode the last is the one the
    // game used. A count with no decode before it, or an amount or count that did
    // not read as a whole number, makes the needs unreadable.
    CraftMatsNeeds Needs() const {
        CraftMatsNeeds n;
        n.self = m_FindSelf;
        n.readable = m_FindOpen;
        int decodes = 0, rowDecode = 0;
        int64_t amount = -1;
        for (const FindEvent& e : m_Events) {
            if (e.decode) { ++decodes; amount = e.amount; continue; }
            if (e.stashAdded) n.stashAdded = true;
            if (decodes == 0 || amount < 0 || e.game < 0) { n.readable = false; continue; }
            CraftMatsNeed row;
            row.material = e.material;
            row.need = amount;
            row.bag = e.game;
            row.stash = -1;
            if (!n.rows.empty() && rowDecode == decodes) n.rows.back() = row;
            else n.rows.push_back(row);
            rowDecode = decodes;
        }
        if (!n.readable) n.rows.clear();
        return n;
    }

    // ---- the press -----------------------------------------------------------------

    // Before the game's own DoCraftResult. Off, no record, or no stash count
    // added during that CraftFindRecipeItems call: the game's press, untouched.
    // Otherwise the record must pair (Needs().readable), belong to this press's
    // recipe row, and serve no earlier press; if not, the press is refused -
    // the game's count came from the stash and nothing says what to move. A
    // row the adapter could not number (-1) matches nothing, itself included.
    CraftMatsPressStep PressStep(long long self) {
        if (!IsEnabled()) { m_WhileOff.fetch_add(1); return CraftMatsPressStep::Vanilla; }
        if (!m_FindOpen) return CraftMatsPressStep::Vanilla;
        const CraftMatsNeeds n = Needs();
        const bool used = m_FindUsed;
        m_FindUsed = true;
        if (!n.stashAdded) return CraftMatsPressStep::Vanilla;
        if (used || !n.readable || n.self < 0 || n.self != self) {
            Note(CraftMatsRefusal::Unreadable);
            m_PressesRefused.fetch_add(1);
            return CraftMatsPressStep::Refuse;
        }
        return CraftMatsPressStep::Plan;
    }

    // One take across the material's entries in walk order: whole entries
    // first, then a partial from the last one needed. False (and nothing
    // planned) when the tabs hold less than `amount`, or the walk failed.
    static bool Split(const std::string& material, int64_t amount, const CraftMatsWalk& walk,
                      std::vector<CraftMatsEntryTake>& out) {
        out.clear();
        if (amount <= 0 || walk.Count(material) < amount) return false;
        int64_t left = amount;
        for (const CraftMatsEntry& e : walk.entries) {
            if (left == 0) break;
            if (e.material != material || e.count <= 0) continue;
            CraftMatsEntryTake t;
            t.entry = e;
            t.whole = e.count <= left;
            t.amount = t.whole ? e.count : left;
            left -= t.amount;
            out.push_back(t);
        }
        return left == 0;
    }

    // Confirmed: the source dropped by exactly `asked` (or, for a whole entry,
    // its map entry and its cell are both gone) and the destination rose by
    // exactly `asked`. NotTaken: both sides read as they were. Anything else,
    // including a read that could not be made or an entry left without its
    // cell (the save crash, RUNTIME_DATA_MODELS § 16), is a loss.
    static CraftMatsOutcome ClassifyMove(const CraftMatsMoveReport& r) {
        if (r.asked <= 0 || r.sourceBefore < 0 || r.destBefore < 0 || r.destAfter < 0) return CraftMatsOutcome::Loss;
        const bool bothThere = !r.sourceEntryGone && !r.sourceCellGone;
        const bool bothGone = r.sourceEntryGone && r.sourceCellGone;
        if (bothThere && r.sourceAfter == r.sourceBefore && r.destAfter == r.destBefore) return CraftMatsOutcome::NotTaken;
        const bool source = (bothGone && r.sourceBefore == r.asked)
            || (bothThere && r.sourceAfter > 0 && r.sourceAfter == r.sourceBefore - r.asked);
        return source && r.destAfter == r.destBefore + r.asked ? CraftMatsOutcome::Taken : CraftMatsOutcome::Loss;
    }

    CraftMatsOutcome OnMoveReport(const CraftMatsMoveReport& r) {
        if (!IsEnabled()) return CraftMatsOutcome::NotTaken;
        const CraftMatsOutcome o = ClassifyMove(r);
        if (o == CraftMatsOutcome::Taken) {
            m_Taken.fetch_add(1);
            m_TakenUnits.fetch_add(r.asked);
            if (!m_FirstTakeSeen) { m_FirstTakeSeen = true; m_FirstTakeDue = true; }
        } else if (o == CraftMatsOutcome::NotTaken) {
            Note(CraftMatsRefusal::NotTaken);
        } else {
            Lose("a move from the stash could not be confirmed on both sides (the stash entry and the bag or Cube, re-read after it)");
        }
        return o;
    }

    // The game's press runs only when every planned take was confirmed; no take
    // at all (the bag covers the recipe) is the game's own press. A refusal is
    // counted here; its reason was named where the take was reported.
    bool MayCraft(const std::vector<CraftMatsOutcome>& outcomes) {
        for (CraftMatsOutcome o : outcomes) {
            if (o != CraftMatsOutcome::Taken) { m_PressesRefused.fetch_add(1); return false; }
        }
        return true;
    }
    // The same, after Plan: a plan the core refused (a count it could not read)
    // refuses the press before any take.
    bool MayCraft(const CraftMatsPlan& plan, const std::vector<CraftMatsOutcome>& outcomes) {
        if (plan.refused) { m_PressesRefused.fetch_add(1); return false; }
        return MayCraft(outcomes);
    }

    // After the game's own press, per material the mod moved: the character's
    // total of it (bag and Cube, the game's own item map) must be what it held
    // before the move, plus what moved, minus what the recipe needs. Anything
    // else - the game's own craft that consumes nothing included - is named once
    // and turns the mod off for the session. A total that could not be re-read
    // is not a match.
    bool OnConsume(const std::string& material, int64_t before, int64_t moved, int64_t need, int64_t after) {
        const bool read = before >= 0 && after >= 0;
        if (read && after == before + moved - need) { m_ConsumeChecks.fetch_add(1); return true; }
        m_Mismatches.fetch_add(1);
        m_MismatchText = "craftmats: consume mismatch - " + material + ": "
            + (read ? "the character held " + std::to_string(before) + ", " + std::to_string(moved)
                          + " came from the stash and the recipe needs " + std::to_string(need) + ", so "
                          + std::to_string(before + moved - need) + " should be left, but " + std::to_string(after) + " is"
                    : std::string("the character's total could not be re-read after the craft"))
            + "; off for this session - turn it on again after restarting the game";
        m_OffThisSession.store(true);
        m_Enabled.store(false);
        if (!m_MismatchSeen) { m_MismatchSeen = true; m_MismatchDue = true; }
        return false;
    }

    // True once, after the mismatch that turned the mod off.
    bool TakeFirstMismatch() {
        const bool due = m_MismatchDue;
        m_MismatchDue = false;
        return due;
    }
    std::string MismatchLine() const { return m_MismatchText; }

    // The stash is saved by the game's own route only after a confirmed move:
    // a save when nothing moved would write a stash the game did not change.
    static bool SaveDue(int64_t confirmedUnits) { return confirmedUnits > 0; }

    static const char* SaveName(CraftMatsSave s) {
        switch (s) {
        case CraftMatsSave::Yes:    return "yes";
        case CraftMatsSave::Failed: return "failed";
        default:                    return "no";
        }
    }

    static const char* DestinationName(CraftMatsDestination d) {
        switch (d) {
        case CraftMatsDestination::BagNew: return "bag-new";
        case CraftMatsDestination::Cube:   return "cube";
        default:                           return "bag-stack";
        }
    }

    // The one line a press that moved something writes: what moved, from which
    // tab, to where, and whether the stash was saved - work done, not armed
    // state. Moves of one material by one route are one figure.
    static std::string PressLine(const std::vector<CraftMatsMoved>& moved, CraftMatsSave saved) {
        std::vector<CraftMatsMoved> merged;
        for (const CraftMatsMoved& m : moved) {
            CraftMatsMoved* same = nullptr;
            for (CraftMatsMoved& x : merged) if (x.material == m.material && x.from == m.from && x.to == m.to) { same = &x; break; }
            if (same) same->units += m.units;
            else merged.push_back(m);
        }
        std::string line = "craftmats: moved ";
        for (size_t i = 0; i < merged.size(); ++i)
            line += (i ? ", " : "") + std::to_string(merged[i].units) + " " + merged[i].material + " from "
                + SourceName(merged[i].from) + " to " + DestinationName(merged[i].to);
        return line + "; saved=" + SaveName(saved);
    }

    // The install could not put both routes on every hook: nothing will run
    // this session, and turning it on again is refused.
    void TurnOffForSession() {
        m_OffThisSession.store(true);
        m_Enabled.store(false);
    }

    // Each reason at most once per session, in the order first seen, for the
    // adapter's one log line; None when nothing new is waiting.
    CraftMatsRefusal TakeFirstRefusal() {
        if (m_Unreported.empty()) return CraftMatsRefusal::None;
        const CraftMatsRefusal r = m_Unreported.front();
        m_Unreported.erase(m_Unreported.begin());
        return r;
    }

    static const char* RefusalName(CraftMatsRefusal r) {
        switch (r) {
        case CraftMatsRefusal::Unreadable:      return "unreadable";
        case CraftMatsRefusal::NotTaken:        return "not-taken";
        case CraftMatsRefusal::StashUnreadable: return "stash-unreadable";
        default:                                return "none";
        }
    }

    // The one line a refusal is reported with: what the mod did.
    std::string RefusalLine(CraftMatsRefusal r) const {
        const std::string head = std::string("craftmats: ") + RefusalName(r) + " - ";
        switch (r) {
        case CraftMatsRefusal::Unreadable:
            return head + "a recipe amount or a material count could not be read; the craft was refused, and nothing was"
                          " taken from the stash for it";
        case CraftMatsRefusal::NotTaken:
            return head + "the game declined a move from the stash, which was left as it was; nothing was taken for that"
                          " input, and the craft was refused";
        case CraftMatsRefusal::StashUnreadable:
            return head + "the stash's Materials and Socketable tabs could not be read for a recipe count; that count is"
                          " the bag's alone";
        default:
            return head + "nothing";
        }
    }

    // True once, after the first confirmed take of the session.
    bool TakeFirstTake() {
        const bool due = m_FirstTakeDue;
        m_FirstTakeDue = false;
        return due;
    }
    // `craftmats: first take from the stash tab - plans=1 ...`: the player
    // log's line naming work done, not armed state.
    std::string FirstTakeLine() const { return HeadedStatLine("first take from the stash tab"); }

    // True once, after the loss that turned the mod off.
    bool TakeFirstLoss() {
        const bool due = m_LossDue;
        m_LossDue = false;
        return due;
    }
    std::string LossLine() const {
        return std::string("craftmats: off for this session - ") + m_LossReason
            + "; turn it on again after restarting the game";
    }

    long Plans() const { return m_Plans.load(); }
    long PlansWithTakes() const { return m_PlansWithTakes.load(); }
    long WhileOff() const { return m_WhileOff.load(); }
    long Taken() const { return m_Taken.load(); }
    long long TakenUnits() const { return m_TakenUnits.load(); }
    long Losses() const { return m_Losses.load(); }
    long CountsRaised() const { return m_CountsRaised.load(); }
    long Walks() const { return m_Walks.load(); }
    long PressesRefused() const { return m_PressesRefused.load(); }
    long ConsumeChecks() const { return m_ConsumeChecks.load(); }
    long Mismatches() const { return m_Mismatches.load(); }
    long Refused(CraftMatsRefusal r) const {
        const int i = (int)r;
        return i > 0 && i < (int)CraftMatsRefusal::Count ? m_Refused[i].load() : 0;
    }

    // `craftmats: ON plans=2 with-takes=1 taken=1 (units=3) ...` - a line that
    // names what was done, so "ON and nothing happened" reads as taken=0
    // instead of as silence (guide, Known Limitations item 7).
    std::string StatLine() const {
        return std::string("craftmats: ") + (IsEnabled() ? "ON" : "off")
            + " plans=" + std::to_string(Plans())
            + " with-takes=" + std::to_string(PlansWithTakes())
            + " taken=" + std::to_string(Taken()) + " (units=" + std::to_string(TakenUnits()) + ")"
            + " not-taken=" + std::to_string(Refused(CraftMatsRefusal::NotTaken))
            + " refused(unreadable=" + std::to_string(Refused(CraftMatsRefusal::Unreadable)) + ")"
            + " losses=" + std::to_string(Losses())
            + " while-off=" + std::to_string(WhileOff())
            + " counts-raised=" + std::to_string(CountsRaised())
            + " walks=" + std::to_string(Walks())
            + " stash-unreadable=" + std::to_string(Refused(CraftMatsRefusal::StashUnreadable))
            + " presses-refused=" + std::to_string(PressesRefused())
            + " consume(ok=" + std::to_string(ConsumeChecks()) + " mismatch=" + std::to_string(Mismatches()) + ")"
            + " session=" + (OffThisSession() ? "off" : "ok");
    }

private:
    // One event inside a CraftFindRecipeItems call, in the order the game made it.
    struct FindEvent {
        bool        decode = false;
        int64_t     amount = -1;    // a decode's amount
        std::string material;       // a count's material
        int64_t     game = -1;      // a count's own answer
        bool        stashAdded = false;
    };

    void Lose(const char* why) {
        m_LossReason = why;
        m_Losses.fetch_add(1);
        m_OffThisSession.store(true);
        m_Enabled.store(false);
        if (!m_LossSeen) { m_LossSeen = true; m_LossDue = true; }
    }

    // `craftmats: <what happened> - plans=1 ...`: StatLine's fields, headed by
    // what happened instead of by ON/off.
    std::string HeadedStatLine(const std::string& what) const {
        const std::string stat = StatLine();
        const std::string head = std::string("craftmats: ") + (IsEnabled() ? "ON " : "off ");
        return "craftmats: " + what + " - " + (stat.rfind(head, 0) == 0 ? stat.substr(head.size()) : stat);
    }

    void Note(CraftMatsRefusal r) {
        m_Refused[(int)r].fetch_add(1);
        const unsigned bit = 1u << (int)r;
        if (!(m_ReportedMask & bit)) { m_ReportedMask |= bit; m_Unreported.push_back(r); }
    }

    CraftMatsPlan Refuse(CraftMatsRefusal r) {
        Note(r);
        CraftMatsPlan plan;
        plan.refused = true;
        plan.reason = r;
        return plan;
    }

    std::atomic<bool> m_Enabled{ false };
    std::atomic<bool> m_OffThisSession{ false };
    const char*       m_LossReason = "";

    bool m_FirstTakeSeen = false;
    bool m_FirstTakeDue = false;
    bool m_LossSeen = false;
    bool m_LossDue = false;

    unsigned                      m_ReportedMask = 0;
    std::vector<CraftMatsRefusal> m_Unreported;

    std::atomic<long>      m_Plans{ 0 };
    std::atomic<long>      m_PlansWithTakes{ 0 };
    std::atomic<long>      m_WhileOff{ 0 };
    std::atomic<long>      m_Taken{ 0 };
    std::atomic<long long> m_TakenUnits{ 0 };
    std::atomic<long>      m_Losses{ 0 };
    std::atomic<long>      m_Refused[(int)CraftMatsRefusal::Count]{};

    // The count's walk, kept for the display frames of one game frame.
    CraftMatsWalk m_Walk;
    uint64_t      m_WalkFrame = 0;
    bool          m_WalkKept = false;

    // The needs record of the latest CraftFindRecipeItems call.
    std::vector<FindEvent> m_Events;
    long long              m_FindSelf = -1;
    bool                   m_FindOpen = false;
    bool                   m_FindUsed = false;

    bool        m_MismatchSeen = false;
    bool        m_MismatchDue = false;
    std::string m_MismatchText;

    std::atomic<long> m_CountsRaised{ 0 };
    std::atomic<long> m_Walks{ 0 };
    std::atomic<long> m_PressesRefused{ 0 };
    std::atomic<long> m_ConsumeChecks{ 0 };
    std::atomic<long> m_Mismatches{ 0 };
};

// ---- the kept stash map (Phase 1e) ---------------------------------------------
//
// The stash's item map is reached only as the return of a call the game itself
// makes, GetItemMap with first argument 9 (research doc, Phase 1d); the research
// build's `mapkeep` keeps that return. GameMaker reuses a destroyed map's index,
// so an index that still exists (ds_exists) says nothing about whether it is
// still the stash's map. The rule this type encodes: the kept map is current
// only when the game's own call refreshed it after the latest invalidation - a
// character load or a room change - or since construction. The adapter checks
// ds_exists at the point of use as well, but that is its half: nothing here ever
// takes a ds_exists answer as "current".
//
// Why a kept map is not current, as `mapkeep stat` prints it; None while it is.
// (`ds-gone` is the adapter's own token: the index no longer exists at the read.)
enum class CraftMatsMapReason : int { None = 0, NotKept, CharacterLoaded, RoomChanged };

// Threading: fed from hook bodies inside the game's own calls and from the frame
// callback, and asked from the IPC poll - all on the game thread - so nothing is
// locked.
class CraftMatsKeptMap {
public:
    // The game's own GetItemMap(9) call returned the map at this index.
    void Refreshed(long long index) {
        m_Index = index;
        m_Kept = true;
        m_Reason = CraftMatsMapReason::None;
        ++m_Refreshes;
    }

    // A character load or a room change: whatever is kept is no longer current
    // until the game's own call returns a map again. With nothing kept, "not
    // kept" stays the reason.
    void Invalidate(CraftMatsMapReason why) {
        ++m_Invalidations;
        if (!m_Kept) return;
        m_Reason = why == CraftMatsMapReason::None ? CraftMatsMapReason::NotKept : why;
    }

    // Release what is kept; keeping starts again from the game's next call.
    void Clear() {
        m_Kept = false;
        m_Index = -1;
        m_Reason = CraftMatsMapReason::NotKept;
    }

    bool IsCurrent() const { return m_Kept && m_Reason == CraftMatsMapReason::None; }
    bool IsKept() const { return m_Kept; }
    // The last index kept, current or not; meaningful only while IsKept().
    long long Index() const { return m_Index; }
    CraftMatsMapReason Reason() const { return m_Reason; }
    long Refreshes() const { return m_Refreshes; }
    long Invalidations() const { return m_Invalidations; }

    static const char* ReasonName(CraftMatsMapReason r) {
        switch (r) {
        case CraftMatsMapReason::None:            return "none";
        case CraftMatsMapReason::NotKept:         return "not-kept";
        case CraftMatsMapReason::CharacterLoaded: return "character-loaded";
        case CraftMatsMapReason::RoomChanged:     return "room-changed";
        }
        return "none";
    }

private:
    bool               m_Kept = false;
    long long          m_Index = -1;
    CraftMatsMapReason m_Reason = CraftMatsMapReason::NotKept;
    long               m_Refreshes = 0;
    long               m_Invalidations = 0;
};

} // namespace ForgePact
