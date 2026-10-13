#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ForgePact::FlightCharms {

// ---- charmprobe's core (ForgePact #158, phase 1, research build only) -------
//
// docs/flight-charms-research.md asks three things about charms before the
// Flight charms are built: which two itemInfoStruct keys hold an item's grid
// footprint (GetItemSize reads two keys whose names the static reading did
// not render), whether a forged charm can show our own sprite and which key
// holds it, and which unique rarities DropItemHeroic places. ModuleMain.cpp's
// `charmprobe` reads and spawns; this header is everything it decides:
//
// - the size keys, identified from samples of known size: a key holds w (or
//   h) when its value equals that sample's w (h) on every sample, and the
//   pair is identified when exactly one key holds w and exactly one other key
//   holds h across at least two distinct sizes;
// - the sprite key, the same way: a candidate is a key whose value is a
//   sprite (the adapter asks sprite_exists by name), and the key is
//   identified when exactly one key is a candidate on every sample and it
//   took at least two different values; `sprite key <name>` sets it by hand;
// - the argument parsers for `spawn`, `dress`, `sprite`, `heroic` and `repo`,
//   each refusal naming its reason;
// - the dress rules (at most kMaxDressRules, one per definition `a`);
// - the Heroic summary line.
//
// Game-independent by contract - numbers, strings and maps, never an RValue -
// so tests/flight_charm_probe_harness.cpp compiles it whole.

inline constexpr std::string_view kMarker = "fcp1";
inline constexpr int kNormalCharmMaxB = 59;      // Small 0-19, Large 20-39, Grand 40-59 (Item Editor catalog)
inline constexpr int kMaxSide = 10;              // a footprint side, in grid cells
inline constexpr int kMaxStats = 8;              // `stat <id>=<v>` options on one spawn
inline constexpr int kMaxStatId = 999;
inline constexpr size_t kMaxDressRules = 8;
inline constexpr int kMaxHeroicCalls = 20;
inline constexpr long long kSeedBase = 779001;   // research seeds, one per spawn
inline constexpr long long kMaxSeed = 9007199254740991LL;   // 2^53 - 1: a whole number a real holds exactly
inline constexpr size_t kInfoTextMax = 40;       // a string itemInfoStruct value the `info=` dump prints
inline constexpr size_t kKeyNameMax = 40;
inline constexpr size_t kListedKeysMax = 6;      // candidate keys a reason names

// ---- text --------------------------------------------------------------------

// A whole number without a fraction, anything else with up to six significant
// digits; never "nan" or "inf" for a value a test compares.
inline std::string NumberText(double v)
{
    if (!std::isfinite(v)) return "?";
    char b[48];
    if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(b, sizeof b, "%lld", static_cast<long long>(v));
    else std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

// A whole number in [lo, hi], written in plain decimal digits (no sign, no
// fraction, no exponent).
inline bool ParseWhole(std::string_view s, long long lo, long long hi, long long& out)
{
    if (s.empty() || s.size() > 16) return false;
    long long v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    if (v < lo || v > hi) return false;
    out = v;
    return true;
}

// A finite decimal number: optional sign, digits, at most one point.
inline bool ParseNumber(std::string_view s, double& out)
{
    if (s.empty() || s.size() > 24) return false;
    size_t i = 0;
    bool negative = false;
    if (s[0] == '-' || s[0] == '+') { negative = s[0] == '-'; i = 1; }
    if (i >= s.size()) return false;
    double whole = 0, scale = 1;
    bool point = false, digits = false;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '.') { if (point) return false; point = true; continue; }
        if (c < '0' || c > '9') return false;
        digits = true;
        if (point) { scale /= 10; whole += (c - '0') * scale; }
        else whole = whole * 10 + (c - '0');
    }
    if (!digits || !std::isfinite(whole)) return false;
    out = negative ? -whole : whole;
    return true;
}

// An itemInfoStruct key the probe may name: 1..kKeyNameMax letters, digits or
// underscores (the keys the Custom Forge writes are digit strings).
inline bool ValidKeyName(std::string_view s)
{
    if (s.empty() || s.size() > kKeyNameMax) return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) return false;
    return true;
}

