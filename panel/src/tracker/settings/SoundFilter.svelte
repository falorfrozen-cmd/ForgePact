<script>
  // The sound lists (#185): HS Offline Tracker's sound-filter editor (MIT,
  // Q7(b); its SoundFilter view) in ForgePact's design (design/tracker/tab.html,
  // "Sound lists"). A filter is a set of lists; each list names items whose
  // drop plays the list's own sound, whatever their rarity or grade, and the
  // first list that names an item wins. Only the filter `filter` names is
  // used, and only while `use_filter` is on (the ledger's prefs_from_settings).
  //
  // - Every change is one edit from settings/edit.js through `write`, the
  //   section's queued read-modify-write of the whole document.
  // - A list's sound is the store's `list-<id>` file. With none, a drop on the
  //   list plays its rarity's sound (alerts.js). Deleting a list or a filter
  //   clears the files it leaves; Duplicate copies them to the new ids.
  // - Export puts the filter, with its sounds inlined, on the clipboard in the
  //   Tracker's own file format (settings/transfer.js). ForgePact's window
  //   offers no file downloads, so the clipboard is the one route that works
  //   both there and in a browser. Import reads such a file, or the clipboard.
  // - Item names come from the item codex (`/api/tracker/codex`) as
  //   suggestions; any name can be typed, as in the Tracker.
  // - Deleting a filter or a list asks first, in place.
  import { onMount } from 'svelte';
  import {
    activeFilter, addFilter, addItem, addList, clashes, duplicateFilter, moveList, removeFilter, removeItem, removeList,
    renameFilter, selectFilter, setListField, volumePercent,
  } from './edit.js';
  import {
    SOUND_ACCEPT, bytesToBase64, exportFilter, fileUpload, importFilter, parseFilterExport, soundExt, soundSource,
  } from './transfer.js';
  import { play, soundUrl, unlock } from '../audio.js';

  let { client, write } = $props();

  const STEP = 5;

  const doc = $derived(client.settings);
  const filters = $derived(Array.isArray(doc?.filters) ? doc.filters : []);
  const active = $derived(activeFilter(doc));
  const lists = $derived(active?.lists ?? []);
  let pickedList = $state(null);
  const list = $derived(lists.find((l) => l.id === pickedList) ?? lists[0] ?? null);
  const soundKey = $derived(list ? `list-${list.id}` : null);
  const source = $derived(soundKey ? soundSource(soundKey, client.sounds) : null);
  const clash = $derived(clashes(active));
  let dragging = $state(null);
  const pct = $derived(dragging ?? volumePercent(list));

  let query = $state('');
  const shown = $derived.by(() => {
    const q = query.trim().toLowerCase();
    const items = [...(list?.items ?? [])].sort((a, b) => a.localeCompare(b));
    return q ? items.filter((n) => n.toLowerCase().includes(q)) : items;
  });

  let menuOpen = $state(false);
  let confirming = $state(null); // 'filter' | 'list'
  let error = $state(null);
  let notice = $state(null);
  let busy = $state(false);
  let listFile = $state(null);
  let importFile = $state(null);
  let menuButton = $state(null);

  // The codex's names, for the item field's suggestions, and each name's rarity for its colour.
  let names = $state([]);
  let rarityOf = $state(new Map());
  onMount(() => {
    client.loadCodex().then((res) => {
      if (res.err || !Array.isArray(res.items)) return;
      const map = new Map();
      for (const row of res.items) {
        if (typeof row?.name === 'string' && row.name && !map.has(row.name.toLowerCase())) map.set(row.name.toLowerCase(), String(row.rarity ?? '').toLowerCase());
      }
      rarityOf = map;
      names = [...new Set(res.items.map((r) => r?.name).filter((n) => typeof n === 'string' && n))].sort((a, b) => a.localeCompare(b));
    });
    const away = (e) => { if (menuOpen && !e.target.closest?.('.trk-menu-host')) menuOpen = false; };
    document.addEventListener('pointerdown', away, true);
    return () => document.removeEventListener('pointerdown', away, true);
  });

  const RARITY_CLASS = { satanic: 'r-satanic', set: 'r-set', heroic: 'r-heroic', angelic: 'r-angelic', unholy: 'r-unholy' };
  const nameClass = (name) => RARITY_CLASS[rarityOf.get(String(name).toLowerCase())] ?? '';

  function say(text) { notice = text; error = null; }
  function fail(text) { error = text; notice = null; }

  async function edit(fn) {
    const res = await write(fn);
    if (res.err) fail(res.err); else error = null;
    return res;
  }

  // ---- Filters -------------------------------------------------------------

  async function newFilter() {
    let made = null;
    const res = await edit((d) => { made = addFilter(d, `Filter ${(d.filters?.length ?? 0) + 1}`); });
    if (!res.err && made) { pickedList = made.lists[0].id; say(null); }
  }

  async function pickFilter(id) {
    const res = await edit((d) => selectFilter(d, id));
    if (!res.err) { pickedList = null; confirming = null; }
  }

  // Each edit names the filter and list by the ids shown when it was asked
  // for, not whatever is picked by the time the queue runs it.
  async function rename(text) {
    if (!active || !text.trim() || text.trim() === active.name) return;
    const id = active.id;
    await edit((d) => renameFilter(d, id, text));
  }

  async function duplicate() {
    menuOpen = false;
    if (!active) return;
    const id = active.id;
    let pairs = [];
    const res = await edit((d) => { pairs = duplicateFilter(d, id).pairs; });
    if (res.err) return;
    let failed = 0;
    for (const [from, to] of pairs) {
      if (!client.sounds?.[from]) continue;
      const r = await client.changeSound(to, { copy_from: from });
      if (r.err) failed += 1;
    }
    pickedList = null;
    say(failed ? `Copied, but ${failed} list sound${failed === 1 ? '' : 's'} could not be copied.` : 'Copied, sounds and all.');
  }

  async function deleteFilter() {
    if (!active) return;
    const { id, name } = active;
    let keys = [];
    const res = await edit((d) => { keys = removeFilter(d, id); });
    confirming = null;
    if (res.err) return;
    for (const key of keys) if (client.sounds?.[key]) await client.changeSound(key, { clear: true });
    pickedList = null;
    say(`Deleted ${name}.`);
  }

  async function writeClipboard(text) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch (e) {
      const area = document.createElement('textarea');
      area.value = text;
      area.setAttribute('readonly', '');
      area.style.position = 'fixed';
      area.style.opacity = '0';
      document.body.appendChild(area);
      area.select();
      let ok = false;
      try { ok = document.execCommand('copy'); } catch (err) {}
      area.remove();
      return ok;
    }
  }

  async function exportActive() {
    menuOpen = false;
    if (!active) return;
    busy = true;
    try {
      const inlined = {};
      for (const l of active.lists) {
        const key = `list-${l.id}`;
        const entry = client.sounds?.[key];
        if (!entry) continue;
        const res = await fetch(`/api/tracker/sound/${encodeURIComponent(key)}`, { cache: 'no-store' });
        if (!res.ok) { fail(`The sound of ${l.name} could not be read, so nothing was copied.`); return; }
        inlined[l.id] = { ext: entry.ext, data: bytesToBase64(new Uint8Array(await res.arrayBuffer())) };
      }
      const text = JSON.stringify(exportFilter(active, inlined), null, 1);
      if (await writeClipboard(text)) say(`Copied ${active.name} to the clipboard, sounds included. Paste it into a .json file to keep it, or import it on another panel.`);
      else fail('Could not copy to the clipboard.');
    } finally {
      busy = false;
    }
  }

  async function importText(text, from) {
    let parsed;
    try { parsed = parseFilterExport(text); } catch (e) { fail(`${from}: ${e.message}`); return; }
    busy = true;
    try {
      let made = null;
      const res = await edit((d) => { made = importFilter(d, parsed); });
      if (res.err || !made) return;
      let failed = 0;
      for (const s of made.sounds) {
        const r = await client.changeSound(s.key, { name: `${s.key}.${s.ext}`, data: s.data });
        if (r.err) failed += 1;
      }
      pickedList = null;
      const count = `${parsed.lists.length} list${parsed.lists.length === 1 ? '' : 's'}`;
      say(failed ? `Imported ${parsed.name} (${count}), but ${failed} sound${failed === 1 ? '' : 's'} could not be filed.` : `Imported ${parsed.name}: ${count}.`);
    } finally {
      busy = false;
    }
  }

  async function importChosen(e) {
    menuOpen = false;
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (file) await importText(await file.text(), file.name);
  }

  async function importClipboard() {
    menuOpen = false;
    let text;
    try { text = await navigator.clipboard.readText(); } catch (e) { fail('The clipboard could not be read. Import the file instead.'); return; }
    await importText(text, 'The clipboard');
  }

  // ---- Lists ---------------------------------------------------------------

  async function newList() {
    if (!active) return;
    const filterId = active.id;
    let id = null;
    const res = await edit((d) => { id = addList(d, filterId); });
    if (!res.err && id) pickedList = id;
  }

  async function setField(field, value) {
    if (!active || !list) return;
    const [filterId, listId] = [active.id, list.id];
    return edit((d) => setListField(d, filterId, listId, field, value));
  }

  async function writeVolume(value) {
    await setField('volume', value / 100);
    dragging = null;
  }

  function step(direction) {
    const next = Math.min(100, Math.max(0, pct + direction * STEP));
    dragging = next;
    writeVolume(next);
  }

  async function move(direction) {
    if (!active || !list) return;
    const [filterId, listId] = [active.id, list.id];
    await edit((d) => moveList(d, filterId, listId, direction));
  }

  async function deleteList() {
    if (!active || !list) return;
    const [filterId, listId, name] = [active.id, list.id, list.name];
    let key = null;
    const res = await edit((d) => { key = removeList(d, filterId, listId); });
    confirming = null;
    if (res.err) return;
    if (key && client.sounds?.[key]) await client.changeSound(key, { clear: true });
    pickedList = null;
    say(`Deleted ${name}.`);
  }

  async function addTyped(e) {
    e.preventDefault();
    const text = query.trim();
    if (!text || !active || !list) return;
    const [filterId, listId, listName] = [active.id, list.id, list.name];
    query = '';
    let added = false;
    const res = await edit((d) => { added = addItem(d, filterId, listId, text); });
    if (!res.err && !added) say(`${text} is already on ${listName}.`);
    else if (!res.err) notice = null;
  }

  async function remove(name) {
    if (!active || !list) return;
    const [filterId, listId] = [active.id, list.id];
    await edit((d) => removeItem(d, filterId, listId, name));
  }

  async function test() {
    if (!soundKey || !source) return;
    await unlock();
    void play(soundUrl(soundKey, client.sounds), pct / 100, soundKey);
  }

  async function chosenSound(e) {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file || !soundKey || !list) return;
    if (!soundExt(file.name)) { fail(`${file.name} is not a WAV, MP3, OGG or FLAC file, so the ${list.name} sound did not change.`); return; }
    busy = true;
    try {
      const res = await client.changeSound(soundKey, await fileUpload(file));
      if (res.err) fail(`${file.name} was not used: ${res.err}. The ${list.name} sound did not change.`);
      else error = null;
    } finally {
      busy = false;
    }
  }

  async function clearSound() {
    if (!soundKey) return;
    const res = await client.changeSound(soundKey, { clear: true });
    if (res.err) fail(res.err); else error = null;
  }

  function menuKeys(e) {
    if (e.key === 'Escape') { menuOpen = false; menuButton?.focus(); }
  }

  const alsoOn = (name) => (clash.get(name.toLowerCase()) ?? []).filter((n) => n !== list?.name).join(', ');
