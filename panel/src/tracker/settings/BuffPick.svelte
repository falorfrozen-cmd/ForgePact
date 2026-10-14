<script>
  // Which Satanic zone rotations are worth the zone sound (#185): one switch
  // per buff, named from `/api/state`'s `satanicBuffs` (hs-game-sdk's
  // SATANIC_BUFFS, which the page already holds), writing `zone_buffs`. An
  // empty pick means every rotation alerts, as in HS Offline Tracker; with
  // buffs picked, only a zone that carries one of them does (the ledger's
  // take_zone_change). Clear goes back to every rotation.
  import { clearBuffs, toggleBuff } from './edit.js';

  let { client, buffs = [], write } = $props();

  const picked = $derived(new Set(client.settings?.zone_buffs ?? []));
  const count = $derived(picked.size);
  let error = $state(null);

  async function pick(box, id) {
    const res = await write((d) => toggleBuff(d, id, box.checked));
    if (res.err) { box.checked = picked.has(id); error = res.err; } else error = null;
  }

  async function clear() {
    const res = await write(clearBuffs);
    error = res.err ?? null;
  }
</script>

<section class="card trk-half" aria-labelledby="trkBuffsTitle" id="trkBuffs">
  <div class="trk-head"><h2 id="trkBuffsTitle">Zone rotations worth a sound</h2><button type="button" class="trk-quiet" id="trkBuffsClear" disabled={!count} onclick={clear}>Clear</button></div>
  <p class="hint">
    {#if !count}Every rotation alerts until you pick buffs. Then only a zone that carries one of them does.
    {:else if count === buffs.length}Every buff is picked. A rotation with none of these, or with a buff this list does not know, stays quiet; Clear to hear every rotation.
    {:else}{count} picked. A rotation with none of them stays quiet.{/if}
  </p>
  {#if buffs.length}
    <div class="trk-picks">
      {#each buffs as [id, name, desc] (id)}
        <label class="trk-pick" title={desc}>{name}<span class="switch"><input type="checkbox" data-id={id} checked={picked.has(id)} aria-label={name} onchange={(e) => pick(e.currentTarget, id)}><span class="sl"></span></span></label>
      {/each}
    </div>
  {:else}
    <p class="trk-empty">The buff list has not loaded. Reconnect to pick buffs.</p>
  {/if}
  {#if error}<p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{error}</p>{/if}
</section>

<style>
  .trk-picks { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--space-2) var(--space-11); }
  .trk-pick { display: flex; align-items: center; justify-content: space-between; gap: var(--space-6); min-height: 36px; font-size: var(--font-size-sm); }
  .trk-quiet {
    display: inline-flex;
    flex: none;
    align-items: center;
    min-height: 28px;
    padding: 0 var(--space-4);
    border: 0;
    border-radius: var(--radius-sm);
    background: none;
    color: var(--color-text-muted);
    font: inherit;
    font-size: var(--font-size-sm);
    cursor: pointer;
  }
  .trk-quiet:disabled { cursor: default; color: var(--color-text-faint); }
  .trk-error { display: flex; align-items: center; gap: var(--space-4); color: var(--color-text-primary); }
  .trk-error svg { flex: none; width: 16px; height: 16px; color: var(--color-danger); stroke: currentColor; stroke-width: 1.4; fill: none; stroke-linecap: round; stroke-linejoin: round; }
  .trk-empty { margin: 0; padding: var(--space-7) 0; color: var(--color-text-faint); font-size: var(--font-size-sm); }
  @media (hover: hover) and (pointer: fine) {
    .trk-quiet:hover:not(:disabled) { color: var(--color-text-primary); background-color: var(--color-bg-hover); }
  }
  @media (max-width: 720px) {
    .trk-picks { grid-template-columns: minmax(0, 1fr); }
  }
</style>
