<script>
  // The Tracker tab's Shopping list section (#185): the shopping list.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `shop` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="shopping" aria-label="Shopping list"></section>
