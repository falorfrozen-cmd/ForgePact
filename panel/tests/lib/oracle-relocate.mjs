// The Gems supplement's navigation relocation: the one, explicit and tested
// way a recording's navigation steps follow a control that moved.
//
// tests/behaviour-oracle-gems.json was recorded from origin/main's legacy page,
// where the Gems of Incarnation controls sat on Mods › Quality of Life, and it
// is never re-recorded or edited (it would then prove the page by the page).
// The controls now sit on the Loot tab, in #gemsCard, under the same hooks, so
// only the recording's two navigation steps point at the wrong place. This
// maps them, and nothing else:
//
// - only a step's `control` changes, and only when it is a key of `moves`
//   (the step then carries `relocatedFrom`, the recorded control);
// - every key and value is a navigation step (`tab:…` or `subtab:…`);
// - a step it would move must have sent nothing (`posts` and `cmds` empty),
//   so a relocation can never hide a behaviour: what is compared stays what
//   was recorded;
// - a key that matches no step is a stale map, and throws;
// - the input is never mutated.
//
// Used in exactly two places: replaySupplement in tests/oracle.mjs and the
// supplement branch of derive() in tests/oracle-derive.mjs. Never on the
// legacy recording (its `tab:mods` steps are real) and never by the recorder.

export const SUPPLEMENT_RELOCATION = Object.freeze({
  'tab:mods': 'tab:loot',
  'subtab:qol': 'tab:loot',
});

const NAVIGATION = /^(tab|subtab):[a-z0-9-]+$/;

export function relocate(recording, moves) {
  for (const [from, to] of Object.entries(moves)) {
    if (!NAVIGATION.test(from) || !NAVIGATION.test(to)) {
      throw new Error(`relocate: ${from} -> ${to} is not a navigation step (tab:… or subtab:…)`);
    }
  }
  const used = new Set();
  const steps = recording.steps.map((step) => {
    const copy = structuredClone(step);
    if (!Object.hasOwn(moves, step.control)) return copy;
    const sent = (step.posts || []).length + (step.cmds || []).length;
    if (sent) {
      throw new Error(`relocate: step ${step.step} (${step.control}) sent ${JSON.stringify({ posts: step.posts, cmds: step.cmds })}; only a step that sent nothing may move`);
    }
    used.add(step.control);
    copy.control = moves[step.control];
    copy.relocatedFrom = step.control;
    return copy;
  });
  const stale = Object.keys(moves).filter((from) => !used.has(from));
  if (stale.length) throw new Error(`relocate: ${stale.join(', ')} matches no step (a stale map)`);
  return { ...structuredClone(recording), steps };
}
