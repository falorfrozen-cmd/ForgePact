<script>
  // The page shell: the top rail (the <aside class="sidebar">, restyled as a
  // rail), the status bar, page heading, the search toolbar and the
  // workspace every tab's cards sit in. The markup is the old page's;
  // src/panel.js wires it up once it is mounted, as the old inline script
  // did. The tab components follow the order the old page's cards ended up
  // in once preparePanelUI() had re-appended World's cards last.
  // Added since the port: the "Enabled mods" list under the dock (the theme
  // choice, first added to the status bar, is on Setup since the owner's
  // polish pass). The restyle moved the apply chip and the credits line into
  // the status bar (app.css places #chipGame in the rail) and left every id
  // where the tests and the oracle look for it. The plugin warning is an icon
  // beside Apply all, and a second one beside the save indicator, each with a
  // tooltip (src/lib/plugin-warning.js); status() in panel.js still decides
  // when #pluginWarning shows and what it says. The Setup and Mods rail icons
  // are the design's ring and 2x2 grid (finish review F1).
  import { ICON_SPRITE, WARN_ICON } from './icons.js';
  import { openTab } from './nav.js';
  import Setup from './tabs/Setup.svelte';
  import Loot from './tabs/Loot.svelte';
  import Modifiers from './tabs/Modifiers.svelte';
  import Mods from './tabs/Mods.svelte';
  import World from './tabs/World.svelte';
  import Overview from './ember/Overview.svelte';
</script>

{@html ICON_SPRITE}<div id="appShell">
<aside class="sidebar" aria-label="ForgePact navigation">
  <!-- The design's mark: two nested diamonds, drawn in the palette's accent (currentColor). -->
  <div class="brand"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" aria-hidden="true"><path d="M12 2.5 21.5 12 12 21.5 2.5 12Z"/><path d="M12 7.5 16.5 12 12 16.5 7.5 12Z"/></svg><div><div class="brand-name">FORGEPACT</div><div class="brand-sub">HERO SIEGE TOOLS</div></div></div>
  <!-- svelte-ignore a11y_no_noninteractive_element_to_interactive_role (the old page's markup, kept as is; restyling is a later change) -->
  <nav class="tabbar" role="tablist" aria-label="ForgePact categories" aria-orientation="horizontal"><button class="tabbtn" data-tab="setup" role="tab" id="nav-setup" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20 12a8 8 0 1 1-16 0 8 8 0 0 1 16 0M14.5 12a2.5 2.5 0 1 1-5 0 2.5 2.5 0 0 1 5 0"/></svg>Setup</button>
<button class="tabbtn" data-tab="modifiers" role="tab" id="nav-modifiers" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 6h18M3 12h18M3 18h18M7 3v6M16 9v6M10 15v6"/></svg>Modifiers</button>
<button class="tabbtn" data-tab="world" role="tab" id="nav-world" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18"/></svg>World</button>
<button class="tabbtn" data-tab="loot" role="tab" id="nav-loot" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h16v12H4zM3 8l3-5h12l3 5M9 8v5h6V8"/></svg>Loot</button>
<button class="tabbtn" data-tab="mods" role="tab" id="nav-mods" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4.5 4.5h6v6h-6zM13.5 4.5h6v6h-6zM4.5 13.5h6v6h-6zM13.5 13.5h6v6h-6z"/></svg>Mods</button>
</nav>
</aside>
<main id="wrap" tabindex="0" aria-labelledby="pageTitle">
  <div class="control-dock"><div class="topline">
    <div class="breadcrumb">ForgePact / <strong id="breadcrumbPage">Modifiers</strong></div>
    <div id="statusbar"><span class="chip" id="chipGame">Connecting...</span><span id="saveIndicator" role="status" aria-live="polite">Loading settings...</span><span class="plugin-warning status-warning" hidden><button type="button" class="plugin-warning-button" aria-label="Open Setup" aria-describedby="statusWarningText" onclick={() => openTab('setup')}>{@html WARN_ICON}</button><span class="plugin-warning-tooltip" role="tooltip" hidden><span id="statusWarningText"></span><span class="plugin-warning-action">Open Setup</span></span></span><div class="note" id="chipApply" role="status"></div><div class="status-foot"><span class="status-offline">Offline tools</span><span>Created by Falor and ST4H<span id="panelver"></span></span></div></div>
  </div></div>
  <!-- Filled by src/lib/enabled-mods-list.js from the saved settings, in boot() and after every save. -->
  <section id="enabledMods" aria-label="Enabled mods"><h2>Enabled mods</h2><span id="enabledModsCount">0 on</span><p class="enabled-mods-empty">Nothing is on</p></section>
  <div class="page-heading"><div><h1 id="pageTitle">Character modifiers</h1><p id="pageDescription">Tune your character and combat bonuses.</p></div>
    <div class="page-actions"><label class="auto-control">Auto-apply <span class="switch"><input type="checkbox" id="autoapply" aria-label="Auto-apply on game launch"><span class="sl"></span></span></label><button class="btn" id="applyall" title="Send all saved settings to the game">Apply all now</button><div id="pluginWarning" class="plugin-warning" role="status" hidden><button type="button" class="plugin-warning-button" aria-label="Open Setup" aria-describedby="pluginWarningText" onclick={() => openTab('setup')}>{@html WARN_ICON}</button><span class="plugin-warning-tooltip" role="tooltip" hidden><span id="pluginWarningText"></span><span class="plugin-warning-action">Open Setup</span></span></div></div>
  </div>
  <div id="controlToolbar" hidden><input type="search" id="controlSearch" class="control-search" placeholder="Search settings by name or effect..." aria-label="Search settings in this section"><div class="control-filters" role="group" aria-label="Filter settings"><button data-control-filter="all" aria-pressed="true">All settings</button><button data-control-filter="modified" aria-pressed="false">Modified</button></div></div>
  <div id="workspace" role="tabpanel" aria-labelledby="nav-modifiers">
<div id="modsSubtabs" class="subtabbar" role="tablist" aria-label="Mods categories" hidden><button type="button" class="subtabbtn" role="tab" id="subtab-qol" aria-controls="qolCard" aria-selected="true" tabindex="0">Quality of Life</button><button type="button" class="subtabbtn" role="tab" id="subtab-items" aria-controls="itemsCard" aria-selected="false" tabindex="-1">Items</button><button type="button" class="subtabbtn" role="tab" id="subtab-gameplay" aria-controls="gameplayCard" aria-selected="false" tabindex="-1">Gameplay</button></div>
<Overview />
<Setup />
<Loot />
<Modifiers />
<Mods />
<World />
</div>
</main></div>
<div id="toast" role="status"></div>
