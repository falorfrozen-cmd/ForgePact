#pragma once

#include "Common.hpp"

namespace ForgePact {

// Pet Quest Collector: while enabled and the player has a pet out, the pet is
// meant to collect quest items anywhere on screen without the player hovering
// each one and pressing the interact key. See
// `ForgePact/docs/pet-quest-collector-plan.md` for the full design.
//
// STATUS (2026-09-11): WORKING, and re-confirmed live after the call route
// was rewritten to use no game addresses - the quest counter advanced on a
// pet collect, with `petquest 0` reporting
// `call route=script_execute (name-resolved, no layout)`,
// `dispatched-but-item-remained=0` and zero structural refusals.
//
// The mechanism, confirmed live -
// a plugin-invoked collect advanced a quest counter 7/15 -> 8/15, which is
// the pass condition the plan set (the objective credited, not merely the
// item removed). The call, every part of it measured rather than inferred:
// with the quest item as `self` and `Loot_Manager_obj` as `other`, invoke the
// item's own `m_Questpickup` method value with one real argument. It calls
// update_quest(questIndex, questObjectiveNumber, questValue) and
// QuestSaveUpdate internally, so the credit is inside the call and nothing
// here has to fake it.
//
// REVISED the same day, twice. That call originally went through the
// runtime's call-a-method-value helper at a fixed address, which would have
// jumped into unrelated bytes on the next game build - `relicgate`'s defect,
// shipped again. Reading the callable off the value's own CScriptRef needs no
// address but was then measured wrong for this runner (the `m_Quest*` values
// are not CScriptRefs here). What ships is `script_execute`, resolved by
// name, with self/other supplied through CallBuiltinEx: no address, and no
// struct layout either. The call shape never changed across any of it.
// `InvokeMethodValue` keeps the CScriptRef route as a validated fallback, and
// `petquest 0` reports which route ran. See agents.md, "Never Call an Address
// You Resolved by Hand", and docs/pet-quest-collector-c-research.md.
//
// Three earlier mechanisms were closed by measurement and one turned out to
// be an artefact:
//   B1/B2 (call or hook the script the interaction invokes) - the "0 calls"
//      that blocked these was the *instrument*: those hooks swap a pointer in
//      the script table, and compiled GML calls another script with a direct
//      call bound at compile time, which never reads that table;
//   B4 (fake the player's hover + keypress) - genuinely disproven: the
//      GML-visible input state is a downstream mirror of real device input.
// See docs/pet-quest-collector-c-research.md for the whole chain.
//
// What the tick does now: with the mod on and a pet out, it picks the nearest
// eligible quest item on screen, walks the pet to it (writing x/y, the same
// way OrbPickupTick pulls globes), and on arrival invokes the confirmed
// collect - one item at a time, with a cooldown between, so it reads as the
// pet fetching rather than items silently vanishing.
//
// Two gates are re-read from the live instance at collect time, never cached
// from selection: `canPickup`, and `lootType == 0`. Both are the game's own,
// taken from its real call site. lootType 0 is the m_Questpickup branch;
// the other quest object types (activate/destructible/interact/active) want a
// different m_Quest* method, none of which is confirmed, so they are left
// alone rather than guessed at. Driving a collect on an item the game would
// have skipped is exactly how an objective gets credited for something
// uncollectable, which the plan calls worse than no mod at all.
//
// Deliberately simpler than RelicFilterMod: no hook is installed by this mod
// (the tick only reads instance positions/state through YYTK's CallBuiltin,
// same chokepoint OrbPickupTick already uses), so there is no arm/pending
// lifecycle to manage and `build_cmds` can safely emit `petquest 1` at
// launch.
class PetQuestCollectorMod {
public:
    static PetQuestCollectorMod& Instance() {
        static PetQuestCollectorMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }

    // "petquest 1" / "petquest 0".
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        Out(std::string("petquest -> ") + (enabled ? "ON (pet fetches and collects lootType-0 quest items on screen)" : "OFF"));
    }

private:
    PetQuestCollectorMod() = default;
    std::atomic<bool> m_Enabled{ false };
};

} // namespace ForgePact
