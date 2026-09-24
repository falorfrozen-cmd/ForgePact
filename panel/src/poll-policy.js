// ---- adaptive poll policy -------------------------------------------------
// Shared, deliberately verbatim, with
// HS-Offline-Launcher/src/hs_offline_launcher.py: same function, same three
// constant names, same values. A fixed interval forces a trade nobody wins -
// fast costs poll work for the whole session, slow costs feedback latency at
// exactly the moments somebody is watching. The trade only exists because the
// interval is fixed, and both clients can already tell when a change is
// plausible: the user just moved a control, or the payload they just received
// differs from the previous one.
//
// Known gap, documented rather than special-cased: a window OCCLUDED by a
// fullscreen game is not necessarily document.hidden, so it idles (one poll
// per 30 s) instead of suspending.
//
// POLL_WATCHED_FIELDS is also declared in src/forgepact.py; the tests hold the
// two lists equal. Kept as one plain module, free of Svelte, so the tests can
// run it through node.
export const POLL_FAST_MS = 2000;          // something just happened; the user is watching
export const POLL_IDLE_MS = 30000;         // nothing has changed for a while
export const POLL_FAST_WINDOW_MS = 15000;  // how long "just happened" lasts
export const POLL_WATCHED_FIELDS = ["gameRunning", "ipcOk", "lastApplied", "queued"];
// null means: do not schedule a poll at all.
export function pollDelayMs(hidden, msSinceChange){
  if(hidden) return null;
  return msSinceChange < POLL_FAST_WINDOW_MS ? POLL_FAST_MS : POLL_IDLE_MS;
}
// Watched fields are dotted paths so a nested one (game.build) reads the same
// way as a flat one.
export function pollFieldValue(payload, field){
  let cur = payload;
  for(const part of field.split('.')){
    if(cur === null || cur === undefined) return undefined;
    cur = cur[part];
  }
  return cur;
}
export function pollPayloadChanged(prev, next){
  if(!prev) return true;
  return POLL_WATCHED_FIELDS.some(f =>
    JSON.stringify(pollFieldValue(prev, f)) !== JSON.stringify(pollFieldValue(next, f)));
}
// The change clock: a local action, or an observed difference in a watched
// field, resets it.  Anything else leaves it where it was.
export function pollNextChangeAt(prev, next, localAction, now, lastChange){
  return (localAction || pollPayloadChanged(prev, next)) ? now : lastChange;
}
