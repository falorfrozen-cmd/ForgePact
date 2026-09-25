#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Gems of Incarnation: every new drop Mythic, with the mods the player ticked,
// every affix at its best tier's top.
//
// Game-independent by contract: this header names no runtime interface. The
// adapter in ModuleMain reads the item struct, asks these functions what to do
// and writes the answer back; tests/incarnation_gems_harness.cpp drives them
// with no game. The facts (docs/incarnation-gems-research.md, all measured
// 2026-09-25 on build pe-6aaa6779-0cad4fc8 by having the game build 14,521 gems):
//   - A Gem of Incarnation is socketable base 136 (c 0, j 0). The game rolls it
//     from the definition alone: seed `a` and the drop's `n`.
//   - Rarity decides the affix count: Superior 1-3, Rare 3-4, Mythic (5) 4-5.
//   - Each affix sits in stat slot "10".."14" as [stat, min, max, tier]. Every
//     (stat, tier) pair has exactly one range, whatever `n` is, and tier 4 is
//     the best one.
//   - `n` is a table index, not a scale (5 and 8 behave like no `n`), so the
//     mod never invents one: a drop keeps its own `n` and only its seed changes.
namespace ForgePact::IncarnationGems {

inline constexpr int kGemBase = 136;
inline constexpr int kSocketableType = 15;   // HeroSiege::Items::ItemType::Socketable
inline constexpr int kMythicRarity = 5;      // itemInfoStruct["27"]
inline constexpr int kBestTier = 4;
// Mythic seeds kept per `n`. Enough that a filter on the rarest mod still finds
// some: +All Skills is on 0.7% of Mythic gems, 3-4 of 512 at every `n` measured.
inline constexpr size_t kSeedsPerN = 512;
inline constexpr uint32_t kMaxSeed = 2147483647u;   // real seeds are below 2^31
inline constexpr const char* kAffixSlots[] = { "10", "11", "12", "13", "14" };
// Affix stats whose value names something rather than measuring it: the skill a
// skill grant gives (462), and a class (21). Their "max" would be another skill.
inline bool IsIdentifierStat(int stat) { return stat == 462 || stat == 21; }
// A skill grant is two slots: the skill (462) and its levels (463). The filter
// and the tables name it by the skill slot only.
inline constexpr int kSkillGrantLevels = 463;

// The definition fields that identify a Gem of Incarnation.
inline bool IsGem(double itemType, double base, double c)
{
    return itemType == kSocketableType && base == kGemBase && c == 0;
}

// A drop's `n` as a table key: absent is its own key, like every value seen.
inline std::string NKey(std::optional<double> n)
{
    if (!n || !std::isfinite(*n) || *n != std::floor(*n) || *n < -1000000 || *n > 1000000) return "none";
    return std::to_string(static_cast<long long>(*n));
}

struct Affix {
    int stat = 0;
    double min = 0, max = 0, tier = 0;
    double value = 0;
};

// One affix at its best: the best tier's range and its top value. An unknown
// best range keeps the affix's own range and takes its top. Identifier stats
// are never touched. Returns true when anything changed.
inline bool MaxRoll(std::vector<Affix>& affixes, const std::map<int, std::pair<double, double>>& best)
{
    bool changed = false;
    for (Affix& a : affixes) {
        if (IsIdentifierStat(a.stat)) continue;
        Affix next = a;
        const auto it = best.find(a.stat);
        if (it != best.end() && it->second.second >= it->second.first) {
            next.min = it->second.first;
            next.max = it->second.second;
            next.tier = kBestTier;
        }
        next.value = next.max;
        if (next.min != a.min || next.max != a.max || next.tier != a.tier || next.value != a.value) {
            a = next;
            changed = true;
        }
    }
    return changed;
}

// splitmix64: the candidate seeds for one (build, n) are a fixed sequence, so a
// table that was half built resumes where it stopped.
inline uint64_t Mix(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline uint64_t Hash(const std::string& text)
{
    uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : text) { h ^= ch; h *= 1099511628211ull; }
    return h;
}

inline uint32_t Candidate(const std::string& build, const std::string& nKey, uint64_t index)
{
    const uint64_t x = Mix(Hash(build) ^ Mix(Hash(nKey) + index));
    return static_cast<uint32_t>(x % kMaxSeed) + 1u;
}

// The mods a player wants on a gem: affix stats (a skill grant by 462). Empty
// means every mod - no filter.
using Filter = std::set<int>;

// `gemfilter all` or `gemfilter 28,68,...` (at most 64 distinct stats).
inline std::optional<Filter> ParseFilter(const std::string& text)
{
    if (text == "all") return Filter{};
    Filter out;
    std::stringstream in(text);
    std::string part;
    while (std::getline(in, part, ',')) {
        if (part.empty() || part.size() > 5) return std::nullopt;
        for (char ch : part) if (ch < '0' || ch > '9') return std::nullopt;
        const int stat = std::stoi(part);
        if (stat == kSkillGrantLevels) continue;   // the grant is named by its skill slot
        out.insert(stat);
        if (out.size() > 64) return std::nullopt;
    }
    if (out.empty()) return std::nullopt;
    return out;
}

// A Mythic seed and the affix stats the game gave it (a skill grant by 462).
struct Seed {
    uint32_t value = 0;
    std::vector<int> stats;
};

// How many of a seed's mods the filter wants.
inline int Score(const Seed& seed, const Filter& filter)
{
    int score = 0;
    for (int stat : seed.stats) score += filter.count(stat) ? 1 : 0;
    return score;
}

// Seeds the game itself rolled Mythic, per drop `n`, and the best tier's range
// per affix stat - all for one game build.
struct Tables {
    struct Row { uint64_t tried = 0; std::vector<Seed> seeds; };
    std::string build;
    std::map<std::string, Row> rows;                      // n key -> row
    std::map<int, std::pair<double, double>> best;        // stat -> best tier range
    uint64_t conflicts = 0;                                // a second range for one (stat, best tier)
    bool dirty = false;

