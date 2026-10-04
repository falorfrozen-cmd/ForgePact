<!-- World tab. #spawners and the Satanic lists are filled from /api/state by boot() in src/panel.js; preparePanelUI() puts the cards in their visual order. -->
<div class="card tab-card" data-tab="world" id="densityCard">
  <h2>Monster Density</h2>
  <div class="hint">Multiplies enemy spawners - applies to newly loaded zones.<br><b>Density and Special Content stack.</b> Each on its own is fine, but a high density together with high special-content rates can overload a heavy zone and crash the game on entry. Verified stable: density x3 with every special content at x20. If a zone crashes, lower density first.</div>
  <div class="note density-once">Density is applied once per creator placement. Returning to a previously visited zone does not multiply it again.</div>
  <div class="row">
    <span class="lbl">Density multiplier</span>
    <label class="switch"><input type="checkbox" id="den_on"><span class="sl"></span></label>
    <input type="range" id="den" min="1" max="5" step="0.5">
    <span class="val" id="denval">x3</span>
  </div>
</div>

<div class="card tab-card" data-tab="world" id="speedCard">
  <h2>Enemy Movement Speed</h2>
  <div class="hint">Enemies run at you faster, so waves end sooner. Scales the game's own path speed (base speed &times; bonus); slows and debuffs still apply on top, goblins keep their own pace. <b>Only inside Chaos Tower</b> leaves every other zone vanilla - switch it off to speed up enemies everywhere.</div>
  <div class="row">
    <span class="lbl">Speed bonus</span>
    <label class="switch slider-switch"><input type="checkbox" id="sw_enemy_speed" data-switch="enemy_speed" aria-label="Enable Speed bonus"><span class="sl"></span></label>
    <input type="range" id="enemyspeed" min="0" max="300" step="5">
    <span class="val" id="enemyspeedval">off</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl">Only inside Chaos Tower</span>
    <label class="switch"><input type="checkbox" id="enemyspeed_ct"><span class="sl"></span></label>
    <span class="val" id="enemyspeedctval">CT only</span>
  </div>
</div>

<div class="card tab-card" data-tab="world" id="spawnsCard">
  <h2>Special Content Spawns</h2>
  <div class="hint">Multiplies the game's own spawn markers, so the game places and runs each mechanic itself - nothing is hand-placed. Higher = more of that content per zone. Applies to newly loaded zones. (The Abyss is not listed: it sits behind a discovery gate that is not solved yet.)</div>
  <div id="spawners"></div>
</div>

<div class="card tab-card" data-tab="world" id="rarityCard">
  <h2>Monster Rarity</h2>
  <div class="hint">Raises a share of the normal monsters to <b>Rare</b> (yellow) or <b>Ancient</b> (skull) as they spawn, through the game's own rarity setup: the monster gets that tier's stats, affixes and health bar exactly as if it had rolled that way. The two shares are separate and together stay at 100% or less - 25% Rare with 15% Ancient leaves 60% normal. Champions, the game's own rares, and bosses are left alone - bosses already have their own scripted health and affixes. Stacks with Tyrant's Crown and Density.</div>
  <div class="row" style="border:none">
    <span class="lbl">Rare</span>
    <label class="switch slider-switch"><input type="checkbox" id="sw_rarity_rare" data-switch="rarity_rare" aria-label="Enable Rare"><span class="sl"></span></label>
    <input type="range" min="0" max="100" step="5" id="rarity_rare" value="0">
    <span class="val off" id="rarityrareval" style="width:64px">off</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl">Ancient</span>
    <label class="switch slider-switch"><input type="checkbox" id="sw_rarity_ancient" data-switch="rarity_ancient" aria-label="Enable Ancient"><span class="sl"></span></label>
    <input type="range" min="0" max="100" step="5" id="rarity_ancient" value="0">
    <span class="val off" id="rarityancval" style="width:64px">off</span>
  </div>
  <div class="note" id="raritynote">off</div>
