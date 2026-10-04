<!-- Modifiers tab. Each group is filled from /api/state by boot() in src/panel.js. -->
<div class="card modifier-card tab-card" data-tab="modifiers">
  <div class="section-title">
    <div><h2>Combat &amp; Character Modifiers</h2>
      <div class="hint">Live modifiers use the character's current total value, including equipment and other bonuses. <b>Off</b> keeps the game at its normal value.</div>
    </div>
    <span class="live-badge">LIVE MODIFIERS</span>
  </div>
  <div class="modifier-grid">
    <div class="modifier-group">
      <div class="group-title sustain">Utility</div>
      <div id="stats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title offense">Offense</div>
      <div id="offensivestats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title sustain">Defense &amp; Sustain</div>
      <div id="sustainstats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title critical">Critical Strikes</div>
      <div id="criticalstats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title offense">Skills</div>
      <div id="skillstats"></div>
    </div>
  </div>
</div>
