<script>
  // The Tracker tab's Shopping list section (#185): what the player still
  // needs, ported from HS Offline Tracker's Shop view (MIT, Q7(b)) in
  // ForgePact's design (design/tracker/tab.html, "Shopping list").
  //
  // The list lives in the tracker store's `shopping.json`, read and written
  // whole through `/api/tracker/shopping` (client.loadShopping/saveShopping).
  // Unlike the Tracker, nothing changes here before the server agrees: an add
  // or a remove posts the whole new list and shows what the server answered,
  // so a refusal (a full list, an entry too long) leaves the list as saved and
  // says why, with the typed entry kept in the field. A blank entry is never
  // sent. Clicking an entry copies its text to the clipboard, as the Tracker's
  // did, for the trade chat or a search. Nothing here polls.
  //
  // Removing an entry hands focus to the next entry's Remove (or the one
  // before, or the field), since the button that had it went with the row.
  // "Copied" is said through one status line that is always there, so a
  // screen reader hears it; the word beside the entry is for the eye.
  import { onMount, tick } from 'svelte';

  let { client } = $props();

  const MAX_CHARS = 120;

  let items = $state([]);
  let loaded = $state(false);
  let draft = $state('');
  let saving = $state(false);
  let error = $state(null);
  let copied = $state(null);
  let copyTimer;
  let field = $state(null);
  let list = $state(null);

  const countText = $derived(`${items.length} ${items.length === 1 ? 'item' : 'items'}`);

  onMount(() => {
    client.loadShopping().then((res) => {
      if (res.err) error = res.err;
      else items = Array.isArray(res.items) ? res.items : [];
      loaded = true;
    });
    return () => clearTimeout(copyTimer);
  });

  // Post the whole list; on success the server's answer is the list.
  async function save(next) {
    saving = true;
    try {
      const res = await client.saveShopping(next);
      if (res.err) { error = res.err; return false; }
      error = null;
      items = Array.isArray(res.items) ? res.items : next;
      copied = null;
      return true;
    } finally {
      saving = false;
    }
  }

  async function add(e) {
    e.preventDefault();
    const text = draft.trim();
    if (!text || saving) return;
    if (await save([...items, text])) draft = '';
  }

  async function remove(i) {
    if (saving) return;
    if (!(await save(items.filter((_, j) => j !== i)))) return;
    await tick();
    const rest = [...(list?.querySelectorAll('li button[aria-label^="Remove "]') ?? [])];
    (rest[i] ?? rest[i - 1] ?? field)?.focus();
  }

  async function writeClipboard(text) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch (e) {
      // No clipboard API (or no permission): the selection route.
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

  async function copy(i) {
    if (!(await writeClipboard(items[i]))) { error = 'Could not copy to the clipboard.'; return; }
    copied = i;
    clearTimeout(copyTimer);
    copyTimer = setTimeout(() => (copied = null), 900);
  }
</script>

<section class="trk-section" data-section="shopping" aria-label="Shopping list" data-loaded={loaded ? '' : undefined}>
  <section class="card trk-half" aria-labelledby="trkShopTitle">
    <div class="trk-head"><h2 id="trkShopTitle">Shopping list</h2>{#if items.length}<span class="trk-count">{countText}</span>{/if}</div>
    <p class="hint">What you still need. Click an entry to copy its name for the trade chat or a search.</p>
    <form class="trk-entry-line" onsubmit={add}>
      <input class="trk-field" id="trkShopField" type="text" placeholder="Add an item" aria-label="Add an item to the shopping list"
        maxlength={MAX_CHARS} autocomplete="off" bind:value={draft} bind:this={field}>
      <button type="submit" class="btn" disabled={saving}>Add</button>
    </form>
    {#if error}
      <p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{error}</p>
    {/if}
    {#if items.length}
      <ul class="trk-names trk-shop" bind:this={list}>
        {#each items as item, i (i + '\u0000' + item)}
          <li>
            <button type="button" class="trk-copy" title="Copy" onclick={() => copy(i)}>{item}</button>
            {#if copied === i}<span class="trk-copied" aria-hidden="true">Copied</span>{/if}
            <button type="button" class="trk-quiet trk-icon-button" aria-label={'Remove ' + item} disabled={saving} onclick={() => remove(i)}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="m4.5 4.5 7 7M11.5 4.5l-7 7"/></svg></button>
          </li>
        {/each}
      </ul>
    {:else if loaded}
      <p class="trk-empty">Nothing on the list. Add what you still need; an entry copies its name when you click it.</p>
    {/if}
    <p class="trk-live" role="status">{copied !== null && items[copied] !== undefined ? `Copied ${items[copied]}` : ''}</p>
  </section>
</section>

<style>
  /* design/tracker/tracker.css's shopping list rules, tokens only; the field,
     the list and the quiet buttons are the Tracker's shared ones in app.css. */
  .trk-copy { flex: 1; min-width: 0; padding: 0; border: 0; background: none; color: var(--color-text-primary); font: inherit; text-align: left; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; cursor: copy; }
  .trk-copied { flex: none; color: var(--color-text-muted); font-size: var(--font-size-xs); }
</style>
