<script>
  // The Tracker tab's Session section (#185): the session header, totals and rates, loot, notable counts, tallies, the Satanic zone, the series and the journal.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `tab-session` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="session" aria-label="Session"></section>
