<script>
  // The Tracker tab's Alerts section (#185): the alert rows, the lowest grade, the zone-rotation buff pick and the sound lists.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `tab-settings` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="alerts" aria-label="Alerts"></section>
