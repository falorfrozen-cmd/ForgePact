<script>
  // The Tracker tab's Runs section (#185): the run history, the picked run and its run card.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `tab-runs` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="runs" aria-label="Runs"></section>