// Digit keys in numeric order ("2" before "10"), then the rest by name.
inline bool KeyLess(const std::string& a, const std::string& b)
{
    const auto digits = [](const std::string& s) {
        return !s.empty() && s.size() < 16 && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    const bool da = digits(a), db = digits(b);
    if (da != db) return da;
    if (da) return std::stoll(a) < std::stoll(b);
    return a < b;
}

inline std::string KeyList(const std::set<std::string>& keys)
{
    std::vector<std::string> sorted(keys.begin(), keys.end());
    std::sort(sorted.begin(), sorted.end(), KeyLess);
    std::string out;
    for (size_t i = 0; i < sorted.size() && i < kListedKeysMax; ++i) out += (i ? "," : "") + sorted[i];
    if (sorted.size() > kListedKeysMax) out += ",...";
    return out;
}

// One `info=` dump: key=value pairs in KeyLess order, `;`-separated. A string
// value longer than kInfoTextMax is left out (InfoTextFits), as is every value
// that is neither a number nor such a string (the adapter decides that).
inline bool InfoTextFits(std::string_view s) { return s.size() <= kInfoTextMax; }

inline std::string InfoDump(std::vector<std::pair<std::string, std::string>> entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return KeyLess(a.first, b.first); });
    std::string out;
    for (const auto& [k, v] : entries) out += (out.empty() ? "" : ";") + k + "=" + v;
    return out.empty() ? "none" : out;
}

// ---- the size keys -------------------------------------------------------------

struct Size {
    int w = 0;
    int h = 0;
};

inline bool ValidSize(int w, int h) { return w >= 1 && w <= kMaxSide && h >= 1 && h <= kMaxSide; }
inline std::string SizeText(Size s) { return std::to_string(s.w) + "x" + std::to_string(s.h); }

// An itemInfoStruct's keys whose values are numbers, by key.
using NumberInfo = std::map<std::string, double>;

class SizeKeys {
public:
    // One sample: an item GetItemSize answered w x h for, with its
    // itemInfoStruct's numeric keys. False (and nothing kept) for a size
    // outside 1..kMaxSide.
    bool Add(int w, int h, const NumberInfo& info)
    {
        if (!ValidSize(w, h)) return false;
        std::set<std::string> holdsW, holdsH;
        for (const auto& [key, value] : info) {
            if (value == static_cast<double>(w)) holdsW.insert(key);
            if (value == static_cast<double>(h)) holdsH.insert(key);
        }
        if (m_Samples == 0) { m_W = holdsW; m_H = holdsH; }
        else { Keep(m_W, holdsW); Keep(m_H, holdsH); }
        ++m_Samples;
        m_Sizes.insert({ w, h });
        return true;
    }
    bool Known() const
    {
        return m_Sizes.size() >= 2 && m_W.size() == 1 && m_H.size() == 1 && *m_W.begin() != *m_H.begin();
    }
    std::string W() const { return Known() ? *m_W.begin() : std::string(); }
    std::string H() const { return Known() ? *m_H.begin() : std::string(); }
    int Samples() const { return m_Samples; }
    std::string Why() const
    {
        if (Known()) return "";
        if (m_Samples == 0) return "no samples: `charmprobe charms` with charms of two sizes equipped";
        if (m_Sizes.size() < 2)
            return "one size only (" + SizeText({ m_Sizes.begin()->first, m_Sizes.begin()->second }) + " in "
                + std::to_string(m_Samples) + " sample" + (m_Samples == 1 ? "" : "s") + ")";
        if (m_W.empty()) return "no key holds w on every sample";
        if (m_W.size() > 1) return std::to_string(m_W.size()) + " keys hold w on every sample: " + KeyList(m_W);
        if (m_H.empty()) return "no key holds h on every sample";
        if (m_H.size() > 1) return std::to_string(m_H.size()) + " keys hold h on every sample: " + KeyList(m_H);
        return "w and h are one key (" + *m_W.begin() + ")";
    }
    // `charmprobe sizekeys:`'s text.
    std::string Line() const { return Known() ? "w=" + W() + " h=" + H() : "unknown (" + Why() + ")"; }
    // The bare marker's `sizekeys=` value.
    std::string Pair() const { return Known() ? W() + "," + H() : "unknown"; }
    void Reset() { *this = SizeKeys(); }

private:
    static void Keep(std::set<std::string>& kept, const std::set<std::string>& sample)
    {
        for (auto it = kept.begin(); it != kept.end();) it = sample.count(*it) ? std::next(it) : kept.erase(it);
    }
    int m_Samples = 0;
    std::set<std::string> m_W, m_H;               // keys that held w (h) on every sample so far
    std::set<std::pair<int, int>> m_Sizes;
};

