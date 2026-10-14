<script>
  // The Tracker tab's Overlay section (#185): the overlay window's and the drop card's settings, and Discord status.
  // A stub that renders an empty section, so the tab builds before its owner,
  // the `overlay` item, replaces it. Tracker.svelte passes the one client
  // (client.svelte.js) every section reads and calls through.
  let { client } = $props();
</script>

<section class="trk-section" data-section="overlay" aria-label="Overlay"></section>