</div>

<section class="card tab-card" data-tab="world" id="satanicMods" aria-labelledby="satTitle" aria-busy="false">
  <div class="sat-heading">
    <div>
      <h2 id="satTitle">Satanic Zone Mods</h2>
      <p class="sat-intro">Choose which modifiers can roll in your zones.
        <span>Enabled mods are eligible, not guaranteed. The game still rolls your zone.</span>
      </p>
    </div>
    <button type="button" class="sat-button" id="satRestore" title="Enable every positive and negative zone modifier">&#8634; Restore defaults</button>
  </div>
  <div class="sat-zonecontrol">
    <div class="row" style="border:none">
      <span class="lbl" style="width:auto;flex:1">Keep the zone you are in satanic<br><span class="feature-description">Wherever you go, the game treats the zone you are in as the Satanic Zone. Towns and sub-areas are left alone. Off by default.</span></span>
      <label class="switch"><input type="checkbox" id="satanic_follow"><span class="sl"></span></label>
      <span class="val" id="szfval">off</span>
    </div>
    <div class="row" style="border:none">
      <span class="lbl" style="width:auto;flex:1">Every zone counts as satanic<br><span class="feature-description">The game's own "is this a Satanic Zone?" answer becomes yes wherever you are, so satanic modifiers and relic chances apply everywhere. Off by default; not yet confirmed in a live game.</span></span>
      <label class="switch"><input type="checkbox" id="satanic_everywhere"><span class="sl"></span></label>
      <span class="val" id="szeval">off</span>
    </div>
  </div>
  <div class="sat-toolbar">
    <label class="sat-search">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></svg>
      <input type="search" id="satSearch" placeholder="Search by name or effect..." aria-label="Search zone modifiers" autocomplete="off">
    </label>
    <div class="sat-filters" role="group" aria-label="Filter zone modifiers">
      <button type="button" data-sat-filter="all" aria-pressed="true">All mods</button>
      <button type="button" data-sat-filter="enabled" aria-pressed="false">Enabled</button>
      <button type="button" data-sat-filter="disabled" aria-pressed="false">Disabled</button>
    </div>
  </div>
  <div class="sat-columns">
    <section class="sat-panel" data-polarity="buff" aria-labelledby="satbuffTitle">
      <div class="sat-panel-head">
        <div class="sat-panel-title"><span class="sat-sign" aria-hidden="true">+</span><h3 id="satbuffTitle">Positive modifiers</h3><span class="sat-count" id="satbuffCount"></span></div>
        <div class="sat-panel-tools"><span>Keep at least <strong id="satbuffMin">3</strong> enabled</span><button type="button" class="sat-button" id="satbuffAll" aria-label="Enable all positive modifiers">Enable all</button></div>
      </div>
      <div class="sat-list" id="satbuffs" role="group" aria-labelledby="satbuffTitle"></div>
      <div class="sat-panel-foot" id="satbuffHelp"></div>
    </section>
    <section class="sat-panel" data-polarity="debuff" aria-labelledby="satdebuffTitle">
      <div class="sat-panel-head">
        <div class="sat-panel-title"><span class="sat-sign" aria-hidden="true">&minus;</span><h3 id="satdebuffTitle">Negative modifiers</h3><span class="sat-count" id="satdebuffCount"></span></div>
        <div class="sat-panel-tools"><span>Keep at least <strong id="satdebuffMin">2</strong> enabled</span><button type="button" class="sat-button" id="satdebuffAll" aria-label="Enable all negative modifiers">Enable all</button></div>
      </div>
      <div class="sat-list" id="satdebuffs" role="group" aria-labelledby="satdebuffTitle"></div>
      <div class="sat-panel-foot" id="satdebuffHelp"></div>
    </section>
  </div>
  <div class="sat-footer">
    <div id="satSummary" role="status" aria-live="polite"></div>
    <span id="satSaveState" role="status" aria-live="polite">Changes save automatically</span>
  </div>
</section>
