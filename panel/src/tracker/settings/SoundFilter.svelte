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
  // - Deleting a filter or a list asks first, in place, as End run does on
  //   Session: focus goes to Keep it, and back to what asked once it is gone.
  // - More (…) is a disclosure, not a menu: a plain group of buttons under a
  //   trigger that carries aria-expanded. Escape closes it from the trigger or
  //   from inside, and a button that closes it hands focus back to the trigger.
  // - A control that goes away while it has focus hands it on: a removed
  //   item's to the next item (or the one before, or the field), a deleted
  //   list's to the list now shown.
  import { onMount, tick } from 'svelte';
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
  let itemField = $state(null);
  let section = $state(null);

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

  // Close More (…); focus goes back to its trigger unless a caller moves it on.
  function closeMenu(refocus = true) {
    menuOpen = false;
    if (refocus) menuButton?.focus();
  }

  // Focus the first element a selector finds in this card, once the page has drawn.
  async function focusIn(...selectors) {
    await tick();
    for (const selector of selectors) {
      const el = section?.querySelector(selector);
      if (el) { el.focus(); return; }
    }
  }

  async function ask(what) {
    closeMenu(false);
    confirming = what;
    await focusIn(what === 'filter' ? '#trkFilterDeleteNo' : '#trkListDeleteNo');
  }

  async function keep() {
    const what = confirming;
    confirming = null;
    await focusIn(what === 'filter' ? '#trkFilterMore' : '#trkListDelete');
  }

  function confirmKeys(e) {
    if (e.key === 'Escape') { e.preventDefault(); keep(); }
  }

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
    closeMenu();
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
    if (res.err) { await focusIn('#trkFilterMore'); return; }
    for (const key of keys) if (client.sounds?.[key]) await client.changeSound(key, { clear: true });
    pickedList = null;
    say(`Deleted ${name}.`);
    await focusIn('#trkFilterMore');
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
    closeMenu();
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
    closeMenu();
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (file) await importText(await file.text(), file.name);
  }

  async function importClipboard() {
    closeMenu();
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
    if (res.err) { await focusIn('#trkListDelete'); return; }
    if (key && client.sounds?.[key]) await client.changeSound(key, { clear: true });
    pickedList = null;
    say(`Deleted ${name}.`);
    await focusIn('.trk-lists button[aria-pressed="true"]', '#trkListAdd');
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
    const at = shown.indexOf(name);
    const res = await edit((d) => removeItem(d, filterId, listId, name));
    if (res.err) return;
    // The row went with its button: the next row's, else the one before, else the field.
    await tick();
    const rest = [...(section?.querySelectorAll('.trk-items li button') ?? [])];
    (rest[at] ?? rest[at - 1] ?? itemField)?.focus();
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

  // Escape closes More (…) from its trigger or from inside it.
  function menuKeys(e) {
    if (e.key === 'Escape' && menuOpen) { e.preventDefault(); closeMenu(); }
  }

  const alsoOn = (name) => (clash.get(name.toLowerCase()) ?? []).filter((n) => n !== list?.name).join(', ');
</script>

<section class="card trk-half" aria-labelledby="trkFilterTitle" id="trkFilters" bind:this={section}>
  <div class="trk-head"><h2 id="trkFilterTitle">Sound lists</h2><label class="switch"><input type="checkbox" id="trkFilterUse" checked={doc?.use_filter !== false} aria-label="Use the sound lists" onchange={(e) => { const box = e.currentTarget; edit((d) => { d.use_filter = box.checked; return d; }).then((r) => { if (r.err) box.checked = doc?.use_filter !== false; }); }}><span class="sl"></span></label></div>
  <p class="hint">Name the items you hunt. A list's sound plays for them whatever their rarity or grade; an earlier list wins when an item is on two.</p>
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="trk-actions trk-menu-host" onkeydown={menuKeys}>
    {#if filters.length}
      <select class="style-select" id="trkFilter" aria-label="Filter" value={active?.id ?? ''} onchange={(e) => pickFilter(e.currentTarget.value)}>
        {#if !active}<option value="" disabled>Pick a filter</option>{/if}
        {#each filters as f (f.id)}<option value={f.id}>{f.name}</option>{/each}
      </select>
    {/if}
    <button type="button" class="trk-quiet" id="trkFilterNew" onclick={newFilter}>New filter</button>
    <button type="button" class="trk-quiet trk-icon-button" id="trkFilterMore" aria-label="More filter actions: duplicate, import, export, delete" aria-expanded={menuOpen ? 'true' : 'false'} aria-controls="trkFilterMenu" bind:this={menuButton} onclick={() => (menuOpen = !menuOpen)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 8h.01M8 8h.01M12.5 8h.01"/></svg></button>
    {#if menuOpen}
      <div class="trk-menu" id="trkFilterMenu" role="group" aria-label="Filter actions">
        <button type="button" id="trkFilterDuplicate" disabled={!active} onclick={duplicate}>Duplicate</button>
        <button type="button" id="trkFilterExport" disabled={!active || busy} onclick={exportActive}>Copy to the clipboard</button>
        <button type="button" id="trkFilterImportFile" disabled={busy} onclick={() => importFile?.click()}>Import a file…</button>
        <button type="button" id="trkFilterImportPaste" disabled={busy} onclick={importClipboard}>Import from the clipboard</button>
        <button type="button" id="trkFilterDelete" disabled={!active} onclick={() => ask('filter')}>Delete…</button>
      </div>
    {/if}
    <input type="file" class="visually-hidden" id="trkFilterImportInput" accept=".json,application/json" tabindex="-1" aria-label="Import a sound filter file" bind:this={importFile} onchange={importChosen}>
  </div>

  {#if confirming === 'filter' && active}
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div class="trk-confirm" id="trkFilterConfirm" role="group" aria-labelledby="trkFilterConfirmTitle" onkeydown={confirmKeys}>
      <h3 class="trk-confirm-title" id="trkFilterConfirmTitle">Delete {active.name}, its {lists.length} list{lists.length === 1 ? '' : 's'} and their sounds?</h3>
      <div class="trk-actions">
        <button type="button" class="btn primary" id="trkFilterDeleteYes" onclick={deleteFilter}>Delete filter</button>
        <button type="button" class="btn" id="trkFilterDeleteNo" onclick={keep}>Keep it</button>
      </div>
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
        <label class="switch slider-switch" data-live={list.enabled !== false ? '' : undefined}><input type="checkbox" id="trkListOn" checked={list.enabled !== false} aria-label={'Plays: ' + list.name}
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
        <button type="button" class="trk-quiet" id="trkListDelete" onclick={() => ask('list')}>Delete list…</button>
      </div>
      {#if confirming === 'list'}
        <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
        <div class="trk-confirm" id="trkListConfirm" role="group" aria-labelledby="trkListConfirmTitle" onkeydown={confirmKeys}>
          <h3 class="trk-confirm-title" id="trkListConfirmTitle">Delete {list.name}{list.items.length ? ` and its ${list.items.length} item${list.items.length === 1 ? '' : 's'}` : ''}?</h3>
          <div class="trk-actions">
            <button type="button" class="btn primary" id="trkListDeleteYes" onclick={deleteList}>Delete list</button>
            <button type="button" class="btn" id="trkListDeleteNo" onclick={keep}>Keep it</button>
          </div>
        </div>
      {/if}
      <form class="trk-entry-line" onsubmit={addTyped}>
        <input class="trk-field" id="trkItemField" type="search" list="trkItemNames" placeholder="Add an item by name, or narrow the list" aria-label="Add an item to this list" maxlength="120" autocomplete="off" bind:value={query} bind:this={itemField}>
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
  .trk-menu-host { position: relative; }
  /* More (…)'s list floats over the card, so it takes the theme picker's list's
     look (DESIGN.md § Theme picker): the allowlisted 1px subtle edge, 8px
     corners and the Tray shadow. */
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
    border-radius: var(--radius-lg);
    background-color: var(--color-bg-raised);
    box-shadow: 0 12px 32px color-mix(in srgb, var(--color-bg-sunken) 80%, transparent);
  }
  .trk-menu button { min-height: 32px; padding: 0 var(--space-5); border: 0; border-radius: var(--radius-sm); background: none; color: var(--color-text-primary); font: inherit; font-size: var(--font-size-sm); text-align: left; cursor: pointer; }
  .trk-menu button:disabled { color: var(--color-text-faint); cursor: default; }
  .trk-line { display: flex; align-items: center; gap: var(--space-6); min-width: 0; }
  .trk-line-label { flex: none; width: 88px; color: var(--color-text-muted); font-size: var(--font-size-sm); }
  .trk-line .trk-field { flex: 1; }
  .trk-line .range-control { flex: 1; }
  .trk-lists { display: flex; flex-wrap: wrap; gap: var(--space-1); justify-self: start; align-self: flex-start; padding: var(--space-2); border-radius: var(--radius-md); background-color: var(--color-bg-sunken); }
  .trk-lists button[data-list] { min-height: 24px; padding: 0 var(--space-5); border: 0; border-radius: var(--radius-sm); background: none; color: var(--color-text-muted); font: inherit; font-size: var(--font-size-sm); cursor: pointer; }
  .trk-lists button[aria-pressed="true"] { background-color: var(--color-bg-hover); color: var(--color-text-primary); }
  .trk-count { color: var(--color-text-primary); font-family: var(--font-mono-family), Consolas, monospace; font-weight: var(--font-weight-regular); }
  .trk-names .note { margin: 0; }
  @media (hover: hover) and (pointer: fine) {
    .trk-menu button:hover:not(:disabled) { background-color: var(--color-bg-hover); }
    .trk-lists button[data-list]:hover { color: var(--color-text-primary); }
  }
</style>
