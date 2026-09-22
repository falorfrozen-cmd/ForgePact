#pragma once
#include <cstdio>
#include <string>
// Included after the existing item factory, player resolver and instance resolver.
// Original adapter code; the game still creates, hashes, drops and saves the item.
namespace ForgePact::MinerHelmet {
struct Wave { double x, y, born; int64_t room; };
inline std::vector<Wave> waves;
inline std::vector<std::pair<int64_t, int64_t>> recentNodes;
inline std::string equipmentReason = "No helmet loaded";
inline double lastStatus = 0, lastGrant = -10000;
inline std::vector<std::string> grantRequests;

// Ownership helpers for RewardMultiplier (below). They sit above the equipment
// reader because tests/test_miner_helmet_behavior.py compiles the reader alone
// against a smaller fake runner.
inline constexpr double kImplicitDigRadius = 400;
// GameMaker instance ids start at 100000; a smaller real is an object index or
// a sentinel such as noone, and must not reach instance_exists as an instance.
inline constexpr double kFirstInstanceId = 100000;

inline bool InstanceNumber(const RValue& value, double& number) {
    if (value.m_Kind != VALUE_REF && value.m_Kind != VALUE_REAL
        && value.m_Kind != VALUE_INT32 && value.m_Kind != VALUE_INT64) return false;
    number = value.ToDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

// A live instance id read the runner's own way: `id` is a reference on this
// runner and ToDouble() yields the number (same read as ModuleMain's PpInstanceId).
inline bool LiveInstanceId(const RValue& instance, double& number) {
    double raw = -1;
    if (instance.m_Kind != VALUE_REF && instance.m_Kind != VALUE_OBJECT) {
        if (!InstanceNumber(instance, raw) || raw < kFirstInstanceId) return false;
    }
    if (!g_Yytk->CallBuiltin("instance_exists", {instance}).ToBoolean()) return false;
    RValue id = g_Yytk->CallBuiltin("variable_instance_get", {instance, RValue("id")});
    return InstanceNumber(id, number) && number > 0;
}

inline bool Position(const RValue& instance, double& x, double& y) {
    x = g_Yytk->CallBuiltin("variable_instance_get", {instance, RValue("x")}).ToDouble();
    y = g_Yytk->CallBuiltin("variable_instance_get", {instance, RValue("y")}).ToDouble();
    return std::isfinite(x) && std::isfinite(y);
}

// Timed dig probe (`minerhelm probe [seconds]`): while armed, every sixth frame
// it prints the state of the mining node nearest the player, only when that
// state changed, so one real dig records how the game drives a node from
// idle to reward. Bounded by time and by line count; touches nothing.
inline double probeUntil = 0;
inline unsigned probeLines = 0, probeFrames = 0, probeRewardLines = 0;
inline std::string probeLast;
inline constexpr unsigned kProbeMaxLines = 400, kProbeRewardMaxLines = 8;

inline std::string DescribeValue(const RValue& v) {
    if (v.m_Kind == VALUE_UNDEFINED) return "undef";
    if (v.m_Kind == VALUE_BOOL) return v.ToBoolean() ? "true" : "false";
    if (v.m_Kind == VALUE_STRING) return "\"" + v.ToString() + "\"";
    double d = 0;
    if (InstanceNumber(v, d) || v.m_Kind == VALUE_REAL) {
        char buf[48]; std::snprintf(buf, sizeof buf, "%g", v.ToDouble());
        return std::string(buf) + (v.m_Kind == VALUE_REF ? "r" : "");
    }
    return "k" + std::to_string(v.m_Kind);
}

// `changeKey`, when given, receives the line without the player distance, so a
// walking player does not count as a change of node state.
inline std::string DescribeNode(const RValue& node, const RValue& player, std::string* changeKey = nullptr) {
    std::string line;
    for (const char* name : {"miningActive", "miningPlayer", "stop", "range", "rangeMax", "dir", "miningQue", "hp", "miningActivateDistance", "sliderSpeed"}) {
        try {
            if (!g_Yytk->CallBuiltin("variable_instance_exists", {node, RValue(name)}).ToBoolean()) { line += std::string(name) + "=- "; continue; }
            line += std::string(name) + "=" + DescribeValue(g_Yytk->CallBuiltin("variable_instance_get", {node, RValue(name)})) + " ";
        } catch (...) { line += std::string(name) + "=! "; }
    }
    if (changeKey) *changeKey = line;
    try {
        double px, py, nx, ny;
        if (Position(player, px, py) && Position(node, nx, ny)) {
            char buf[64]; std::snprintf(buf, sizeof buf, "dist=%.0f node=(%.0f,%.0f)", std::hypot(px - nx, py - ny), nx, ny);
            line += buf;
            if (changeKey) { std::snprintf(buf, sizeof buf, " node=(%.0f,%.0f)", nx, ny); *changeKey += buf; }
        }
    } catch (...) {}
    return line;
}

inline void ProbeTick() {
    if (probeUntil <= 0) return;
    const double now = HhNowMs();
    if (!std::isfinite(now) || now > probeUntil || probeLines >= kProbeMaxLines) {
        probeUntil = 0; probeLast.clear();
        Out("minerhelm: probe finished (" + std::to_string(probeLines) + " lines)");
        return;
    }
    if ((++probeFrames % 6) != 0) return;
    try {
        RValue player;
        if (!HhResolveLocalPlayer(player)) return;
        double px, py;
        if (!Position(player, px, py)) return;
        const RValue object = g_Yytk->CallBuiltin("asset_get_index", {RValue("Mining_Node_obj")});
        if (object.ToDouble() < 0) { probeUntil = 0; Out("minerhelm: probe stopped - Mining_Node_obj unknown"); return; }
        const RValue node = g_Yytk->CallBuiltin("instance_nearest", {RValue(px), RValue(py), object});
        double id = -1;
        if (!LiveInstanceId(node, id)) return;
        std::string key;
        const std::string line = DescribeNode(node, player, &key);
        key = std::to_string((long long)id) + " " + key;
        if (key == probeLast) return;
        probeLast = key; ++probeLines;
        Out("minerhelm probe: node " + std::to_string((long long)id) + " " + line);
    } catch (...) {}
}

// ===== Vein Resonance =====
// A finished dig also finishes up to MinerRules::MaxTargets eligible veins
// within MinerRules::Radius of the finished node. It creates no loot itself.
// The live probe of 2026-09-23 showed how the game digs: the interact press,
// or the node's own `miningQue` flag, sends the node through its level check
// and straight to the reward in that same step (`miningActive` true, `range`
// at `rangeMax`, `stop` 0), provided the player stands within the node's
// `miningActivateDistance` (16 px). So each chosen vein gets `miningQue`
// raised and its activate distance widened for a few frames; its next step
// then runs the game's own completion - hit effect, ore, mining XP, quests,
// depletion (`hp` 0) and the network message - and the ore hook applies 4x
// as for any dig. A vein that has not completed within kResonanceFrames is
// released unchanged. A vein finished this way never starts a chain of its own.
struct Resonance { int64_t room, id; unsigned frame; double savedDistance; RValue inst; };
inline std::vector<Resonance> resonating;
inline bool veinResonance = true;
inline unsigned resonanceFrame = 0, resonanceStarted = 0, resonanceExpired = 0, resonanceLogs = 0;
inline constexpr unsigned kResonanceFrames = 90;
inline constexpr double kResonanceReach = 4096;
inline constexpr int kMaxNodesScanned = 512;

inline bool RealNumber(const RValue& v, double& d) {
    if (v.m_Kind != VALUE_REAL && v.m_Kind != VALUE_INT32 && v.m_Kind != VALUE_INT64 && v.m_Kind != VALUE_BOOL) return false;
    d = v.ToDouble();
    return std::isfinite(d);
}

// The local character's mining level, through the game's own script; NaN when
// it cannot be read, which queues nothing.
inline double MiningLevel() {
    try {
        RValue r = g_Yytk->CallGameScript("gml_Script_GetMiningLevel", {});
        double d;
        if (RealNumber(r, d) && d >= 0) return d;
    } catch (...) {}
    return NAN;
}

// `miningReq` holds a handle into the game's protected store; GPV resolves it.
// NaN (unreadable) makes the selection skip that vein.
inline double NodeRequirement(const RValue& node) {
    try {
        if (!g_Yytk->CallBuiltin("variable_instance_exists", {node, RValue("miningReq")}).ToBoolean()) return NAN;
        RValue handle = g_Yytk->CallBuiltin("variable_instance_get", {node, RValue("miningReq")});
        RValue r = g_Yytk->CallGameScript("gml_Script_GPV", {handle});
        double d;
        if (RealNumber(r, d) && d >= 0) return d;
    } catch (...) {}
    return NAN;
}

inline double NodeNumber(const RValue& node, const char* name, double fallback) {
    try {
        if (!g_Yytk->CallBuiltin("variable_instance_exists", {node, RValue(name)}).ToBoolean()) return fallback;
        double d;
        if (RealNumber(g_Yytk->CallBuiltin("variable_instance_get", {node, RValue(name)}), d)) return d;
    } catch (...) {}
    return fallback;
}

inline void ResonanceLog(const std::string& text) {
    if (resonanceLogs >= 12) return;
    ++resonanceLogs;
    Out("minerhelm: " + text);
}

inline bool Resonating(int64_t room, int64_t id) {
    for (const auto& r : resonating) if (r.room == room && r.id == id) return true;
    return false;
}

// Put the vein back as it was. A completed vein keeps its own `miningQue`
// (the game resets it); an abandoned one has the flag lowered again.
inline void ReleaseVein(const Resonance& r, bool completed) {
    try {
        if (!g_Yytk->CallBuiltin("instance_exists", {r.inst}).ToBoolean()) return;
        g_Yytk->CallBuiltin("variable_instance_set", {r.inst, RValue("miningActivateDistance"), RValue(r.savedDistance)});
        if (!completed) g_Yytk->CallBuiltin("variable_instance_set", {r.inst, RValue("miningQue"), RValue(false)});
    } catch (...) {}
}

inline void StartResonance(int64_t room, int64_t sourceId, double sx, double sy) {
    if (!veinResonance) return;
    try {
        const double skill = MiningLevel();
        if (!std::isfinite(skill)) { ResonanceLog("vein resonance skipped - mining level unreadable"); return; }
        const RValue object = g_Yytk->CallBuiltin("asset_get_index", {RValue("Mining_Node_obj")});
        if (object.ToDouble() < 0) return;
        int count = static_cast<int>(g_Yytk->CallBuiltin("instance_number", {object}).ToDouble());
        if (count > kMaxNodesScanned) count = kMaxNodesScanned;
        std::vector<MinerRules::Node> nodes;
        std::vector<std::pair<int64_t, RValue>> handles;
        for (int i = 0; i < count; ++i) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", {object, RValue((double)i)});
            double id;
            if (!LiveInstanceId(inst, id) || static_cast<int64_t>(id) == sourceId) continue;
            double nx, ny;
            if (!Position(inst, nx, ny) || std::hypot(nx - sx, ny - sy) > MinerRules::Radius) continue;
            const bool busy = NodeNumber(inst, "miningActive", 1) != 0 || NodeNumber(inst, "miningQue", 1) != 0;
            nodes.push_back({static_cast<int64_t>(id), nx, ny, NodeNumber(inst, "hp", 0), NodeRequirement(inst), busy, true});
            handles.emplace_back(static_cast<int64_t>(id), inst);
        }
        for (const auto& pick : MinerRules::Nearby(nodes, sourceId, sx, sy, skill)) {
            if (Resonating(room, pick.id)) continue;
            RValue inst;
            for (const auto& h : handles) if (h.first == pick.id) inst = h.second;
            const double saved = NodeNumber(inst, "miningActivateDistance", 16);
            g_Yytk->CallBuiltin("variable_instance_set", {inst, RValue("miningActivateDistance"), RValue(kResonanceReach)});
            g_Yytk->CallBuiltin("variable_instance_set", {inst, RValue("miningQue"), RValue(true)});
            resonating.push_back({room, pick.id, resonanceFrame, saved, inst});
            ++resonanceStarted;
            ResonanceLog("vein resonance -> node " + std::to_string(pick.id) + " (" + std::to_string((int)std::hypot(pick.x - sx, pick.y - sy)) + " px away)");
        }
    } catch (...) { ResonanceLog("vein resonance failed - neighbouring veins left alone"); }
}

inline void ResonanceTick(int64_t room) {
    for (size_t i = 0; i < resonating.size();) {
        const Resonance& r = resonating[i];
        if (r.room != room) { resonating.erase(resonating.begin() + i); continue; }
        if (resonanceFrame - r.frame < kResonanceFrames) { ++i; continue; }
        ReleaseVein(r, false);
        ++resonanceExpired;
        ResonanceLog("vein resonance: node " + std::to_string(r.id) + " did not complete; released");
        resonating.erase(resonating.begin() + i);
    }
}

inline bool Index(const RValue& array, int index, RValue& value) {
    if (array.m_Kind != VALUE_ARRAY || index < 0) return false;
    const double count = g_Yytk->CallBuiltin("array_length", {array}).ToDouble();
    if (!std::isfinite(count) || index >= count) return false;
    value = g_Yytk->CallBuiltin("array_get", {array, RValue((double)index)});
    return true;
}

// The equipment table stores fingerprints, not item structs. Read ONLY the
// local player's non-mercenary helmet slot, then use the game's own resolver.
// No inventory walk, created-item registry or permissive fallback is involved.
inline bool ReadWorn() {
    worn = equipmentReadable = false;
    equipmentReason = "Equipment unavailable";
    try {
        RValue player;
        if (!HhResolveLocalPlayer(player)) {
            equipmentReason = "Enter a map with your character";
            return false;
        }
        double index = -1;
        if (!MiningOre::WholeNumber(g_Yytk->CallBuiltin("variable_global_get", {RValue("mplr")}), index)
            || index < 0 || index > 4) return false;
        RValue all = g_Yytk->CallBuiltin("variable_global_get", {RValue("equippedItems")});
        RValue ownerSlots, slots, fingerprint;
        if (!Index(all, (int)index, ownerSlots) || !Index(ownerSlots, 0, slots)
            || !Index(slots, 0, fingerprint)) return false;
        equipmentReadable = true;
        equipmentReason = "Equip Miner's Helmet for 4x ore";
        if (fingerprint.m_Kind == VALUE_UNDEFINED) return false;
        // Invalid values must never enter the game's string/map resolver.
        // A C++ catch cannot make a native runner error safe after the fact.
        if (fingerprint.m_Kind != VALUE_STRING) {
            equipmentReadable = false;
            equipmentReason = "Equipment unavailable - ore stays at x1";
            return false;
        }
        if (fingerprint.ToString().empty()) return false;
        CInstance* global = nullptr; g_Yytk->GetGlobalInstance(&global);
        if (!global) { equipmentReadable = false; return false; }
        RValue owner, item;
        if (!AurieSuccess(g_Yytk->CallGameScriptEx(owner,
            HeroSiege::Scripts::gml_Script_GetOnlinePlayerItemOwner.data(), global, global, {RValue(index)}))) {
            equipmentReadable = false; return false;
        }
        if (!AurieSuccess(g_Yytk->CallGameScriptEx(item,
            HeroSiege::Scripts::gml_Script_GetItemFromFingerprint.data(), global, global, {fingerprint, owner}))) {
            equipmentReadable = false; return false;
        }
        if (item.m_Kind != VALUE_OBJECT || !item.m_Object) return false;
        double type, seed, base, rarity, subtype;
        RValue definition = g_Yytk->CallBuiltin("variable_struct_get", {item, RValue("itemDefinitionStruct")});
        if (definition.m_Kind != VALUE_OBJECT || !definition.m_Object
            || !TryStructNumber(item, "itemType", type) || !TryStructNumber(definition, "a", seed)
            || !TryStructNumber(definition, "b", base) || !TryStructNumber(definition, "c", rarity)
            || !TryStructNumber(definition, "j", subtype)) return false;
        for (double value : {type, seed, base, rarity, subtype}) {
            if (!std::isfinite(value) || value < 0 || std::floor(value) != value) {
                equipmentReadable = false;
                equipmentReason = "Invalid equipment data - ore stays at x1";
                return false;
            }
        }
        worn = MinerRules::Matches(type, seed, base, rarity, subtype);
        // Custom Forge creates a fresh seed. Its validated sidecar tags that
        // exact item with miner, so custom helmets work without a reserved seed.
        if (!worn && type == 0 && g_Yytk->CallBuiltin("variable_struct_exists", {item, RValue("fp_mechanic")}).ToBoolean()) {
            const RValue mechanic = g_Yytk->CallBuiltin("variable_struct_get", {item, RValue("fp_mechanic")});
            worn = mechanic.m_Kind == VALUE_STRING && mechanic.ToString() == "miner";
        }
        if (worn) equipmentReason = "Equipped - 4x mining bonus ready";
        return worn;
    } catch (...) { equipmentReadable = false; equipmentReason = "Equipment read failed - ore stays at x1"; return false; }
}

// Who dug this node. Established by private static inspection of the installed
// build (2026-09-23): the node's `miningPlayer` starts as noone and exactly one
// path of the node's step script ever writes it (a with-loop over players that
// the ordinary local dig does not take); nothing clears it. So at the ore
// reward it is still noone for a normal dig, and the game's own reward script
// attributes ground loot made by this client to the local player (`mplr`).
// Two rules follow:
//   1. `miningPlayer` names a live instance -> it must be the local player.
//   2. `miningPlayer` is not an instance (the ordinary dig) -> the local player
//      must be standing next to the node; a dig needs the player within a few
//      dozen pixels, so a remote player's node elsewhere in the zone can never
//      pass, and one dug beside us is loot this client already owns in vanilla.
// True when the reward now being dispatched carries the helmet's x4 rather
// than the ore slider; RewardDispatched keeps the pulse, the counters and
// Vein Resonance to helmet rewards.
inline bool lastRewardWasHelmet = false;

inline int RewardMultiplier(CInstance* node) {
    // Without the helmet's x4 the reward gets what the ore slider asks for, as
    // it would if the helmet mechanic had never been armed.
    auto refuse = [](const std::string& reason) {
        const int slider = MinerRules::QuantityMultiplier(true, false, MiningOre::multiplier);
        const std::string text = slider > 1 ? reason + " (ore slider x" + std::to_string(slider) + " applies)" : reason;
        if (lastRewardReason != text && rewardRefusalsLogged < 4) {
            ++rewardRefusalsLogged;
            Out("minerhelm: ore bonus skipped - " + text);
        }
        lastRewardReason = text;
        lastRewardWasHelmet = false;
        return slider;
    };
    lastRewardWasHelmet = false;
    if (!enabled) return refuse("Helmet effect is not armed");
    try {
        RValue player;
        if (!node || !HhResolveLocalPlayer(player)) return refuse("Local player unavailable");
        const RValue self = node->ToRValue();
        if (!g_Yytk->CallBuiltin("variable_instance_exists", {self, RValue("miningPlayer")}).ToBoolean())
            return refuse("Mining node has no miningPlayer field");
        RValue miner = g_Yytk->CallBuiltin("variable_instance_get", {self, RValue("miningPlayer")});
        if (probeRewardLines < kProbeRewardMaxLines) {
            ++probeRewardLines;
            try { Out("minerhelm probe: reward from node " + DescribeNode(self, player)); } catch (...) {}
        }
        double localId = -1;
        if (!LiveInstanceId(player, localId)) return refuse("Local player identity unreadable");
        double miningId = -1, nodeId = -1;
        std::string rule;
        if (LiveInstanceId(miner, miningId)) {
            if (miningId != localId) return refuse("Ore belongs to another player");
            rule = "named miner";
        } else if (!resonating.empty() && LiveInstanceId(self, nodeId) && Resonating(CurrentRoomKey(), static_cast<int64_t>(nodeId))) {
            rule = "vein resonance";
        } else {
            double px, py, nx, ny;
            if (!Position(player, px, py) || !Position(self, nx, ny)) return refuse("Positions unreadable");
            const double distance = std::hypot(px - nx, py - ny);
            if (distance > kImplicitDigRadius) {
                double raw = -1; InstanceNumber(miner, raw);
                return refuse("Node has no named miner and the player is " + std::to_string((int)distance)
                    + " px away (miningPlayer kind " + std::to_string(miner.m_Kind) + " value " + std::to_string((long long)raw) + ")");
            }
            rule = "player beside the node, " + std::to_string((int)distance) + " px";
        }
        if (!ReadWorn()) return refuse("Miner's Helmet is not equipped or unreadable");
        lastRewardReason = "Local equipped miner verified (" + rule + ")";
        lastRewardWasHelmet = true;
        return MinerRules::QuantityMultiplier(true, true, MiningOre::multiplier);
    } catch (...) { return refuse("Mining ownership check failed"); }
}

inline void RewardDispatched(CInstance* node, double x, double y) {
    // A slider-only reward (helmet off) is the ore slider's business: no
    // helmet counter, no pulse, no Vein Resonance.
    if (!lastRewardWasHelmet) return;
    ++rewards;
    lastRewardReason = "4x ore reward dispatched";
    if (!node) return;
    const int64_t room = CurrentRoomKey();
    if (room == INT64_MIN) return;
    double idNumber = -1;
    if (!LiveInstanceId(node->ToRValue(), idNumber)) return;
    const int64_t id = static_cast<int64_t>(idNumber);
    // One node completion may hand out several ore rewards (rich veins); the
    // resonance and the pulse follow the first one only.
    const auto key = std::make_pair(room, id);
    if (std::find(recentNodes.begin(), recentNodes.end(), key) != recentNodes.end()) return;
    if (recentNodes.size() >= 128) recentNodes.erase(recentNodes.begin());
    recentNodes.push_back(key);
    bool chained = false;
    for (size_t i = 0; i < resonating.size(); ++i) {
        if (resonating[i].room != room || resonating[i].id != id) continue;
        ReleaseVein(resonating[i], true);
        resonating.erase(resonating.begin() + i);
        ++bonusVeins;
        chained = true;
        break;
    }
    if (!chained) {
        double nx, ny;
        if (Position(node->ToRValue(), nx, ny)) StartResonance(room, id, nx, ny);
    }
    if (!hudNative || !std::isfinite(x) || !std::isfinite(y)) return;
    if (waves.size() >= 6) waves.erase(waves.begin());
    waves.push_back({x, y, HhNowMs(), room});
    ++wavesStarted;
    if (wavesStarted == 1) Out("minerhelm: first local 4x reward and golden pulse queued");
}

inline void Arm() {
    pending = false;
    if (enabled) return;
    // Install outside item constructors, after the runner/player is ready.
    if (!MiningOre::Install()) { equipmentReason = "Mining hooks unavailable"; return; }
    MiningOre::rewardMultiplier = RewardMultiplier;
    MiningOre::rewardDispatched = RewardDispatched;
    enabled = true;
    InstallHeadLabelHook();
    hudNative = g_Orig_DrawHudBuffs && !AddrIsExecutableInModule(GetModuleHandleA(nullptr), (const void*)g_Orig_DrawHudBuffs);
    Out("minerhelm: armed; only a worn helmet grants 4x ore; the ore slider no longer stacks");
}

inline void Tick() {
    ProbeTick();
    ++resonanceFrame;
    if (!resonating.empty()) ResonanceTick(CurrentRoomKey());
    if (!pending && !enabled) return;
    const double now = HhNowMs();
    if (!std::isfinite(now)) return;
    if (now >= lastStatus && now - lastStatus < 1000) return;
    lastStatus = now;
    RValue player;
    if (!HhResolveLocalPlayer(player)) {
        worn = equipmentReadable = false;
        equipmentReason = "Enter a map with your character";
        waves.clear(); recentNodes.clear(); resonating.clear(); return;
    }
    if (pending) Arm();
    if (enabled) ReadWorn(); // status only; RewardMultiplier always checks afresh.
}

inline void Draw() {
    if (waves.empty()) return;
    const double now = HhNowMs();
    const int64_t room = CurrentRoomKey();
    waves.erase(std::remove_if(waves.begin(), waves.end(), [&](const auto& w) {
        return w.room != room || !std::isfinite(now) || now < w.born || now - w.born >= 550;
    }), waves.end());
    if (waves.empty()) return;
    RValue oldColour, oldAlpha;
    bool saved = false;
    try {
        RValue cam = g_Yytk->CallBuiltin("view_get_camera", {RValue(0.0)});
        const double vx = g_Yytk->CallBuiltin("camera_get_view_x", {cam}).ToDouble();
        const double vy = g_Yytk->CallBuiltin("camera_get_view_y", {cam}).ToDouble();
        const double vw = g_Yytk->CallBuiltin("camera_get_view_width", {cam}).ToDouble();
        const double vh = g_Yytk->CallBuiltin("camera_get_view_height", {cam}).ToDouble();
        const double gw = g_Yytk->CallBuiltin("display_get_gui_width", {}).ToDouble();
        const double gh = g_Yytk->CallBuiltin("display_get_gui_height", {}).ToDouble();
        if (vw <= 0 || vh <= 0 || !std::isfinite(vw + vh + gw + gh + vx + vy)) return;
        oldColour = g_Yytk->CallBuiltin("draw_get_colour", {});
        oldAlpha = g_Yytk->CallBuiltin("draw_get_alpha", {}); saved = true;
        g_Yytk->CallBuiltin("draw_set_colour", {
            g_Yytk->CallBuiltin("make_colour_rgb", {RValue(255.0), RValue(197.0), RValue(65.0)})});
        for (const auto& w : waves) {
            const double progress = std::clamp((now - w.born) / 550.0, 0.0, 1.0);
            g_Yytk->CallBuiltin("draw_set_alpha", {RValue((1 - progress) * 0.9)});
            const double radius = 8 + MinerRules::Radius * progress;
            for (double inset : {0.0, 2.0}) {
                const double rx = (radius - inset) * gw / vw, ry = (radius - inset) * gh / vh;
                const double sx = (w.x - vx) * gw / vw, sy = (w.y - vy) * gh / vh;
                g_Yytk->CallBuiltin("draw_ellipse", {RValue(sx-rx), RValue(sy-ry), RValue(sx+rx), RValue(sy+ry), RValue(true)});
            }
        }
    } catch (...) { waves.clear(); }
    if (saved) {
        try { g_Yytk->CallBuiltin("draw_set_colour", {oldColour}); } catch (...) {}
        try { g_Yytk->CallBuiltin("draw_set_alpha", {oldAlpha}); } catch (...) {}
    }
}

inline void Command(const std::string& args) {
    std::istringstream input(args);
    std::string action, request, extra;
    input >> action >> request;
    if (action == "status" && request.empty()) {
        ReadWorn();
        Out("minerhelm: " + equipmentReason + "; rewards=" + std::to_string(rewards)
            + " pulses=" + std::to_string(wavesStarted) + " veins=" + std::string(veinResonance ? "on" : "off")
            + " bonusVeins=" + std::to_string(bonusVeins) + " queued=" + std::to_string(resonanceStarted)
            + " released=" + std::to_string(resonanceExpired) + "; last reward: " + lastRewardReason); return;
    }
    if (action == "veins") {
        if ((request != "0" && request != "1") || (input >> extra)) { Out("minerhelm: use `veins 0|1`"); return; }
        veinResonance = request == "1";
        Out(std::string("minerhelm: vein resonance ") + (veinResonance ? "on - a finished dig also finishes up to two veins within 192 px" : "off")); return;
    }
    if (action == "probe") {
        int seconds = 20;
        if (!request.empty()) { try { seconds = std::stoi(request); } catch (...) { seconds = 0; } }
        if (seconds < 1 || seconds > 120 || (input >> extra)) { Out("minerhelm: use `probe [1-120 seconds]`"); return; }
        const double now = HhNowMs();
        if (!std::isfinite(now)) return;
        probeUntil = now + seconds * 1000.0; probeLines = 0; probeLast.clear();
        Out("minerhelm: probe armed for " + std::to_string(seconds) + " s - dig the nearest node now"); return;
    }
    if (action != "grant" || request.size() != 32 || (input >> extra)
        || request.find_first_not_of("0123456789abcdef") != std::string::npos) return;
    if (std::find(grantRequests.begin(), grantRequests.end(), request) != grantRequests.end()) return;
    if (grantRequests.size() >= 128) grantRequests.erase(grantRequests.begin());
    grantRequests.push_back(request); grantRequest = request;
    const double now = HhNowMs();
    if (now - lastGrant < 2000) { grantResult = "Please wait before creating another helmet"; return; }
    lastGrant = now;
    RValue player;
    if (!HhResolveLocalPlayer(player)) { grantResult = "Enter a map with your character first"; return; }
    Arm();
    if (!enabled) { grantResult = "Mining hooks unavailable; no helmet created"; return; }
    CInstance* context = HhResolveInstance(player);
    if (!context) { grantResult = "Player unavailable; no helmet created"; return; }
    try {
        const double x = g_Yytk->CallBuiltin("variable_instance_get", {player, RValue("x")}).ToDouble();
        const double y = g_Yytk->CallBuiltin("variable_instance_get", {player, RValue("y")}).ToDouble();
        if (!std::isfinite(x) || !std::isfinite(y)) { grantResult = "Player position unavailable"; return; }
        grantResult = SpawnSignatureItem(2, x, y, context)
            ? "Helmet dropped beside your character. Pick it up and equip it."
            : "Helmet creation failed. See the plugin log.";
    } catch (...) { grantResult = "Helmet creation failed; request was not retried"; }
    Out("minerhelm: " + grantResult);
}
}
