<section
  class="tab-card ember-overview"
  data-tab="overview"
  id="overviewCard"
  aria-label="Overview"
>
  <div class="ember-hero">
    <div class="ember-hero-copy">
      <h1>Your forge.<br />Your rules.</h1>
      <p>Shape your offline adventure.</p>
      <button type="button" class="btn ember-outline" data-ember-search>
        Explore all settings <span aria-hidden="true">→</span>
      </button>
    </div>
  </div>
  <div class="ember-overview-grid">
    <section class="ember-panel ember-quick">
      <div class="ember-section-head">
        <h2>
          <span class="ember-heading-icon" aria-hidden="true">⚒</span> Quick
          controls
        </h2>
        <button
          type="button"
          class="ember-star"
          id="customizeQuick"
          aria-label="Customize quick controls"
          title="Customize quick controls"
        >
          ★
        </button>
      </div>
      <div id="overviewQuick" aria-busy="true">
        <p class="hint">Loading your settings…</p>
      </div>
    </section>
    <section class="ember-panel ember-qol">
      <div class="ember-section-head">
        <h2>
          <span class="ember-heading-icon" aria-hidden="true">⚙</span> Quality
          of life
        </h2>
      </div>
      <div class="ember-toggle-row">
        <span class="ember-item-icon ember-map" aria-hidden="true"></span>
        <div class="ember-control-label">
          <label for="quick-map">Reveal full map</label>
        </div>
        <label class="switch"
          ><input
            type="checkbox"
            id="quick-map"
            data-ember-toggle="map_reveal"
            aria-label="Reveal full map (quick control)" /><span
            class="sl"
          ></span></label
        ><span class="ember-toggle-state" data-state-for="quick-map">Off</span>
      </div>
      <div class="ember-toggle-row">
        <span class="ember-item-icon ember-banner" aria-hidden="true"></span>
        <div class="ember-control-label">
          <label for="quick-packs">Show pack locations</label
          ><small id="quick-packs-hint"
            >Marks packs without spawning them.</small
          >
        </div>
        <label class="switch"
          ><input
            type="checkbox"
            id="quick-packs"
            data-ember-toggle="map_reveal_packs"
            aria-label="Show pack locations (quick control)"
            aria-describedby="quick-packs-hint" /><span
            class="sl"
          ></span></label
        ><span class="ember-toggle-state" data-state-for="quick-packs"
          >Off</span
        >
      </div>
      <div class="ember-toggle-row">
        <span class="ember-item-icon ember-paw" aria-hidden="true"></span>
        <div class="ember-control-label">
          <label for="quick-pet">Pet collects quest items</label>
        </div>
        <label class="switch"
          ><input
            type="checkbox"
            id="quick-pet"
            data-ember-toggle="mod_pet_quest_pickup"
            aria-label="Pet collects quest items (quick control)" /><span
            class="sl"
          ></span></label
        ><span class="ember-toggle-state" data-state-for="quick-pet">Off</span>
      </div>
    </section>
  </div>
  <section class="ember-panel ember-pool">
    <span class="ember-rune" aria-hidden="true">⛧</span>
    <div class="ember-pool-title">
      <h2>Satanic Zone Mods</h2>
      <p>Choose which modifiers can roll.</p>
    </div>
    <div class="ember-count positive">
      <strong id="quick-buffs">Loading…</strong
      ><small id="quick-buffs-min">Minimum 3</small>
    </div>
    <div class="ember-count negative">
      <strong id="quick-debuffs">Loading…</strong
      ><small id="quick-debuffs-min">Minimum 2</small>
    </div>
    <button type="button" class="btn ember-outline" id="overviewEditPool">
      Edit pool <span aria-hidden="true">→</span>
    </button>
  </section>
</section>
<section class="card tab-card ember-help" data-tab="help" id="helpCard">
  <h2>Your offline workshop</h2>
  <p class="hint">A quick guide to ForgePact.</p>
  <div class="ember-help-grid">
    <article>
      <h3>1. Connect your game</h3>
      <p>
        Choose your Hero_Siege.exe in Setup. Install the mod plugin while the
        game is closed, then launch your offline game.
      </p>
      <button class="btn" type="button" data-open-tab="setup">
        Open Setup →
      </button>
    </article>
    <article>
      <h3>2. Shape your adventure</h3>
      <p>
        Character changes your bonuses. World controls density, rarity and zone
        content. Loot adjusts drop rates. Mods contains quality-of-life features
        and item mechanics.
      </p>
      <button class="btn" type="button" data-ember-search>
        Find a setting →
      </button>
    </article>
    <article>
      <h3>3. Know when it applies</h3>
      <p>
        Settings save automatically. Some changes need a new zone. Auto-apply on
        launch sends your saved choices when the game starts. Apply all now
        sends them immediately, or queues them if the game is closed.
      </p>
    </article>
    <article>
      <h3>Your quick controls</h3>
      <p>
        Use the star on Overview to choose up to three shortcuts. They edit the
        same settings as the full pages. Click a number to type an exact value;
        sliders and − / + buttons work too.
      </p>
      <button class="btn" type="button" data-open-tab="overview">
        Open Overview →
      </button>
    </article>
  </div>
</section>
<dialog
  id="emberSearchDialog"
  class="ember-dialog"
  aria-labelledby="emberSearchTitle"
>
  <div class="ember-dialog-head">
    <h2 id="emberSearchTitle">Find a setting</h2>
    <button
      class="ember-close"
      type="button"
      data-close-dialog
      aria-label="Close search"
    >
      ×
    </button>
  </div>
  <label class="sr-only" for="emberSearchInput"
    >Search all settings by name or effect</label
  ><input
    id="emberSearchInput"
    type="search"
    placeholder="Density, gold, map reveal…"
    autocomplete="off"
  />
  <p id="emberSearchCount" role="status"></p>
  <div id="emberSearchResults"></div>
  <p class="ember-dialog-hint">
    Use Tab to choose a result. Enter opens it. Esc closes search.
  </p>
</dialog>
<dialog
  id="emberQuickDialog"
  class="ember-dialog"
  aria-labelledby="emberQuickTitle"
>
  <div class="ember-dialog-head">
    <h2 id="emberQuickTitle">Your quick controls</h2>
    <button
      class="ember-close"
      type="button"
      data-close-dialog
      aria-label="Close quick controls"
    >
      ×
    </button>
  </div>
  <p>Choose up to three settings for Overview.</p>
  <div id="emberQuickChoices"></div>
  <p id="emberQuickMessage" role="status"></p>
  <button class="btn primary" type="button" id="saveQuickChoices">
    Save shortcuts
  </button>
</dialog>
