#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ForgePact {

// Crafting from the stash's material tab (ForgePact issue #14). While the mod
// is on, a recipe at the game's own Crafting Cube may use materials held in the
// shared stash's purchasable material tab, without the player first moving
// them to the character's materials tab. Owner's decisions (2026-09-22,
// docs/crafting-materials-research.md): count AND consume; bag first, then the
// stash tab, which supplies only the shortfall; ordinary stash grid tabs are
// never a source.
//
// How the game counts and consumes a recipe's materials is not measured yet
// (the research doc's Phase 1), so this header is only the arithmetic every
// hypothesis shares - per material, need N, bag count k, stash-tab count s:
// take min(N-k, s) from the stash tab only when the switch is on and k < N -
// plus what to refuse, what counts as a loss, and what to say. It is
// game-independent on purpose: it names no runtime interface, builtin, log
// call or runtime value type (test_craft_mats_contract.py checks the
// spellings), so tests/craft_mats_harness.cpp compiles it whole with no
// runtime stub. The adapter the owner's chosen mechanism needs (hook or pull,
// research doc § Hypotheses) re-reads the counts at the press, calls Plan, runs
// each take through the game's own routine by name, and reports it with
// OnTakeReport. Until that adapter exists, the switch changes nothing.

// Where a take comes from. There is exactly one source, and that is the
// design: nothing an adapter can pass names an ordinary stash grid tab, the
// guild stash or any other container, so no input can make the core plan a
// take from one.
enum class CraftMatsSource : int { StashMaterialTab = 1 };

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

// Why a press or a take was not served. Each is named once per session.
enum class CraftMatsRefusal : int {
    None = 0,
    Unreadable,   // a count the decision needed could not be read; nothing was taken
    NotTaken,     // the game declined a take and the stash tab is unchanged; nothing was taken
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
        m_LossReason = r.success
            ? "the game answered success but the stash tab's re-read could not confirm the take"
            : "the stash tab changed, or could not be read, without the game answering success";
        m_Losses.fetch_add(1);
        m_OffThisSession.store(true);
        m_Enabled.store(false);
        if (!m_LossSeen) { m_LossSeen = true; m_LossDue = true; }
        return CraftMatsOutcome::Loss;
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
        case CraftMatsRefusal::Unreadable: return "unreadable";
        case CraftMatsRefusal::NotTaken:   return "not-taken";
        default:                           return "none";
        }
    }

    // The one line a refusal is reported with: what the mod did.
    std::string RefusalLine(CraftMatsRefusal r) const {
        const std::string head = std::string("craftmats: ") + RefusalName(r) + " - ";
        switch (r) {
        case CraftMatsRefusal::Unreadable:
            return head + "a material count could not be read; nothing was taken from the stash tab and the craft is left to the game";
        case CraftMatsRefusal::NotTaken:
            return head + "the game declined a take from the stash tab, which is unchanged; nothing was taken";
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
            + " session=" + (OffThisSession() ? "off" : "ok");
    }

private:
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
};

} // namespace ForgePact
