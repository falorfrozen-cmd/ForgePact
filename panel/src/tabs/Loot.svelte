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
    <input type="range" id="angelic_items" min="1" max="100" step="1" value="1">
    <span class="val off" id="angelicval" style="width:64px">off</span>
  </div>
  <div class="note" id="angelicnote">off</div>
</div>
