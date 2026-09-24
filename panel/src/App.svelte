<script>
  // The page shell: sidebar, status bar, page heading, the search toolbar and
  // the workspace every tab's cards sit in. The markup is the old page's, one
  // for one; src/panel.js wires it up once it is mounted, as the old inline
  // script did. The tab components follow the order the old page's cards
  // ended up in once preparePanelUI() had re-appended World's cards last.
  import { ICON_SPRITE } from './icons.js';
  import { openTab } from './nav.js';
  import Setup from './tabs/Setup.svelte';
  import Loot from './tabs/Loot.svelte';
  import Modifiers from './tabs/Modifiers.svelte';
  import Mods from './tabs/Mods.svelte';
  import World from './tabs/World.svelte';
</script>

{@html ICON_SPRITE}<div id="appShell">
<aside class="sidebar" aria-label="ForgePact navigation">
  <!-- Anvil adapted from Falor's toolkit ToolIcon.svelte; closed body and continuous top face. -->
  <div class="brand"><svg viewBox="0 0 80 80" fill="none" aria-hidden="true"><defs><linearGradient id="forge-anvil" x1="15" y1="8" x2="65" y2="73" gradientUnits="userSpaceOnUse"><stop stop-color="#efc79b"/><stop offset=".48" stop-color="#b27a48"/><stop offset="1" stop-color="#563d2c"/></linearGradient></defs><g stroke="#efc79b" stroke-width="1.3" stroke-linejoin="round" stroke-linecap="round"><path d="M39 3 51 17 40 31 29 17Z" fill="url(#forge-anvil)"/><path d="m40 9-5 8 5 8 5-8Z" fill="#141619"/><path d="M28 27H47V30H70C68 38 60 42 47 43V55L55 64H28L35 55V43H24C15 43 8 38 3 30H28Z" fill="url(#forge-anvil)"/><path d="M3 30H70L66 34H8Z" fill="#d8aa7b"/><path d="M30 64h23l5 7H24Z" fill="url(#forge-anvil)"/><path d="M39 34v25m-7 8h19" opacity=".7"/></g></svg><div><div class="brand-name">FORGEPACT</div><div class="brand-sub">HERO SIEGE TOOLS</div></div></div>
  <!-- svelte-ignore a11y_no_noninteractive_element_to_interactive_role (the old page's markup, kept as is; restyling is a later change) -->
  <nav class="tabbar" role="tablist" aria-label="ForgePact categories" aria-orientation="vertical"><button class="tabbtn" data-tab="setup" role="tab" id="nav-setup" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M10 3h4l1 3 3 1 3 2v4l-3 2-1 3-3 3h-4l-1-3-3-1-3-2v-4l3-2 1-3zM15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0"/></svg>Setup</button>
<button class="tabbtn" data-tab="modifiers" role="tab" id="nav-modifiers" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 6h18M3 12h18M3 18h18M7 3v6M16 9v6M10 15v6"/></svg>Modifiers</button>
<button class="tabbtn" data-tab="world" role="tab" id="nav-world" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18"/></svg>World</button>
<button class="tabbtn" data-tab="loot" role="tab" id="nav-loot" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h16v12H4zM3 8l3-5h12l3 5M9 8v5h6V8"/></svg>Loot</button>
<button class="tabbtn" data-tab="mods" role="tab" id="nav-mods" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 4h6V2a3 3 0 0 1 6 0v2h5v6h-2a3 3 0 0 0 0 6h2v5h-6v-2a3 3 0 0 0-6 0v2H4v-6H2a3 3 0 0 1 0-6h2Z"/></svg>Mods</button>
</nav>
  <div class="sidebar-foot"><hr><p>Offline tools</p><p>Created by Falor</p><p id="panelver"></p></div>
</aside>
<main id="wrap">
  <div class="control-dock"><div class="topline">
    <div class="breadcrumb">ForgePact / <strong id="breadcrumbPage">Modifiers</strong></div>
    <div id="statusbar"><span class="chip" id="chipGame">Connecting...</span><span id="saveIndicator" role="status" aria-live="polite">Loading settings...</span></div>
  </div></div>
  <div id="pluginWarning" class="plugin-warning" role="status" hidden><span id="pluginWarningText"></span><button class="btn" type="button" onclick={() => openTab('setup')}>Open Setup</button></div>
  <div class="page-heading"><div><h1 id="pageTitle">Character modifiers</h1><p id="pageDescription">Tune your character and combat bonuses.</p></div>
    <div class="page-actions"><label class="auto-control">Auto-apply <span class="switch"><input type="checkbox" id="autoapply" aria-label="Auto-apply on game launch"><span class="sl"></span></span></label><button class="btn" id="applyall" title="Send all saved settings to the game">Apply all now</button></div>
  </div>
  <div id="controlToolbar" hidden><input type="search" id="controlSearch" class="control-search" placeholder="Search settings by name or effect..." aria-label="Search settings in this section"><div class="control-filters" role="group" aria-label="Filter settings"><button data-control-filter="all" aria-pressed="true">All settings</button><button data-control-filter="modified" aria-pressed="false">Modified</button></div></div>
  <div id="workspace" role="tabpanel" aria-labelledby="nav-modifiers">
<div id="modsSubtabs" class="subtabbar" role="tablist" aria-label="Mods categories" hidden><button type="button" class="subtabbtn" role="tab" id="subtab-qol" aria-controls="qolCard" aria-selected="true" tabindex="0">Quality of Life</button><button type="button" class="subtabbtn" role="tab" id="subtab-items" aria-controls="itemsCard" aria-selected="false" tabindex="-1">Items</button></div>
<Setup />
<Loot />
<Modifiers />
<Mods />
<World />
</div>
<div class="note" id="chipApply" role="status"></div>
</main></div>
<div id="toast" role="status"></div>
