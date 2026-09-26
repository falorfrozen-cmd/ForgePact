// The two explicit, tested transforms replay applies to a recording before it
// compares it: the Gems supplement's navigation relocation (the one way a
// recording's navigation steps follow a control that moved), and, further
// down, insertAddedKeys (the one way a full key reset follows a key the
// backend added after the recording was made).
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

// The lines the backend added to a full key reset after a recording was made:
// the one, explicit and tested way a recording follows a key a later main
// added to KEYS.
//
// A live key change sends build_key_cmds(include_resets=True), which walks
// every key in KEYS; origin/main's 1.4.7 put Prime Evil Parts (`primeevil`,
// at its default x1) between Colosseum Fragments and Ruby Keys. The legacy
// recording and the Gems supplement predate it, so each full reset list they
// hold lacks exactly that line. This inserts it, and nothing else:
//
// - only a step whose `cmds` are a full reset list (they hold FULL_RESET)
//   changes, and only by the entry's line, right after its anchor line;
// - posts, controls, actions and every other step stay as recorded;
// - a full reset step without exactly one anchor line, or already carrying
//   the added line, throws: the recording was made after the key existed
//   (behaviour-oracle-primeevil.json, recorded from main's own page, is one),
//   and inserting again would hide what that page really sent;
// - the input is never mutated.
//
// Applied by replay in tests/oracle.mjs to the legacy recording and the Gems
// supplement (which holds no full reset step, so it is a no-op there); never to
// a supplement recorded after the key existed, and never by the recorder.

export const FULL_RESET = 'dungeonkey del 12';

export const ADDED_KEYS = Object.freeze([
  Object.freeze({ key: 'primeevil', line: 'droprate group primeevil 1', after: 'droprate group colosfrag ' }),
]);

export function insertAddedKeys(recording, added = ADDED_KEYS) {
  const steps = recording.steps.map((step) => {
    const copy = structuredClone(step);
    if (!(step.cmds || []).includes(FULL_RESET)) return copy;
    for (const { line, after } of added) {
      if (copy.cmds.includes(line)) {
        throw new Error(`insertAddedKeys: step ${step.step} (${step.control}) already sends ${JSON.stringify(line)}; the recording was made after the key existed`);
      }
      const anchors = copy.cmds.flatMap((c, i) => (c.startsWith(after) ? [i] : []));
      if (anchors.length !== 1) {
        throw new Error(`insertAddedKeys: step ${step.step} (${step.control}) is a full reset with ${anchors.length} lines starting ${JSON.stringify(after)}, not one`);
      }
      copy.cmds.splice(anchors[0] + 1, 0, line);
    }
    return copy;
  });
  return { ...structuredClone(recording), steps };
}