    // A key is built once it holds kSeedsPerN seeds. Keys join when first needed.
    void Plan(const std::string& nKey)
    {
        if (rows.emplace(nKey, Row{}).second) dirty = true;
    }
    bool Complete(const std::string& nKey) const
    {
        const auto it = rows.find(nKey);
        return it != rows.end() && it->second.seeds.size() >= kSeedsPerN;
    }
    // The planned key with the fewest seeds, or "" when all are complete.
    std::string NextWork() const
    {
        std::string pick;
        size_t fewest = kSeedsPerN;
        for (const auto& [key, row] : rows)
            if (row.seeds.size() < fewest) { fewest = row.seeds.size(); pick = key; }
        return pick;
    }
    // The next candidate for a key: the seed to build, its place in the sequence.
    uint32_t NextCandidate(const std::string& nKey) const
    {
        const auto it = rows.find(nKey);
        return Candidate(build, nKey, it == rows.end() ? 0 : it->second.tried);
    }
    // What the game built for a candidate: its rarity and its affixes.
    void Learn(const std::string& nKey, uint32_t seed, double rarity, const std::vector<Affix>& affixes)
    {
        Row& row = rows[nKey];
        ++row.tried;
        dirty = true;
        const bool known = std::any_of(row.seeds.begin(), row.seeds.end(), [&](const Seed& s) { return s.value == seed; });
        if (rarity == kMythicRarity && row.seeds.size() < kSeedsPerN && !known) {
            Seed kept;
            kept.value = seed;
            for (const Affix& a : affixes) if (a.stat != kSkillGrantLevels) kept.stats.push_back(a.stat);
            row.seeds.push_back(kept);
        }
        for (const Affix& a : affixes) {
            if (a.tier != kBestTier || a.max < a.min) continue;
            const auto found = best.find(a.stat);
            if (found == best.end()) best.emplace(a.stat, std::make_pair(a.min, a.max));
            else if (found->second != std::make_pair(a.min, a.max)) ++conflicts;
        }
    }
    // A Mythic seed for a fresh drop at this `n`: the ones carrying the most
    // wanted mods, one of them by `roll`. `missed` says the filter matched none
    // (the drop then takes any Mythic seed). None while the key has no seeds.
    std::optional<uint32_t> Pick(const std::string& nKey, uint64_t roll, const Filter& filter = {}, bool* missed = nullptr) const
    {
        if (missed) *missed = false;
        const auto it = rows.find(nKey);
        if (it == rows.end() || it->second.seeds.empty()) return std::nullopt;
        const std::vector<Seed>& seeds = it->second.seeds;
        if (filter.empty()) return seeds[roll % seeds.size()].value;
        int top = 0;
        for (const Seed& s : seeds) if (const int score = Score(s, filter); score > top) top = score;
        if (top == 0) {
            if (missed) *missed = true;
            return seeds[roll % seeds.size()].value;
        }
        std::vector<uint32_t> bestSeeds;
        for (const Seed& s : seeds) if (Score(s, filter) == top) bestSeeds.push_back(s.value);
        return bestSeeds[roll % bestSeeds.size()];
    }
};

inline std::string Number(double v)
{
    char text[40];
    std::snprintf(text, sizeof text, "%.15g", v);
    return text;
}

// {"schema":2,"build":"...","rows":{"4":{"tried":9,"seeds":[[seed,stat,..],..]}},"best":{"68":[3,12]}}
inline std::string Serialize(const Tables& t)
{
    std::string out = "{\"schema\":2,\"build\":\"";
    for (char ch : t.build) if (ch != '"' && ch != '\\' && static_cast<unsigned char>(ch) >= 0x20) out += ch;
    out += "\",\"rows\":{";
    bool first = true;
    for (const auto& [key, row] : t.rows) {
        if (!first) out += ',';
        first = false;
        out += "\"" + key + "\":{\"tried\":" + std::to_string(row.tried) + ",\"seeds\":[";
        for (size_t i = 0; i < row.seeds.size(); ++i) {
            out += (i ? ",[" : "[") + std::to_string(row.seeds[i].value);
            for (int stat : row.seeds[i].stats) out += "," + std::to_string(stat);
            out += "]";
        }
        out += "]}";
    }
    out += "},\"best\":{";
    first = true;
    for (const auto& [stat, range] : t.best) {
        if (!first) out += ',';
        first = false;
        out += "\"" + std::to_string(stat) + "\":[" + Number(range.first) + "," + Number(range.second) + "]";
    }
    out += "}}";
    return out;
}

// A small reader for exactly what Serialize writes. Anything else - another
// schema, another build, a damaged file - gives empty tables for `build`.
namespace detail {
struct Reader {
    const std::string& s;
    size_t i = 0;
    bool ok = true;
    explicit Reader(const std::string& text) : s(text) {}
    void Space() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    bool Eat(char c) { Space(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
    void Need(char c) { if (!Eat(c)) ok = false; }
    std::string Str()
    {
        Need('"');
        std::string out;
        while (ok && i < s.size() && s[i] != '"') {
            if (s[i] == '\\') { ok = false; break; }
            out += s[i++];
        }
        Need('"');
        return out;
    }
    double Num()
    {
        Space();
        const size_t start = i;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) ++i;
        if (start == i) { ok = false; return 0; }
        try { return std::stod(s.substr(start, i - start)); } catch (...) { ok = false; return 0; }
    }
};
} // namespace detail

inline Tables Parse(const std::string& text, const std::string& build)
{
    Tables empty;
    empty.build = build;
    Tables t;
    detail::Reader r(text);
    r.Need('{');
    bool sawSchema = false;
    while (r.ok && !r.Eat('}')) {
        const std::string key = r.Str();
        r.Need(':');
        if (key == "schema") { sawSchema = r.Num() == 2; }
        else if (key == "build") { t.build = r.Str(); }
        else if (key == "rows") {
            r.Need('{');
            while (r.ok && !r.Eat('}')) {
                const std::string nKey = r.Str();
                r.Need(':');
                r.Need('{');
                Tables::Row row;
                while (r.ok && !r.Eat('}')) {
                    const std::string field = r.Str();
                    r.Need(':');
                    if (field == "tried") row.tried = static_cast<uint64_t>(r.Num());
                    else if (field == "seeds") {
                        r.Need('[');
                        while (r.ok && !r.Eat(']')) {
                            r.Need('[');
                            Seed seed;
                            bool first = true, valid = true;
                            while (r.ok && !r.Eat(']')) {
                                const double v = r.Num();
                                if (first) {
                                    valid = v >= 1 && v <= kMaxSeed && v == std::floor(v);
                                    seed.value = valid ? static_cast<uint32_t>(v) : 0;
                                    first = false;
                                } else if (v >= 0 && v <= 100000 && v == std::floor(v) && seed.stats.size() < 5) {
                                    seed.stats.push_back(static_cast<int>(v));
                                } else {
                                    valid = false;
                                }
                                r.Eat(',');
                            }
                            if (valid && !first && row.seeds.size() < kSeedsPerN) row.seeds.push_back(seed);
                            r.Eat(',');
                        }
                    } else r.ok = false;
                    r.Eat(',');
                }
                if (nKey.empty() || nKey.size() > 16) r.ok = false;
                t.rows[nKey] = row;
                r.Eat(',');
            }
        } else if (key == "best") {
            r.Need('{');
            while (r.ok && !r.Eat('}')) {
                const std::string stat = r.Str();
                r.Need(':');
                r.Need('[');
                const double lo = r.Num();
                r.Need(',');
                const double hi = r.Num();
                r.Need(']');
                int id = -1;
                try { id = std::stoi(stat); } catch (...) { r.ok = false; }
                if (r.ok && hi >= lo) t.best[id] = { lo, hi };
                r.Eat(',');
            }
        } else r.ok = false;
        r.Eat(',');
    }
    if (!r.ok || !sawSchema || t.build != build) return empty;
    t.dirty = false;
    return t;
}

// The switches, the filter and what they did this session.
struct State {
    bool mythic = false;
    bool maxRoll = false;
    Filter filter;   // empty: every mod
    uint64_t swaps = 0, vanillaDrops = 0, dressed = 0, filterMisses = 0;
    bool loggedSwap = false, loggedVanilla = false, loggedDress = false, loggedFilterMiss = false;
};

// What a fresh drop's seed should become: a Mythic seed for its `n` carrying as
// many of the wanted mods as any, or none (the switch is off, it is not a gem,
// or its `n` has no seeds yet - the caller then plans that key so the next drop
// can have one). `missed`: no Mythic seed for this `n` has a wanted mod.
inline std::optional<uint32_t> DropSeed(const State& state, const Tables& tables, bool freshDrop,
                                        double itemType, double base, double c, const std::string& nKey, uint64_t roll,
                                        bool* missed = nullptr)
{
    if (missed) *missed = false;
    if (!state.mythic || !freshDrop || !IsGem(itemType, base, c)) return std::nullopt;
    return tables.Pick(nKey, roll, state.filter, missed);
}

// What a finished gem's affixes should become whenever the game builds one -
// a drop, a load, anything: MaxRoll when the switch is on. False: leave it.
inline bool Dress(const State& state, double itemType, double base, double c,
                  std::vector<Affix>& affixes, const Tables& tables)
{
    if (!state.maxRoll || !IsGem(itemType, base, c)) return false;
    return MaxRoll(affixes, tables.best);
}

} // namespace ForgePact::IncarnationGems