// ---- the sprite key ------------------------------------------------------------

class SpriteKey {
public:
    // One sample: the item's numeric itemInfoStruct keys whose value
    // sprite_exists called a sprite. A sample with none still counts.
    void Add(const NumberInfo& spriteValues)
    {
        if (m_Samples == 0) {
            for (const auto& [key, value] : spriteValues) m_Candidates[key].insert(value);
        } else {
            for (auto it = m_Candidates.begin(); it != m_Candidates.end();) {
                const auto found = spriteValues.find(it->first);
                if (found == spriteValues.end()) { it = m_Candidates.erase(it); continue; }
                it->second.insert(found->second);
                ++it;
            }
        }
        ++m_Samples;
    }
    // `charmprobe sprite key <name>`: the operator's fallback when the rule
    // cannot decide. False for a name ValidKeyName refuses.
    bool SetByHand(const std::string& key)
    {
        if (!ValidKeyName(key)) return false;
        m_Hand = key;
        return true;
    }
    bool ByHand() const { return !m_Hand.empty(); }
    bool Known() const
    {
        if (ByHand()) return true;
        return m_Samples >= 2 && m_Candidates.size() == 1 && m_Candidates.begin()->second.size() >= 2;
    }
    std::string Key() const
    {
        if (ByHand()) return m_Hand;
        return Known() ? m_Candidates.begin()->first : std::string();
    }
    int Samples() const { return m_Samples; }
    std::string Why() const
    {
        if (Known()) return "";
        if (m_Samples == 0) return "no samples: `charmprobe charms` or a spawn";
        if (m_Samples == 1) return "one sample only";
        if (m_Candidates.empty()) return "no key is a sprite on every sample";
        if (m_Candidates.size() > 1) {
            std::set<std::string> keys;
            for (const auto& c : m_Candidates) keys.insert(c.first);
            return std::to_string(keys.size()) + " keys are sprites on every sample: " + KeyList(keys);
        }
        return m_Candidates.begin()->first + " held one value (" + NumberText(*m_Candidates.begin()->second.begin())
            + ") on every sample";
    }
    // The `key=` value of `charmprobe sprite:`.
    std::string Text() const { return Known() ? Key() : "unknown (" + Why() + ")"; }
    void Reset() { *this = SpriteKey(); }

private:
    int m_Samples = 0;
    std::map<std::string, std::set<double>> m_Candidates;   // a sprite on every sample so far, with its values
    std::string m_Hand;
};

// ---- spawn ---------------------------------------------------------------------

struct SpawnRequest {
    int b = -1;
    bool hasSize = false;
    Size size;
    std::vector<std::pair<int, double>> stats;   // item stat id -> value, in the order given
    bool sprite = false;
};

// `spawn <b> [size <w> <h>] [stat <id>=<v>]... [sprite]`, options in any order
// after b; `tok` excludes the word `spawn`.
inline bool ParseSpawn(const std::vector<std::string>& tok, SpawnRequest& out, std::string& why)
{
    out = SpawnRequest();
    if (tok.empty()) { why = "usage: spawn <b> [size <w> <h>] [stat <id>=<v>]... [sprite]"; return false; }
    long long b = -1;
    if (!ParseWhole(tok[0], 0, kNormalCharmMaxB, b)) {
        why = "b must be a whole number 0.." + std::to_string(kNormalCharmMaxB) + " (normal charms), not " + tok[0];
        return false;
    }
    out.b = static_cast<int>(b);
    for (size_t i = 1; i < tok.size(); ++i) {
        const std::string& word = tok[i];
        if (word == "size") {
            if (out.hasSize) { why = "size given twice"; return false; }
            long long w = 0, h = 0;
            if (i + 2 >= tok.size() || !ParseWhole(tok[i + 1], 1, kMaxSide, w) || !ParseWhole(tok[i + 2], 1, kMaxSide, h)) {
                why = "size needs two whole numbers 1.." + std::to_string(kMaxSide);
                return false;
            }
            out.hasSize = true;
            out.size = { static_cast<int>(w), static_cast<int>(h) };
            i += 2;
        } else if (word == "stat") {
            if (i + 1 >= tok.size()) { why = "stat needs <id>=<value>"; return false; }
            const std::string& pair = tok[i + 1];
            const size_t eq = pair.find('=');
            long long id = -1;
            double value = 0;
            if (eq == std::string::npos || !ParseWhole(std::string_view(pair).substr(0, eq), 0, kMaxStatId, id)
                || !ParseNumber(std::string_view(pair).substr(eq + 1), value)) {
                why = "stat needs <id>=<value>, id a whole number 0.." + std::to_string(kMaxStatId) + ", not " + pair;
                return false;
            }
            for (const auto& s : out.stats)
                if (s.first == id) { why = "stat " + std::to_string(id) + " given twice"; return false; }
            if (static_cast<int>(out.stats.size()) >= kMaxStats) {
                why = "at most " + std::to_string(kMaxStats) + " stats";
                return false;
            }
            out.stats.push_back({ static_cast<int>(id), value });
            i += 1;
        } else if (word == "sprite") {
            if (out.sprite) { why = "sprite given twice"; return false; }
            out.sprite = true;
        } else {
            why = "unknown option " + word + " (size, stat or sprite)";
            return false;
        }
    }
    return true;
}

