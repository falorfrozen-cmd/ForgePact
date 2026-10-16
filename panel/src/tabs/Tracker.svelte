<script>
  // The Tracker tab (#185): HS Offline Tracker's features in ForgePact. The
  // `mod_tracker` switch heads it in a card of its own (panel.js wires it like
  // every boolean mod), then the sub-tabs, one section component each
  // (panel/DESIGN.md § Tracker, the comps in design/tracker/).
  //
  // One TrackerClient (tracker/client.svelte.js) is made here and handed to
  // every section and to startAlerts. It polls `/api/tracker` only while the
  // switch is on or this tab is open, so with the switch off and another tab
  // open the page sends no tracker request at all.
  //
  // Everything but the switch card sits in #trackerSections. Those controls
  // write `/api/tracker/*` and send no plugin command, so the behaviour
  // oracle's walk skips that one container (tests/oracle.mjs) and
  // `npm run e2e:tracker` pins them instead, and Ember's search leaves them
  // out. The sections are mounted only while the tab is open: none of them
  // reads or repaints behind another tab.
  import { onMount } from 'svelte';
  import { activeTab } from '../nav.js';
  import { ST } from '../state.svelte.js';
  import { TrackerClient, stateLine } from '../tracker/client.svelte.js';
  import { startAlerts } from '../tracker/alerts.js';
  import Session from '../tracker/Session.svelte';
  import Runs from '../tracker/Runs.svelte';
  import AlertSettings from '../tracker/AlertSettings.svelte';
  import OverlaySettings from '../tracker/OverlaySettings.svelte';
  import Shop from '../tracker/Shop.svelte';
  import Codex from '../tracker/Codex.svelte';

  const SECTIONS = [
    { key: 'session', label: 'Session' },
    { key: 'runs', label: 'Runs' },
    { key: 'alerts', label: 'Alerts' },
    { key: 'overlay', label: 'Overlay' },
    { key: 'shopping', label: 'Shopping list' },
    { key: 'codex', label: 'Item codex' },
  ];
  const STORED = 'forgepact_tracker_subtab';

  function storedSection() {
    try {
      const key = sessionStorage.getItem(STORED);
      if (SECTIONS.some((s) => s.key === key)) return key;
    } catch (e) {}
    return 'session';
  }

  const client = new TrackerClient({
    switchOn: () => !!ST?.cfg?.mod_tracker,
    tabOpen: () => activeTab === 'tracker',
  });
  const initial = storedSection();
  let section = $state(initial);
  // Set without loading anything: the first poll reads the runs if Runs is open.
  client.section = initial;
  let tabOpen = $state(false);
  const line = $derived(stateLine({ on: client.on, snapshot: client.snapshot, session: client.session, catching_up: client.catching_up }));

  function open(key, focus = false) {
    section = key;
    client.openSection(key);
    try { sessionStorage.setItem(STORED, key); } catch (e) {}
    if (focus) document.getElementById('trk-tab-' + key)?.focus();
  }

  function keys(e, index) {
    const direction = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
    if (!direction && !['Home', 'End'].includes(e.key)) return;
    e.preventDefault();
    const next = e.key === 'Home' ? 0 : e.key === 'End' ? SECTIONS.length - 1 : (index + direction + SECTIONS.length) % SECTIONS.length;
    open(SECTIONS[next].key, true);
  }

  onMount(() => {
    const navigated = () => { tabOpen = activeTab === 'tracker'; };
    document.addEventListener('forgepact:navigate', navigated);
    navigated();
    client.start();
    const stopAlerts = startAlerts(client);
    return () => {
      document.removeEventListener('forgepact:navigate', navigated);
      stopAlerts();
      client.stop();
    };
  });
</script>

<!-- Tracker tab: the switch card, then the sections. The container draws no surface; each section is its own card. -->
<div class="tab-card trk-tab" data-tab="tracker" id="trackerTab">
  <section class="card" id="trackerCard" aria-labelledby="trkSwitchTitle">
    <div class="density-top">
      <h2 id="trkSwitchTitle">Session tracker</h2>
      <!-- The row's first text names the Enabled mods entry ("Tracker"); sighted readers see "Enabled", which the switch's name ends with. -->
      <div class="row"><span class="lbl"><span class="visually-hidden">Tracker</span><span aria-hidden="true">Enabled</span></span><label class="switch"><input type="checkbox" id="mod_tracker" aria-label="Session tracker enabled"><span class="sl"></span></label><span class="val off" id="mtrval">off</span></div>
    </div>
    <p class="hint">Counts the gold, experience, kills and the five rarities you find while you play, keeps a journal of those drops, and files a run when a session ends. It only reads the game. Off by default.</p>
    <p class="trk-state" id="trackerState" role="status"><span class="trk-dot {line.tone}" aria-hidden="true"></span>{line.text}</p>
  </section>

  <div id="trackerSections" class="trk-sections">
    <div class="subtabbar trk-subtabs" role="tablist" aria-label="Tracker sections">
      {#each SECTIONS as s, i (s.key)}
        <button type="button" class="subtabbtn trk-subtab" class:active={section === s.key} role="tab" id={'trk-tab-' + s.key}
          aria-controls={'trk-panel-' + s.key} aria-selected={section === s.key ? 'true' : 'false'} tabindex={section === s.key ? 0 : -1}
          onclick={() => open(s.key)} onkeydown={(e) => keys(e, i)}>{s.label}</button>
      {/each}
    </div>
    <div class="trk-panel" role="tabpanel" id={'trk-panel-' + section} aria-labelledby={'trk-tab-' + section}>
      {#if tabOpen}
        {#if section === 'session'}<Session {client} />
        {:else if section === 'runs'}<Runs {client} />
        {:else if section === 'alerts'}<AlertSettings {client} />
        {:else if section === 'overlay'}<OverlaySettings {client} />
        {:else if section === 'shopping'}<Shop {client} />
        {:else if section === 'codex'}<Codex {client} />
        {/if}
      {/if}
    </div>
  </div>
</div>
