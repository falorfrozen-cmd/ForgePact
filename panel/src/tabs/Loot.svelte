<!-- Loot tab. #drops and #keys are filled from /api/state by boot() in src/panel.js. -->
<div class="card tab-card" data-tab="loot" id="dropsCard">
  <h2>Drop Rates</h2>
  <div class="hint">All of these use the game's own dice - <b>nothing is forced</b>.
  <b>x5 means five times more likely than vanilla</b>; <b>off</b> (x1) leaves that drop completely untouched.
  Applies immediately, no zone reload needed.<br>
  Every multiplier scales the item's own vanilla roll, so x2 is twice the vanilla rate.
  Families the game never rolls outside their home zone (Dungeon and Angelic keys, Orbs,
  Scrolls, Shards, Fragments) first get their roll opened at the monster's normal-key
  chance; Relics use their own curve, explained on the row.</div>
  <div id="dropSettings">
    <div id="drops"></div>
    <div id="keys"></div>
  </div>
</div>

<div class="card tab-card" data-tab="loot" id="angelicCard">
  <h2>Angelic / Unholy Drops (Experimental)</h2>
  <div class="hint">The game only rolls for Angelic or Unholy items while an "Angelic item drop chance"
  effect (a Blood Pact or dungeon modifier) is active, so this is ForgePact's own die: on every monster
  kill it rolls, and on a hit the game itself builds one of its 49 real Angelic / Unholy uniques (no
  developer or event pieces) and drops it where the monster died. Headhunter and Tyrant's Crown share
  this same die and pool: they never drop while this is off (x1), and drop exactly as often as any
  other item in it.<br>
  <b>x2</b> is one die per kill at the Angelic Key's own rate (1 in 7,500), every step above adds a die.
  Click the value to type an exact number. <b>x1</b> is off.</div>
  <div class="row">
    <span class="lbl">Angelic / Unholy items</span>
    <label class="switch slider-switch"><input type="checkbox" id="sw_angelic_items" data-switch="angelic_items" aria-label="Enable Angelic / Unholy items"><span class="sl"></span></label>
    <input type="range" id="angelic_items" min="1" max="100" step="1" value="1">
    <span class="val off" id="angelicval" style="width:64px">off</span>
  </div>
  <div class="note" id="angelicnote">off</div>
</div>

<!-- Gems of Incarnation: both switches, then the mod filter row and its list, written in the Mods rows' final
     feature-row shape (preparePanelUI converts only the Mods cards). #gemfilter_panel is drawn by panel.js
     (renderGemFilter) with the Satanic pool's classes each time Filter… opens it. -->
<div class="card tab-card" data-tab="loot" id="gemsCard">
  <h2>Gems of Incarnation</h2>
  <div class="row feature-row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Mythic Gems of Incarnation</span>
    <label class="switch"><input type="checkbox" id="mod_gem_mythic"><span class="sl"></span></label>
    <span class="val" id="mgmval">off</span>
    <span class="feature-description">Every Gem of Incarnation that drops is Mythic, with 4 or 5 mods. The game rolls it itself, from a seed it has already rolled Mythic. Gems you own keep their own mods. The first time, the game rolls sample gems in the background, under a minute at the main menu. Off by default.</span>
  </div>
  <div class="row feature-row" id="mod_gem_maxroll_row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Max-roll Gems of Incarnation</span>
    <label class="switch"><input type="checkbox" id="mod_gem_maxroll"><span class="sl"></span></label>
    <span class="val" id="mgrval">off</span>
    <span class="feature-description">Every mod on every Gem of Incarnation shows the highest value its best tier can roll. Nothing is written to your save: turn it off, and a gem shows its own rolls again the next time the game loads it. Off by default.</span>
  </div>
  <div class="row feature-row" id="gemfilter_row">
    <span class="lbl" style="width:auto;flex:1">Mods on Mythic gems</span>
    <button class="btn" id="gemfilter_toggle" type="button" aria-expanded="false" aria-controls="gemfilter_panel">Filter&hellip;</button>
    <span class="val" id="gemfilter_summary">all</span>
    <span id="gemfilter_unsaved" class="hint" role="status" hidden>Unsaved changes</span>
    <span class="feature-description">Tick the mods you want and save. Every Mythic gem that drops then carries as many of them as the game's own Mythic rolls allow; with everything ticked, any mix. Works with Mythic Gems of Incarnation on.</span>
  </div>
  <div id="gemfilter_panel" style="display:none"></div>
</div>
