<script>
  // The Tracker tab's Item codex section (#185): the item codex.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `codex` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="codex" aria-label="Item codex"></section>
