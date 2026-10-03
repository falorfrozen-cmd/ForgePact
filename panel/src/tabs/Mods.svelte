<script>
  import { HIDDEN_LOOT_KEYS, HIDDEN_LOOT_KEY_DEFAULT } from '../hidden-loot-keys.js';
</script>

<!-- Mods tab: the Quality of Life, Items and Gameplay panels, switched by App.svelte's sub-tab strip. None repeats its
     sub-tab's name as a heading; each mod is drawn as a card of its own (app.css), and a panel is not drawn as one.
     A child row is marked by its indent (app.css .feature-with-child), never by a glyph (finish review F6). -->
<div class="card tab-card" data-tab="mods" id="qolCard" role="tabpanel" aria-labelledby="subtab-qol">
  <div class="hint">Toggle drop pool adjustments and quality-of-life tweaks for your offline session. Settings apply immediately while the game is running.</div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Remove owned relics from drop pool<br><span class="feature-description">When a relic is dropped, prevents relics already at maximum level (10 out of 10) in your equipped slots, backpack, or inventory from dropping.</span></span>
    <label class="switch"><input type="checkbox" id="mod_filter_max_relics"><span class="sl"></span></label>
    <span class="val" id="mfmrval">on</span>
  </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Experience and Magic Find orb pickup radius<br><span class="feature-description">Makes the player collect matching orbs from 10 times the normal distance.</span></span>
        <label class="switch"><input type="checkbox" id="mod_orb_pickup_radius"><span class="sl"></span></label>
        <span class="val" id="morval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Reveal full map<br><span class="feature-description">Reveals the full minimap in every zone (removes fog of war). Waypoints, dungeon entrances, chests, shrines and mining nodes come with it - they are hidden by the fog, not by anything else.</span></span>
        <label class="switch"><input type="checkbox" id="map_reveal"><span class="sl"></span></label>
        <span class="val" id="mapval">on</span>
    </div>
    <div class="row" id="map_reveal_packs_row">
        <span class="lbl" style="width:auto;flex:1">Show every monster pack on the map<br><span class="feature-description">Most mob packs do not exist until you walk near them, so the revealed map used to show only the packs you had already met. This marks every pack's spot and kind (normal, champion, ancient, legion, mini boss) on the minimap the moment you arrive, including density copies, without creating a single monster: the pack is still born by the game when you walk near it, and its real dots replace the marker. Costs nothing per frame beyond the markers themselves.</span><span id="packMarkerStatus" class="hint" role="status" hidden></span></span>
        <label class="switch"><input type="checkbox" id="map_reveal_packs"><span class="sl"></span></label>
        <span class="val" id="mrpval">on</span>
    </div>
    <div class="row" id="map_reveal_spawn_row">
        <span class="lbl" style="width:auto;flex:1">Really spawn every pack on arrival (heavy)<br><span class="feature-description">The old way: each new zone creates all of its packs, including density copies, as you arrive. Every living monster costs the game frame time on top of the markers, so at high density this lags for the whole zone. Off by default; only for comparing against the markers.</span><span id="populationStatus" class="hint" role="status" hidden></span></span>
        <label class="switch"><input type="checkbox" id="map_reveal_spawn"><span class="sl"></span></label>
        <span class="val" id="mrsval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Pet collects quest items<br><span class="feature-description">While your pet is out, it walks to quest items on screen and picks them up for you - one at a time, crediting the quest objective exactly as collecting it by hand does. Only applies to pick-up quest items; things you activate, break or talk to are left alone.</span></span>
        <label class="switch"><input type="checkbox" id="mod_pet_quest_pickup"><span class="sl"></span></label>
        <span class="val" id="mpqpval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Pet collects relics<br><span class="feature-description">While your pet is out, it walks to relics lying on screen and picks them up for you, one at a time, raising the relic you own by one level the way picking it up yourself does. A relic you already have at 10/10 is left where it is, since it cannot be picked up. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_pet_relic_pickup"><span class="sl"></span></label>
        <span class="val" id="mprpval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Pet moves on from loot it cannot pick up<br><span class="feature-description">When a lot of loot is on the ground and your pet stays stuck on one item it cannot pick up, it leaves that item for a few seconds and goes for the next one. A target that is not an item or a coin at all is left at once. Nothing is picked up or destroyed for you. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_pet_loot_unstick"><span class="sl"></span></label>
        <span class="val" id="mpluval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Auto-prospect items put in the Prospect Cube<br><span class="feature-description">Every item you drag or click into the Prospect Cube's grid is prospected straight away, as if you had pressed Prospect, so the grid never fills with items waiting their turn. Anything still in the prospect grid when the game saves is lost.</span></span>
        <label class="switch"><input type="checkbox" id="mod_auto_prospect"><span class="sl"></span></label>
        <span class="val" id="autoprospval">off</span>
    </div>
    <div class="row" id="mod_auto_prospect_bag_row">
        <span class="lbl" style="width:auto;flex:1">Move the previous materials to your materials tab<br><span class="feature-description">When the next item is prospected, the materials from the prospect before it go from the grid to your materials tab first, the way clicking them does. The newest batch stays in the grid where you can see it. A material the game will not take stays in the grid.</span></span>
        <label class="switch"><input type="checkbox" id="mod_auto_prospect_bag"><span class="sl"></span></label>
        <span class="val" id="apbagval">on</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Craft from the stash<br><span class="feature-description">Crafting Cube recipes also count the materials and socketables in your stash's Materials and Socketable tabs. When you craft, only what your bag is short of leaves the stash, and the stash is saved right after. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_craft_mats"><span class="sl"></span></label>
        <span class="val" id="mcmval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Move all into the stash<br><span class="feature-description">With the stash open, click the Move all button beside your bag's Sort, or press F4, to move every item on the bag tab you see into the stash tab you see. Items the tab has no room for or does not take stay in your bag. If a move cannot be confirmed, it turns off until you restart. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_stash_move_all"><span class="sl"></span></label>
        <span class="val" id="msmaval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Mark a running toggle skill<br><span class="feature-description">For a fixed set of toggle skills, each measured in-game: draws a soft red outline around that skill's skill-bar slot while its toggle is running, so you can see at a glance that it is still active. The outline disappears when the toggle ends. A plain cast, made without the skill's toggle sub-talent, lights nothing.</span></span>
        <label class="switch"><input type="checkbox" id="mod_toggle_indicator"><span class="sl"></span></label>
        <span class="val" id="mtival">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Stop double cast re-casting a toggle skill<br><span class="feature-description">For that same fixed set of toggle skills: a double cast proc can cast one of them a second time on its own, which flips its toggle straight back to where it was before your press. With this on, that extra cast is skipped, so the toggle stays the way you set it. It only steps in when you actually have the skill's toggle sub-talent, or the skill is a toggle on its own; your own presses are never affected.</span></span>
        <label class="switch"><input type="checkbox" id="mod_toggle_guard"><span class="sl"></span></label>
        <span class="val" id="mtgval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Restart zone at any time<br><span class="feature-description">The pause menu's Restart normally waits until you have been out of combat for a few seconds; with this on it works straight away. Use the mouse: in combat Restart still looks greyed until the cursor is on it, then lights up and works when clicked.</span></span>
        <label class="switch"><input type="checkbox" id="mod_restart_anytime"><span class="sl"></span></label>
        <span class="val" id="mraval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Far scenery sleep<br><span class="feature-description">Lets the game skip a zone's far trees, bushes, hay, rocks and fences every frame, and wakes them before they come into view, so busy zones run lighter. Shrines, chests, traps and monsters are never touched. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_far_sleep"><span class="sl"></span></label>
        <span class="val" id="mfsval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Extra packs as you approach<br><span class="feature-description">With Monster Density above x1, the extra monster packs are set up as you come near instead of all at once when a zone loads, so crowded zones run lighter. You meet just as many packs. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="density_rolling"><span class="sl"></span></label>
        <span class="val" id="drlval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Sleep loot your filter hides<br><span class="feature-description">Items your loot filter hides are put to sleep as they drop, so the game stops spending time on them every frame. Hold Left Alt, or the key you pick below, to see them and pick them up; let go and they hide again. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_hidden_loot"><span class="sl"></span></label>
        <span class="val" id="mhlval">off</span>
    </div>
    <div class="row" id="mod_hidden_loot_key_row">
        <span class="lbl" style="width:auto;flex:1">Show hidden loot while held<br><span class="feature-description">While you hold this key or mouse button with the game in front, the items your filter hides are shown. None turns the key off.</span></span>
        <select class="style-select" id="mod_hidden_loot_key" aria-label="Show hidden loot while held">
            {#each HIDDEN_LOOT_KEYS as [code, name] (code)}<option value={code} selected={code === HIDDEN_LOOT_KEY_DEFAULT}>{name}</option>{/each}
        </select>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Timed skill countdown<br><span class="feature-description">Shows how much time a timed skill has left, over that skill's slot on the skill bar, in the look you pick below. Works for most timed skills; toggles and companions (turrets, totems) don't get one. Off by default.</span></span>
        <select class="style-select" id="mod_skill_timer_style" aria-label="Timed skill countdown">
            <option value="off">Off</option>
            <option value="arc">Arc</option>
            <option value="bar">Bar</option>
            <option value="number">Number</option>
            <option value="fade">Fade</option>
        </select>
    </div>
</div>

<div class="card tab-card" data-tab="mods" id="itemsCard" role="tabpanel" aria-labelledby="subtab-items">
  <div class="hint">Custom forge mechanics tied to items made in the Item Editor. Settings apply immediately while the game is running.</div>
  <div class="row" id="minerHelmetCard">
    <div>
      <strong>Miner's Helmet</strong>
      <p class="helmet-stats">+1000 Defense &middot; +500% Enhanced Defense<br>+20% Movement Speed &middot; +20% All Resistances &middot; +5 Light Radius</p>
      <p class="hint">While worn, every mining node gives exactly 4&times; its ore. This replaces the Mining Ore Multiplier slider instead of stacking with it; with the helmet off, the slider applies as usual.<br><strong>Vein Resonance:</strong><br>finishing a dig also digs the two nearest veins within 192 units that you could mine yourself, each at 4&times;, through the game's own dig. A vein dug this way never starts another.</p>
      <p class="hint">Forge it in the Item Editor: Item Forge &rarr; Forge a signature item &rarr; Miner's Helmet.</p>
      <div id="minerHelmetStatus" role="status" aria-live="polite">Start the game to check the helmet.</div>
    </div>
  </div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Headhunter buffs on rare kills<br><span class="feature-description">For an item forged with Mechanic: Headhunter. While on, killing a rare or champion monster grants its affixes to you as 20-second buffs (Extra Fast &rarr; movement speed, Berserker/Raging/Enraged &rarr; attack speed, Vampiric &rarr; life replenish, elemental Enchanted &rarr; cast rate, others &rarr; movement speed for now). The equipped-belt check is still in progress, so the effect is active whenever this switch is on and the forged item exists. While this switch is on, Headhunter can also drop from the game's own Angelic roll, the way the game's Angelic uniques do: one item, in place of what that roll would have dropped. Off by default; forging the item alone never turns that drop on.</span></span>
    <label class="switch"><input type="checkbox" id="headhunter"><span class="sl"></span></label>
    <span class="val" id="hhval">on</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Tyrant's Crown: more rares, richer rares<br><span class="feature-description">For an item forged with Mechanic: Tyrant's Crown. While on, normal monsters near you rise to rare more often (15% each) and every rare or champion carries one extra affix. Pairs with Headhunter: more rares, more affixes to steal. While this switch is on, Tyrant's Crown can also drop from the game's own Angelic roll, the way the game's Angelic uniques do: one item, in place of what that roll would have dropped. Off by default; forging the item alone never turns that drop on.</span></span>
    <label class="switch"><input type="checkbox" id="tyrant"><span class="sl"></span></label>
    <span class="val" id="tyval">on</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Beacon: every monster hunts you<br><span class="feature-description">For an amulet forged with Mechanic: Beacon. While on, every monster on the map hunts you the moment it spawns and never turns back, through the game's own aggro system. Plugin commands: beaconmode rare limits it to rares and champions, beaconrange &lt;px&gt; caps the distance.</span></span>
    <label class="switch"><input type="checkbox" id="beacon"><span class="sl"></span></label>
    <span class="val" id="beval">on</span>
  </div>
</div>

<div class="card tab-card" data-tab="mods" id="gameplayCard" role="tabpanel" aria-labelledby="subtab-gameplay">
  <div class="hint">Change how the monsters you meet are made, and when a key dungeon's chest opens. Settings apply immediately while the game is running.</div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Bosses<br><span class="feature-description">While this is on, a boss the game spawns is given Rare ("uber") or Ancient ("uber uber") rarity as it is set up. Tested in a live game at Ancient, a boss came out far stronger, with about five times its health. Bosses the game already made champion, rare or ancient, and bosses another monster creates (phases, clones), are left alone. Off by default.</span></span>
    <select class="style-select" id="boss_rarity" aria-label="Bosses">
      <option value="off">Normal (the game's own)</option>
      <option value="rare">Rare &mdash; "uber" boss</option>
      <option value="ancient">Ancient &mdash; "uber uber" boss</option>
    </select>
  </div>
  <!-- Dungeon chest opens early (issue #31): the percentage is a switch and a slider, never a select (the owner,
       2026-10-03). The value beside the slider is typable (panel.js typable()); "off" while the switch is off, as Monster
       Density's is. Its child row picks where the countdown shows, like the skill timer's look (the owner, 2026-10-04),
       and is disabled while the switch is off, as Sleep loot's show key is. -->
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Dungeon chest opens early<br><span class="feature-description">The chest at the end of a key dungeon opens once the share set here of all its monsters is dead, counting every monster the dungeon plans when it loads, spawned yet or not; that total is an estimate from its spawners. A countdown shows the last 50 kills. Off by default.</span></span>
    <label class="switch"><input type="checkbox" id="mod_dungeon_chest" aria-label="Dungeon chest opens early"><span class="sl"></span></label>
    <input type="range" id="dungeon_chest_pct" min="50" max="95" step="5" value="75" aria-label="Share of the dungeon's monsters to kill">
    <span class="val off" id="dcpval" style="width:64px">off</span>
  </div>
  <div class="row" id="dungeon_chest_countdown_row">
    <span class="lbl" style="width:auto;flex:1">Where the countdown shows<br><span class="feature-description">The last 50 kills before the chest opens are counted down above your character, as chat lines, or both.</span></span>
    <select class="style-select" id="dungeon_chest_countdown" aria-label="Where the countdown shows">
      <option value="head">Above your character</option>
      <option value="chat">In chat</option>
      <option value="both">Both</option>
    </select>
  </div>
</div>
