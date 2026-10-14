<script>
  // The notable groups (#185): named sets of item names the Session section
  // counts under "Notable" (the ledger's notable_defs), such as the S and SS
  // runes. A group is added as a label and its names on one line, split on
  // commas, and removed with its own button. With no group left the ledger
  // falls back to its four defaults, and the card says so.
  import { addNotable, removeNotable } from './edit.js';

  let { client, write } = $props();

  const groups = $derived(Array.isArray(client.settings?.notable) ? client.settings.notable : []);
  let label = $state('');
  let names = $state('');
  let error = $state(null);
  let saving = $state(false);

  async function add(e) {
    e.preventDefault();
    if (saving) return;
    // Checked here first, so a blank group sends nothing.
    try { addNotable({ notable: groups }, label, names); } catch (err) { error = err.message; return; }
    saving = true;
    try {
      const res = await write((d) => addNotable(d, label, names));
      if (res.err) { error = res.err; return; }
      error = null;
      label = '';
      names = '';
    } finally {
      saving = false;
    }
  }

  async function remove(group) {
    const res = await write((d) => removeNotable(d, group));
    error = res.err ?? null;
  }
</script>

<section class="card trk-half" aria-labelledby="trkNotableTitle" id="trkNotable">
  <h2 id="trkNotableTitle">Notable finds</h2>
  <p class="hint">Items the Session tab counts on their own, such as keys and the rune grades. A drop counts toward the first group that names it.</p>
  {#if groups.length}
    <ul class="trk-names">
      {#each groups as g (g.label)}
        <li data-label={g.label}>
          <span class="trk-group"><b>{g.label}</b><span class="trk-group-names">{(g.names ?? []).join(', ')}</span></span>
          <button type="button" class="trk-quiet trk-icon-button" aria-label={'Remove the ' + g.label + ' group'} onclick={() => remove(g.label)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="m4.5 4.5 7 7M11.5 4.5l-7 7"/></svg></button>
        </li>
      {/each}
    </ul>
  {:else}
    <p class="trk-empty">No groups of your own: the tracker counts its four defaults (Angelic Key, Satanic Dice, S runes, SS runes).</p>
  {/if}
  <form class="trk-notable-form" id="trkNotableForm" onsubmit={add}>
    <input class="trk-field" id="trkNotableLabel" type="text" placeholder="Group name" aria-label="New group's name" maxlength="60" autocomplete="off" bind:value={label}>
    <input class="trk-field" id="trkNotableNames" type="text" placeholder="Item names, separated by commas" aria-label="New group's item names" autocomplete="off" bind:value={names}>
    <button type="submit" class="btn" disabled={saving}>Add group</button>
  </form>
  {#if error}<p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{error}</p>{/if}
</section>

<style>
  .trk-names { display: flex; flex-direction: column; margin: 0; padding: 0; list-style: none; }
  .trk-names li { display: flex; align-items: center; justify-content: space-between; gap: var(--space-6); min-height: 32px; padding: 0 0 0 var(--space-4); border-radius: var(--radius-sm); font-size: var(--font-size-sm); }
  .trk-group { display: flex; flex-direction: column; min-width: 0; }
  .trk-group b { font-weight: var(--font-weight-medium); color: var(--color-text-primary); }
  .trk-group-names { color: var(--color-text-muted); font-size: var(--font-size-xs); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .trk-notable-form { display: flex; flex-wrap: wrap; gap: var(--space-4); }
  .trk-field {
    flex: 1 1 160px;
    height: 32px;
    min-width: 0;
    padding: 0 var(--space-6);
    border: 1px solid var(--color-border-strong);
    border-radius: var(--radius-md);
    background-color: var(--color-bg-sunken);
    color: var(--color-text-primary);
    font: inherit;
    font-size: var(--font-size-sm);
  }
  .trk-field::placeholder { color: var(--color-text-muted); }
  .trk-quiet {
    display: inline-flex;
    flex: none;
    align-items: center;
    min-height: 28px;
    border: 0;
    border-radius: var(--radius-sm);
    background: none;
    color: var(--color-text-muted);
    font: inherit;
    cursor: pointer;
  }
  .trk-quiet svg { width: 16px; height: 16px; stroke: currentColor; stroke-width: 1.5; stroke-linecap: round; stroke-linejoin: round; fill: none; }
  .trk-icon-button { width: 28px; padding: 0; justify-content: center; }
  .trk-error { display: flex; align-items: center; gap: var(--space-4); color: var(--color-text-primary); }
  .trk-error svg { flex: none; width: 16px; height: 16px; color: var(--color-danger); stroke: currentColor; stroke-width: 1.4; fill: none; stroke-linecap: round; stroke-linejoin: round; }
  .trk-empty { margin: 0; padding: var(--space-7) 0; color: var(--color-text-faint); font-size: var(--font-size-sm); }
  @media (hover: hover) and (pointer: fine) {
    .trk-names li:hover { background-color: var(--color-bg-hover); }
    .trk-quiet:hover { color: var(--color-text-primary); background-color: var(--color-bg-hover); }
  }
</style>