</script>

<section class="card trk-half" aria-labelledby="trkFilterTitle" id="trkFilters">
  <div class="trk-head"><h2 id="trkFilterTitle">Sound lists</h2><label class="switch"><input type="checkbox" id="trkFilterUse" checked={doc?.use_filter !== false} aria-label="Use the sound lists" onchange={(e) => { const box = e.currentTarget; edit((d) => { d.use_filter = box.checked; return d; }).then((r) => { if (r.err) box.checked = doc?.use_filter !== false; }); }}><span class="sl"></span></label></div>
  <p class="hint">Name the items you hunt. A list's sound plays for them whatever their rarity or grade; an earlier list wins when an item is on two.</p>
  <div class="trk-actions trk-menu-host">
    {#if filters.length}
      <select class="style-select" id="trkFilter" aria-label="Filter" value={active?.id ?? ''} onchange={(e) => pickFilter(e.currentTarget.value)}>
        {#if !active}<option value="" disabled>Pick a filter</option>{/if}
        {#each filters as f (f.id)}<option value={f.id}>{f.name}</option>{/each}
      </select>
    {/if}
    <button type="button" class="trk-quiet" id="trkFilterNew" onclick={newFilter}>New filter</button>
    <button type="button" class="trk-quiet trk-icon-button" id="trkFilterMore" aria-label="More filter actions: duplicate, import, export, delete" aria-haspopup="menu" aria-expanded={menuOpen ? 'true' : 'false'} bind:this={menuButton} onclick={() => (menuOpen = !menuOpen)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 8h.01M8 8h.01M12.5 8h.01"/></svg></button>
    {#if menuOpen}
      <div class="trk-menu" role="menu" aria-label="Filter actions" tabindex="-1" onkeydown={menuKeys}>
        <button type="button" role="menuitem" id="trkFilterDuplicate" disabled={!active} onclick={duplicate}>Duplicate</button>
        <button type="button" role="menuitem" id="trkFilterExport" disabled={!active || busy} onclick={exportActive}>Copy to the clipboard</button>
        <button type="button" role="menuitem" id="trkFilterImportFile" disabled={busy} onclick={() => importFile?.click()}>Import a file…</button>
        <button type="button" role="menuitem" id="trkFilterImportPaste" disabled={busy} onclick={importClipboard}>Import from the clipboard</button>
        <button type="button" role="menuitem" id="trkFilterDelete" disabled={!active} onclick={() => { menuOpen = false; confirming = 'filter'; }}>Delete…</button>
      </div>
    {/if}
    <input type="file" class="visually-hidden" id="trkFilterImportInput" accept=".json,application/json" tabindex="-1" aria-label="Import a sound filter file" bind:this={importFile} onchange={importChosen}>
  </div>

  {#if confirming === 'filter' && active}
    <div class="trk-confirm" role="group" aria-label="Delete this filter?">
      <span>Delete {active.name}, its {lists.length} list{lists.length === 1 ? '' : 's'} and their sounds?</span>
      <button type="button" class="btn primary" id="trkFilterDeleteYes" onclick={deleteFilter}>Delete filter</button>
      <button type="button" class="btn" onclick={() => (confirming = null)}>Keep it</button>
    </div>
  {/if}

  {#if active}
    <div class="trk-line"><span class="trk-line-label">Filter name</span><input class="trk-field" id="trkFilterName" type="text" value={active.name} aria-label="Filter name" maxlength="60" autocomplete="off"
      onchange={(e) => rename(e.currentTarget.value)} onkeydown={(e) => { if (e.key === 'Enter') e.currentTarget.blur(); }}></div>
    <div class="trk-lists" role="group" aria-label="Lists in this filter">
      {#each lists as l (l.id)}<button type="button" data-list={l.id} aria-pressed={list?.id === l.id ? 'true' : 'false'} onclick={() => { pickedList = l.id; confirming = null; }}>{l.name}</button>{/each}
      <button type="button" class="trk-quiet trk-icon-button" id="trkListAdd" aria-label="Add a list" onclick={newList}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 3v10M3 8h10"/></svg></button>
    </div>
    {#if list}
      <div class="trk-line"><span class="trk-line-label">List name</span><input class="trk-field" id="trkListName" type="text" value={list.name} aria-label="List name" maxlength="60" autocomplete="off"
        onchange={(e) => setField('name', e.currentTarget.value)} onkeydown={(e) => { if (e.key === 'Enter') e.currentTarget.blur(); }}></div>
      <div class="trk-line"><span class="trk-line-label">Plays</span>
        <label class="switch slider-switch" data-live={list.enabled !== false ? '' : undefined}><input type="checkbox" id="trkListOn" checked={list.enabled !== false} aria-label={'Play ' + list.name + "'s sound"}
          onchange={(e) => { const box = e.currentTarget; setField('enabled', box.checked).then((r) => { if (r?.err) box.checked = list.enabled !== false; }); }}><span class="sl"></span></label>
        <div class="range-control">
          <input type="range" id="trkListVolume" min="0" max="100" step="1" value={pct} aria-label={list.name + ' volume'}
            oninput={(e) => (dragging = Number(e.currentTarget.value))} onchange={(e) => writeVolume(Number(e.currentTarget.value))}>
          <div class="value-stepper">
            <button type="button" class="step-button" aria-label={'Quieter ' + list.name} onclick={() => step(-1)}>−</button>
            <span class="val" class:off={list.enabled === false}>{list.enabled === false ? 'off' : pct + '%'}</span>
            <button type="button" class="step-button" aria-label={'Louder ' + list.name} onclick={() => step(1)}>+</button>
          </div>
        </div>
      </div>
      <div class="trk-actions">
        <span class="hint">Sound: {#if source}<b class="trk-count">{source}</b>{:else}the item's rarity sound{/if}</span>
        <button type="button" class="trk-quiet" id="trkListTest" disabled={!source} onclick={test}>Test</button>
        <button type="button" class="trk-quiet" disabled={busy} onclick={() => listFile?.click()}>Choose a file…</button>
        {#if source}<button type="button" class="trk-quiet" id="trkListClear" onclick={clearSound}>Use the rarity's sound</button>{/if}
        <input type="file" class="visually-hidden" id="trkListFile" accept={SOUND_ACCEPT} tabindex="-1" aria-label={'Choose a sound file for ' + list.name} bind:this={listFile} onchange={chosenSound}>
      </div>
      <div class="trk-actions">
        <button type="button" class="trk-quiet trk-icon-button" id="trkListEarlier" aria-label="Move this list earlier" disabled={lists[0]?.id === list.id} onclick={() => move(-1)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M10 3.5 5.5 8l4.5 4.5"/></svg></button>
        <button type="button" class="trk-quiet trk-icon-button" id="trkListLater" aria-label="Move this list later" disabled={lists.at(-1)?.id === list.id} onclick={() => move(1)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M6 3.5 10.5 8 6 12.5"/></svg></button>
        <button type="button" class="trk-quiet" id="trkListDelete" onclick={() => (confirming = 'list')}>Delete list…</button>
      </div>
      {#if confirming === 'list'}
        <div class="trk-confirm" role="group" aria-label="Delete this list?">
          <span>Delete {list.name}{list.items.length ? ` and its ${list.items.length} item${list.items.length === 1 ? '' : 's'}` : ''}?</span>
          <button type="button" class="btn primary" id="trkListDeleteYes" onclick={deleteList}>Delete list</button>
          <button type="button" class="btn" onclick={() => (confirming = null)}>Keep it</button>
        </div>
      {/if}
      <form class="trk-entry-line" onsubmit={addTyped}>
        <input class="trk-field" id="trkItemField" type="search" list="trkItemNames" placeholder="Add an item by name, or narrow the list" aria-label="Add an item to this list" maxlength="120" autocomplete="off" bind:value={query}>
        <button type="submit" class="btn" id="trkItemAdd">Add</button>
      </form>
      <datalist id="trkItemNames">{#each names as n (n)}<option value={n}></option>{/each}</datalist>
      {#if shown.length}
        <ul class="trk-names trk-items">
          {#each shown as name (name)}
            <li data-name={name}>
              <span class={nameClass(name)}>{name}</span>
              <span class="trk-actions">{#if alsoOn(name)}<span class="note">also on {alsoOn(name)}</span>{/if}<button type="button" class="trk-quiet trk-icon-button" aria-label={'Remove ' + name} onclick={() => remove(name)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="m4.5 4.5 7 7M11.5 4.5l-7 7"/></svg></button></span>
            </li>
          {/each}
        </ul>
      {:else if (list.items ?? []).length}
        <p class="trk-empty">No item on {list.name} matches.</p>
      {:else}
        <p class="trk-empty">{list.name} names no items yet. Type a name above and press Enter.</p>
      {/if}
    {/if}
  {:else if filters.length}
    <p class="trk-empty">No filter is in use. Pick one above to hear its lists.</p>
  {:else}
    <p class="trk-empty">No sound lists yet. New filter makes one, or import a filter exported from ForgePact or HS Offline Tracker.</p>
  {/if}
  {#if error}<p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{error}</p>
  {:else if notice}<p class="note" role="status">{notice}</p>{/if}
</section>

<style>
  .trk-actions { display: flex; flex-wrap: wrap; align-items: center; gap: var(--space-4); }
  .trk-menu-host { position: relative; }
  .trk-menu {
    position: absolute;
    top: calc(100% + var(--space-2));
    right: 0;
    z-index: 5;
    display: flex;
    flex-direction: column;
    min-width: 200px;
    padding: var(--space-2);
    border: 1px solid var(--color-border-subtle);
    border-radius: var(--radius-md);
    background-color: var(--color-bg-raised);
  }
  .trk-menu button { min-height: 32px; padding: 0 var(--space-5); border: 0; border-radius: var(--radius-sm); background: none; color: var(--color-text-primary); font: inherit; font-size: var(--font-size-sm); text-align: left; cursor: pointer; }
  .trk-menu button:disabled { color: var(--color-text-faint); cursor: default; }
  .trk-confirm { display: flex; flex-wrap: wrap; align-items: center; gap: var(--space-4); font-size: var(--font-size-sm); }
  .trk-line { display: flex; align-items: center; gap: var(--space-6); min-width: 0; }
  .trk-line-label { flex: none; width: 88px; color: var(--color-text-muted); font-size: var(--font-size-sm); }
  .trk-line .trk-field { flex: 1; }
  .trk-line .range-control { flex: 1; }
  .trk-lists { display: flex; flex-wrap: wrap; gap: var(--space-1); justify-self: start; align-self: flex-start; padding: var(--space-2); border-radius: var(--radius-md); background-color: var(--color-bg-sunken); }
  .trk-lists button[data-list] { min-height: 24px; padding: 0 var(--space-5); border: 0; border-radius: var(--radius-sm); background: none; color: var(--color-text-muted); font: inherit; font-size: var(--font-size-sm); cursor: pointer; }
  .trk-lists button[aria-pressed="true"] { background-color: var(--color-bg-hover); color: var(--color-text-primary); }
  .trk-count { color: var(--color-text-primary); font-family: var(--font-mono-family), Consolas, monospace; font-weight: var(--font-weight-regular); }
  .trk-field {
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
  .trk-entry-line { display: flex; gap: var(--space-4); }
  .trk-entry-line .trk-field { flex: 1; }
  .trk-names { display: flex; flex-direction: column; margin: 0; padding: 0; list-style: none; }
  .trk-names li { display: flex; align-items: center; justify-content: space-between; gap: var(--space-6); min-height: 32px; padding: 0 0 0 var(--space-4); border-radius: var(--radius-sm); font-size: var(--font-size-sm); }
  .trk-names .note { margin: 0; }
  .trk-quiet {
    display: inline-flex;
    flex: none;
    align-items: center;
    gap: var(--space-3);
    min-height: 28px;
    padding: 0 var(--space-4);
    border: 0;
    border-radius: var(--radius-sm);
    background: none;
    color: var(--color-text-muted);
    font: inherit;
    font-size: var(--font-size-sm);
    white-space: nowrap;
    cursor: pointer;
  }
  .trk-quiet:disabled { cursor: default; color: var(--color-text-faint); }
  .trk-quiet svg { width: 16px; height: 16px; stroke: currentColor; stroke-width: 1.5; stroke-linecap: round; stroke-linejoin: round; fill: none; }
  .trk-icon-button { width: 28px; padding: 0; justify-content: center; }
  .trk-error { display: flex; align-items: center; gap: var(--space-4); color: var(--color-text-primary); }
  .trk-error svg { flex: none; width: 16px; height: 16px; color: var(--color-danger); stroke: currentColor; stroke-width: 1.4; fill: none; stroke-linecap: round; stroke-linejoin: round; }
  .trk-empty { margin: 0; padding: var(--space-7) 0; color: var(--color-text-faint); font-size: var(--font-size-sm); }
  @media (hover: hover) and (pointer: fine) {
    .trk-names li:hover, .trk-menu button:hover:not(:disabled) { background-color: var(--color-bg-hover); }
    .trk-lists button[data-list]:hover { color: var(--color-text-primary); }
    .trk-quiet:hover:not(:disabled) { color: var(--color-text-primary); background-color: var(--color-bg-hover); }
  }
</style>