// The research seed of spawn number n (0-based), and its loader inputs: the
// key `0-0-<ms>-10` (10 being the charm class) and the definition json the
// signature items' spawn uses.
inline long long SpawnSeed(long n) { return kSeedBase + n; }
inline std::string SpawnKey(long long ms) { return "0-0-" + std::to_string(ms) + "-10"; }
inline std::string SpawnJson(long long seed, int b)
{
    return "{\"w\":1,\"a\":" + std::to_string(seed) + ",\"j\":0,\"b\":" + std::to_string(b) + ",\"c\":0,\"o\":1}";
}
inline std::string StatsText(const std::vector<std::pair<int, double>>& stats)
{
    if (stats.empty()) return "none";
    std::string out;
    for (const auto& [id, v] : stats) out += (out.empty() ? "" : ",") + std::to_string(id) + "=" + NumberText(v);
    return out;
}

// ---- dress ---------------------------------------------------------------------

struct DressRule {
    long long a = 0;
    Size size;
};

enum class DressForm { Show, Add, Off };

struct DressRequest {
    DressForm form = DressForm::Show;
    DressRule rule;
};

// `dress` (show), `dress off`, `dress <a> <w> <h>`; `tok` excludes `dress`.
inline bool ParseDress(const std::vector<std::string>& tok, DressRequest& out, std::string& why)
{
    out = DressRequest();
    if (tok.empty()) return true;
    if (tok.size() == 1 && tok[0] == "off") { out.form = DressForm::Off; return true; }
    long long a = -1, w = 0, h = 0;
    if (tok.size() != 3) { why = "usage: dress <a> <w> <h> | dress off"; return false; }
    if (!ParseWhole(tok[0], 0, kMaxSeed, a)) { why = "a must be a whole number (the item definition's seed), not " + tok[0]; return false; }
    if (!ParseWhole(tok[1], 1, kMaxSide, w) || !ParseWhole(tok[2], 1, kMaxSide, h)) {
        why = "w and h must be whole numbers 1.." + std::to_string(kMaxSide);
        return false;
    }
    out.form = DressForm::Add;
    out.rule = { a, { static_cast<int>(w), static_cast<int>(h) } };
    return true;
}

class DressRules {
public:
    // A rule for an `a` already ruled replaces it; a new one past
    // kMaxDressRules is refused.
    bool Add(const DressRule& rule, std::string& why)
    {
        for (DressRule& r : m_Rules)
            if (r.a == rule.a) { r = rule; return true; }
        if (m_Rules.size() >= kMaxDressRules) {
            why = std::to_string(kMaxDressRules) + " rules already (`dress off` clears them)";
            return false;
        }
        m_Rules.push_back(rule);
        return true;
    }
    void Clear() { m_Rules.clear(); }
    // The rule for a built item's definition `a`, or nullptr. Only a whole
    // number can match: an unreadable `a` never equals a rule.
    const DressRule* Find(double a) const
    {
        if (!std::isfinite(a) || a != std::floor(a)) return nullptr;
        for (const DressRule& r : m_Rules)
            if (static_cast<double>(r.a) == a) return &r;
        return nullptr;
    }
    size_t Count() const { return m_Rules.size(); }
    bool Empty() const { return m_Rules.empty(); }
    void NoteDressed() { ++m_Dressed; }
    long Dressed() const { return m_Dressed; }
    // `charmprobe dress:`'s text: the rules, then the items dressed.
    std::string Line() const
    {
        std::string rules;
        for (const DressRule& r : m_Rules) rules += (rules.empty() ? "" : ",") + std::to_string(r.a) + ":" + SizeText(r.size);
        return "rules=" + std::to_string(m_Rules.size()) + " [" + (rules.empty() ? std::string("none") : rules)
            + "] dressed=" + std::to_string(m_Dressed);
    }

private:
    std::vector<DressRule> m_Rules;
    long m_Dressed = 0;
};

