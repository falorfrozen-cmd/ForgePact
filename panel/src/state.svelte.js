// ST: the last /api/state payload - the saved settings (`cfg`), what the
// plugin reports (`pluginMods`, `chain`, `pluginBuild`, `launch`), and the tables the data
// rows are drawn from (`spawners`, `drops`, `keys`, `stats`, `percentStats`,
// the Satanic pools). boot() replaces it through setST(); the write queue and
// the poll patch its fields in place.
//
// A plain module binding rather than a $state proxy: every reader is plain JS
// that mutates it in place, and the page reads it only through those
// functions. A component that wants to react to it can wrap it here without
// touching its callers.
export let ST=null;
export function setST(value){ST=value}
