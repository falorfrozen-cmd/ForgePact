#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ForgePact::MinerRules {
inline constexpr double Seed = 777003;
inline constexpr double Radius = 192;
inline constexpr unsigned MaxTargets = 2;
struct Node { int64_t id; double x, y, hp, requirement; bool busy, normal; };
inline bool Matches(double type, double seed, double base, double rarity, double subtype) {
    return type == 0 && seed == Seed && base == 7 && rarity == 0 && subtype == 0;
}
// A worn helmet replaces the ore slider with x4; without it (or with the
// helmet mechanic never armed) the slider applies as it always did.
inline int QuantityMultiplier(bool helmetMode, bool worn, int slider) {
    return helmetMode && worn ? 4 : std::clamp(slider, 1, 10);
}
inline std::vector<Node> Nearby(const std::vector<Node>& nodes, int64_t source,
                               double x, double y, double skill) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(skill)) return {};
    std::vector<Node> result;
    for (const auto& node : nodes) {
        if (node.id < 0 || node.id == source || node.busy || !node.normal
            || !std::isfinite(node.hp) || node.hp <= 0
            || !std::isfinite(node.requirement) || node.requirement > skill
            || !std::isfinite(node.x) || !std::isfinite(node.y)) continue;
        const double d = std::hypot(node.x - x, node.y - y);
        if (d > Radius || std::any_of(result.begin(), result.end(),
            [&](const auto& previous) { return previous.id == node.id; })) continue;
        result.push_back(node);
    }
    std::sort(result.begin(), result.end(), [&](const auto& a, const auto& b) {
        const double da = std::hypot(a.x - x, a.y - y), db = std::hypot(b.x - x, b.y - y);
        return da == db ? a.id < b.id : da < db;
    });
    if (result.size() > MaxTargets) result.resize(MaxTargets);
    return result;
}
}