// ---- sprite --------------------------------------------------------------------

enum class SpriteForm { Show, Load, Key };

struct SpriteRequest {
    SpriteForm form = SpriteForm::Show;
    std::string key;
};

// `sprite`, `sprite load`, `sprite key <name>`; `tok` excludes `sprite`.
inline bool ParseSprite(const std::vector<std::string>& tok, SpriteRequest& out, std::string& why)
{
    out = SpriteRequest();
    if (tok.empty()) return true;
    if (tok.size() == 1 && tok[0] == "load") { out.form = SpriteForm::Load; return true; }
    if (tok[0] == "key") {
        if (tok.size() != 2 || !ValidKeyName(tok[1])) {
            why = "sprite key needs one key name: 1.." + std::to_string(kKeyNameMax) + " letters, digits or _";
            return false;
        }
        out.form = SpriteForm::Key;
        out.key = tok[1];
        return true;
    }
    why = "usage: sprite | sprite load | sprite key <name>";
    return false;
}

// ---- heroic and repo -----------------------------------------------------------

// `heroic <n>`, n 1..kMaxHeroicCalls; `tok` excludes `heroic`.
inline bool ParseHeroic(const std::vector<std::string>& tok, int& n, std::string& why)
{
    long long v = 0;
    if (tok.size() != 1 || !ParseWhole(tok[0], 1, kMaxHeroicCalls, v)) {
        why = "usage: heroic <n>, n a whole number 1.." + std::to_string(kMaxHeroicCalls);
        return false;
    }
    n = static_cast<int>(v);
    return true;
}

struct RepoRequest {
    int type = -1;
    int sub = -1;
    int b = -1;
};

// `repo <t> <sub> <b>`; `tok` excludes `repo`.
inline bool ParseRepo(const std::vector<std::string>& tok, RepoRequest& out, std::string& why)
{
    long long t = -1, s = -1, b = -1;
    if (tok.size() != 3 || !ParseWhole(tok[0], 0, 99, t) || !ParseWhole(tok[1], 0, 99, s) || !ParseWhole(tok[2], 0, 9999, b)) {
        why = "usage: repo <t> <sub> <b>, whole numbers (t, sub 0..99, b 0..9999)";
        return false;
    }
    out = { static_cast<int>(t), static_cast<int>(s), static_cast<int>(b) };
    return true;
}

// The calls `heroic <n>` made: how many placed a new ground item, the placed
// items by rarity code ("27"; `?` when unreadable) and how many were charms.
class HeroicTally {
public:
    void Add(bool placed, bool rarityRead, int rarity, bool charm)
    {
        ++m_Calls;
        if (!placed) return;
        ++m_Placed;
        if (rarityRead) ++m_ByRarity[rarity];
        else ++m_Unread;
        if (charm) ++m_Charms;
    }
    int Calls() const { return m_Calls; }
    int Placed() const { return m_Placed; }
    // The summary's text after `charmprobe heroic: `.
    std::string Line() const
    {
        std::string rarity;
        for (const auto& [code, count] : m_ByRarity) rarity += (rarity.empty() ? "" : ",") + std::to_string(code) + ":" + std::to_string(count);
        if (m_Unread) rarity += (rarity.empty() ? "" : ",") + std::string("?:") + std::to_string(m_Unread);
        return "calls=" + std::to_string(m_Calls) + " placed=" + std::to_string(m_Placed) + " rarity="
            + (rarity.empty() ? std::string("none") : rarity) + " charms=" + std::to_string(m_Charms);
    }

private:
    int m_Calls = 0, m_Placed = 0, m_Unread = 0, m_Charms = 0;
    std::map<int, int> m_ByRarity;
};

// ---- the bare marker -----------------------------------------------------------

inline std::string MarkerLine(const SizeKeys& size, const DressRules& dress)
{
    return "charmprobe: research build marker=" + std::string(kMarker) + " sizekeys=" + size.Pair()
        + " dress=" + std::to_string(dress.Count());
}

} // namespace ForgePact::FlightCharms
