// Everything the old page's inline script did once its markup was on screen:
// read /api/state, draw the rows that come from it, wire every control, keep
// the status chips current and poll. The bodies are the old script's; what
// changed is only how they reach each other (imports), and that the names
// other modules own - ST (state.svelte.js) and the navigation state (nav.js)
// - are set through setST(), setControlFilter() and setModsSubtab() instead
// of by assignment (api.js's write counter is only read here).
// start() is what main.js calls once App.svelte is mounted.
import { ST, setST } from './state.svelte.js';
import { j, pendingWrites } from './api.js';
import { PANEL_ICON_MAP } from './icons.js';
import { activeTab, controlFilter, modsSubtab, openTab, bindModsSubtabs, setControlFilter, setModsSubtab } from './nav.js';
import { syncRevealPacks, syncProspectBag } from './mods-sync.js';
import { setupModsColumns } from './mods-columns.js';
import { pollDelayMs, pollNextChangeAt } from './poll-policy.js';
import { switchControlId, switchOn } from './enabled-mods.js';
import { renderEnabledMods } from './lib/enabled-mods-list.js';
import { applyTheme } from './theme.js';

let tmr=null;
function iconMarkup(name){
  return name?`<svg class="setting-icon" data-icon="${name}" viewBox="0 0 32 32" aria-hidden="true" focusable="false"><use href="#fp-icon-${name}"></use></svg>`:'';
}
function decorateIconLabel(label,name){
  if(!label||!name||label.querySelector('.setting-icon'))return;
  const copy=document.createElement('span');copy.className='label-copy';
  while(label.firstChild)copy.append(label.firstChild);
  label.classList.add('has-setting-icon');label.innerHTML=iconMarkup(name);label.append(copy);
}
function decoratePanelIcons(){
  document.querySelectorAll('input[data-sec][data-key]').forEach(input=>
    decorateIconLabel(input.closest('.row')?.querySelector('.lbl'),PANEL_ICON_MAP.controls[input.dataset.sec]?.[input.dataset.key]));
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.static))
    decorateIconLabel(document.getElementById(id)?.closest('.row')?.querySelector('.lbl'),name);
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.sections))
    decorateIconLabel(document.getElementById(id)?.querySelector('h2'),name);
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.actions)){
    const button=document.getElementById(id);
    if(!button.querySelector('.setting-icon')){
      button.textContent=button.textContent.replace(/^[\u{1F4C1}\u25B6\u21BA]\s*/u,'');
      decorateIconLabel(button,name);
    }
  }
  document.querySelectorAll('.group-title').forEach((label,i)=>decorateIconLabel(label,['experience','damage','defense','critical-chance'][i]));
  decorateIconLabel(document.querySelector('.modifier-card h2'),'damage');
}
// The toast rises in and sinks out (app.css), except after a keyboard action:
// a keyboard action never animates, so the last input decides (data-instant).
let lastInputKeyboard=false;
addEventListener('keydown',()=>{lastInputKeyboard=true},true);
addEventListener('pointerdown',()=>{lastInputKeyboard=false},true);
export function toast(m){const t=document.getElementById('toast');t.toggleAttribute('data-instant',lastInputKeyboard);t.textContent=m;t.classList.add('show');clearTimeout(tmr);tmr=setTimeout(()=>t.classList.remove('show'),2200)}
function angelicPaint(){
  const el=document.getElementById('angelic_items'); const v=sliderVal(el);
  const dice=Math.max(0,Math.round(v)-1); const oneIn=dice>0?Math.max(1,Math.round(7500/dice)):0;
  const on=v>1&&!switchedOff('angelic_items');
  const val=document.getElementById('angelicval'); val.textContent=on?'x'+v:'off'; val.className='val '+(on?'':'off');
  document.getElementById('angelicnote').textContent=oneIn>0?`about 1 Angelic or Unholy item in ${oneIn.toLocaleString()} kills (${dice} ${dice>1?'dice':'die'} per kill at 1 in 7,500)`:'off - the game rolls only with an Angelic drop-chance effect';
}
function rarityPaint(){
  const r=sliderVal(document.getElementById('rarity_rare')), a=sliderVal(document.getElementById('rarity_ancient'));
  const rv=document.getElementById('rarityrareval'), av=document.getElementById('rarityancval');
  const ron=r>0&&!switchedOff('rarity_rare'), aon=a>0&&!switchedOff('rarity_ancient');
  rv.textContent=ron?r+'%':'off'; rv.className='val '+(ron?'':'off');
  av.textContent=aon?a+'%':'off'; av.className='val '+(aon?'':'off');
  document.getElementById('raritynote').textContent=(r>0||a>0)?`of the normal monsters: ${a}% Ancient, ${r}% Rare, ${Math.max(0,100-r-a)}% stay normal`:'off - the game rolls rarity on its own';
}
function rarityLoad(c){
  document.getElementById('angelic_items').value=+(c.angelic_items||1); angelicPaint();
  document.getElementById('rarity_rare').value=+(c.rarity_rare||0);
  document.getElementById('rarity_ancient').value=+(c.rarity_ancient||0);
  rarityPaint();
}
// The plugin can refuse a switch for the rest of a session: the insert hook
// went in table-only, or the move pass shut itself down after a material it
// could not account for. `pluginMods` is what the plugin says it is doing;
// the switch keeps the saved preference, and the value beside it says what is
// actually happening, with the reason on hover (review of #54).
// The status a poll repaints (status(), renderLaunchStatus() and the lines
// below) is what it already says nearly every time, and a same-value write
// still queues a mutation and dirties style: an idle poll rewrote 14 of them
// (forgepact-ui-responsive, 2026-09-26). These write only a change.
function setText(el,v){const s=v==null?'':String(v);if(el&&el.textContent!==s)el.textContent=s}
function setClass(el,v){if(el&&el.className!==v)el.className=v}
function setTitle(el,v){if(el&&el.title!==v)el.title=v}
function setHidden(el,v){if(el&&el.hidden!==v)el.hidden=v}
// Move all into the stash turns itself off for the rest of a session after a
// move it could not confirm; the plugin's last `stashmoveall: state=` line
// says so (`stash_move_all_session`), and the value beside the switch shows
// it while the game runs (review of #68). The switch keeps the preference.
// Without a loss it leaves the value as the switch painted it.
function applyStashMoveAllSession(){
  const v=document.getElementById('msmaval');
  if(!v||!ST)return;
  if(ST.gameRunning&&ST.stash_move_all_session==='off-after-loss'){
    setText(v,'off (this session)');setClass(v,'val off');
    setTitle(v,'Move all turned itself off for this game session after a move it could not confirm; it works again after restarting the game.');
    return;
  }
  if(v.textContent==='off (this session)'){const on=!!ST.cfg?.mod_stash_move_all;setText(v,on?'on':'off');setClass(v,'val '+(on?'':'off'))}
  setTitle(v,'');
}
function applyPluginModState(pm){
  const packMarkerStatus=document.getElementById('packMarkerStatus'), packMarkers=pm?.packMarkers;
  if(packMarkerStatus){
    setHidden(packMarkerStatus,!(ST?.gameRunning&&ST?.cfg?.map_reveal&&ST?.cfg?.map_reveal_packs&&packMarkers));
    setText(packMarkerStatus,!packMarkers?'':
      packMarkers.hook==='failed'?'Pack markers unavailable: the minimap layer could not be hooked on this game version.':
      packMarkers.hook==='table'?'Pack markers may not draw on this game version (minimap hook attached table-only).':
      packMarkers.hook==='pending'?'Pack markers start once the game has settled.':
      packMarkers.marked>0?packMarkers.marked+' packs marked in this zone'+(packMarkers.spawned?' · '+packMarkers.spawned+' born so far':'')+'.':
      'No unspawned packs marked in this zone.');
  }
  const populationStatus=document.getElementById('populationStatus'), population=pm?.population;
  if(populationStatus){
    setHidden(populationStatus,!(ST?.gameRunning&&ST?.cfg?.map_reveal&&ST?.cfg?.map_reveal_spawn&&population));
    setText(populationStatus,!population?'':!population.capacityReady?'Early population unavailable: '+population.reason:
      !population.canPopulate?'Early population paused: '+population.reason:
      population.densityCopyReason?'Population waiting: '+population.densityCopyReason:
      population.unconfirmedPacks>0?'Map population is unverified: '+population.unconfirmedPacks+' groups could not be confirmed.':
      population.targetExceeded?'This zone exceeded the 5 s target.'+(population.queuedPacks?' '+population.queuedPacks+' groups waiting.':'')+(population.queuedDensityCopies?' '+population.queuedDensityCopies+' density copies waiting.':''):
      population.windowFrames>0||population.queuedDensityCopies>0?'Populating the map · 5 s target'+(population.queuedPacks?' · '+population.queuedPacks+' groups waiting':'')+(population.queuedDensityCopies?' · '+population.queuedDensityCopies+' density copies waiting':'')+'.':
      'Ready for the next zone.');
  }
  const helmet=pm?.minerHelmet;
  const helmetStatus=document.getElementById('minerHelmetStatus');
  setText(helmetStatus,!ST?.gameRunning?'Start the game to check the helmet.':
    !helmet?.available?'Waiting for the ForgePact plugin to report the helmet.':
    helmet.enabled?(helmet.reason||'Checking the equipped helmet...')+(helmet.bonusVeins>0?' \u00b7 Vein Resonance has dug '+helmet.bonusVeins+' extra veins this session.':''):
    'No Miner\'s Helmet loaded yet.');
  const miningNote=document.querySelector('.note[data-note="mining_ore"]');
  if(miningNote){
    const requested=Number(ST?.cfg?.drops?.mining_ore||1), mining=pm?.miningOre;
    let status='';
    if(ST?.gameRunning&&helmet?.enabled){
      status=' Miner\'s Helmet: x4 replaces this slider while the helmet is worn; with the helmet off, this slider applies.';
    }else if(ST?.gameRunning&&requested>1){
      if(mining?.unavailable)status=' Plugin could not enable this feature; mining remains at x1.';
      else if(mining?.ready&&mining.multiplier===requested)status=' Plugin ready at x'+requested+'.';
      else status=' Waiting for the matching mining plugin to confirm the setting.';
    }
    // Only the live status: the note is empty (and hidden) while there is none.
    setText(miningNote,status.trim());
  }
  const ap=(pm&&pm.autoprospect)||null;
  const parentVal=document.getElementById("autoprospval");
  const bagVal=document.getElementById("apbagval");
  const bagRow=document.getElementById("mod_auto_prospect_bag_row");
  if(!ap||!parentVal||!bagVal||!bagRow)return;
  const reason=ap.reason||"";
  const parentOn=document.getElementById("mod_auto_prospect").checked;
  const wantsBag=document.getElementById("mod_auto_prospect_bag").checked;
  if(ap.hookBlind){
    parentVal.textContent="off (plugin)";
    parentVal.className="val off";
    parentVal.title=reason||"the plugin turned auto-prospect off for this session";
  }else{
    // Recovery matters as much as the failure: a new launch reports healthy
    // state, and the label has to go back to the saved switch by itself -
    // the toast promised it starts again next launch (review of #54).
    parentVal.textContent=parentOn?"on":"off";
    parentVal.className="val "+(parentOn?"":"off");
    parentVal.title="";
  }
  if(wantsBag&&ap.bagPreference&&!ap.movePass&&!ap.hookBlind){
    bagVal.textContent="off (plugin)";
    bagVal.className="val off";
    bagRow.title=(reason||"the plugin turned the move off for this session")+" - it starts again next launch.";
  }else if(!ap.hookBlind){
    syncProspectBag(parentOn,wantsBag);   // repaints the label and the disabled state
  }
}
function sliderOff(sec,v){return sec==='percent_stats'?v<=0:v<=1}
export function sliderText(sec,v){return sliderOff(sec,v)?'off':(sec==='percent_stats'?'+'+v+'%':'x'+v)}
// A slider's on/off switch (Monster Density's #den_on, for every other
// slider): off keeps the value in the range and the saved config, and the
// value box reads "off" the way density's does, while the backend sends the
// slider's default. `id` is the config's switch id, "<section>.<key>" or a
// top-level key; World.svelte/Loot.svelte carry the same markup by hand.
function switchMarkup(id,label){
  return `<label class="switch slider-switch"><input type="checkbox" id="${switchControlId(id)}" data-switch="${id}" aria-label="Enable ${label}"><span class="sl"></span></label>`;
}
function switchedOff(id){return document.getElementById(switchControlId(id))?.checked===false}
// The slider a switch belongs to: a table row by its data-sec/data-key, a
// top-level slider by its element id.
const TOP_LEVEL_RANGES={enemy_speed:'enemyspeed',angelic_items:'angelic_items',rarity_rare:'rarity_rare',rarity_ancient:'rarity_ancient'};
function switchRange(id){
  const dot=id.indexOf('.');
  return dot<0?document.getElementById(TOP_LEVEL_RANGES[id]):
    document.querySelector(`input[type=range][data-sec="${id.slice(0,dot)}"][data-key="${id.slice(dot+1)}"]`);
}
function paintSwitches(c){
  document.querySelectorAll('input[data-switch]').forEach(box=>{box.checked=switchOn(c,box.dataset.switch)});
}
function row(sec,key,label,val,tagHtml,max,note,step){
  const mx=max||100, off=sliderOff(sec,val), mn=sec==='percent_stats'?0:1;
  // A keys, stats or percent_stats row always carries its note element, empty
  // at the slider's default: bind() looks it up once, so a later drag can
  // still write into it. Rows passed no note (drops, spawners) get none.
  // The note's id carries the section (data-note is the key alone), and the
  // range names it in aria-describedby, so a screen reader reads the note.
  const noteId=`note-${sec}-${key}`;
  const n=note!=null?`<div class="note" data-note="${key}" id="${noteId}">${note}</div>`:'';
  return `<div class="row"><span class="lbl">${label}${tagHtml||''}</span>
    ${switchMarkup(sec+'.'+key,label)}
    <input type="range" min="${mn}" max="${mx}" step="${step||1}" value="${val}" data-sec="${sec}" data-key="${key}"${n?` aria-describedby="${noteId}"`:''}>
    <span class="val ${off?'off':''}" style="width:64px" title="Click to type a value">${sliderText(sec,val)}</span></div>${n}`;
}
function satRow(polarity,id,name,desc,enabled){
  const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  return `<label class="sat-option ${enabled?'is-enabled':''}">
    <input type="checkbox" data-sat-polarity="${polarity}" data-sat-id="${esc(id)}" ${enabled?'checked':''}
      aria-labelledby="sat-${polarity}-${esc(id)}" aria-describedby="sat-desc-${polarity}-${esc(id)} sat${polarity}Help">
    <span><span class="sat-name" id="sat-${polarity}-${esc(id)}">${iconMarkup(PANEL_ICON_MAP.satanic[polarity]?.[id])}<span>${esc(name)}</span></span>
      <span class="sat-desc" id="sat-desc-${polarity}-${esc(id)}">${esc(desc)}</span></span>
  </label>`;
}
// Click the value next to a slider to type it.  Sliders with 100-200 steps on a
// 200 px track skip values (80, 85, 95 ...); typing lands exactly.  Enter or
// leaving the box applies through the slider's own handlers, Escape cancels.
// A slider's value once the user is done with it: a typed value is taken as is; a dragged
// value snaps to the slider's own step (the range keeps step="any" after typing so the
// browser does not round the typed number away).
function sliderVal(r){
  let v=parseFloat(r.value); if(!isFinite(v))v=0;
  if(r.dataset.typed==='1')return v;
  const st=parseFloat(r.dataset.step0||r.step)||0;
  if(r.step==='any'&&st>0){v=Math.round(v/st)*st;v=+v.toFixed(3);r.value=v;}
  return v;
}
function typable(r,valEl){
  if(!r||!valEl||valEl.dataset.typable)return;
  valEl.dataset.typable='1'; valEl.style.cursor='text'; valEl.title='Click to type a value';
  valEl.onclick=()=>{
    if(valEl.querySelector('input'))return;
    const inp=document.createElement('input');
    inp.type='number'; inp.setAttribute('aria-label',r.getAttribute('aria-label')||'Setting value'); inp.className='numedit'; inp.min=r.min; inp.max=r.max; inp.step='any'; inp.value=r.value;
    valEl.textContent=''; valEl.appendChild(inp); inp.focus(); inp.select();
    let finished=false;
    const done=async(apply)=>{
      if(finished)return; finished=true;
      let v=parseFloat(inp.value);
      if(apply&&isFinite(v)){
        const mn=parseFloat(r.min), mx=parseFloat(r.max);
        v=Math.min(mx,Math.max(mn,v)); v=+v.toFixed(2);
        if(!r.dataset.step0)r.dataset.step0=r.step||'1';
        r.step='any'; r.value=v; r.dataset.typed='1';
        try{ if(r.oninput)r.oninput(); if(r.onchange)await r.onchange(); } finally { delete r.dataset.typed; }
      } else { if(r.oninput)r.oninput(); else valEl.textContent=r.value; }
      if(!pendingWrites){refreshSavedControls();filterControlRows()}
    };
    inp.onkeydown=(e)=>{ if(e.key==='Enter'){e.preventDefault();done(true);} else if(e.key==='Escape'){e.preventDefault();done(false);} };
    inp.onblur=()=>done(true);
  };
}
// "x2" on its own says nothing - it means something different per family.
// Chaos/Bifrost already have their gate open, so x2 really is double there.  For
// Dungeon and Angelic we make the game roll a die it normally never rolls.  For
// Relic the vanilla rate outside the home zone is ZERO, so there is no "multiple"
// at all - the slider decides how often the die is rolled.
// Experience is not a drop: the game's own calculation runs untouched and only
// its RESULT is multiplied, so your own XP bonuses survive and the slider always
// gives a true multiple.
// At its default a slider writes no note (owner, 2026-09-26): its value box
// already says "off", and an idle row's note would otherwise open as a tooltip
// saying the same.
function statNote(key,v){
  if(v<=1) return '';
  if(key==='exp') return `${v}x experience per kill, on top of your own bonuses`;
  if(key==='magicfind') return `${v}x your current total Magic Find, including all bonuses`;
  if(key==='movespeed') return `${v}x your current total Movement Speed, including all bonuses`;
  return `${v}x the current total`;
}
function percentStatNote(key,v){
  if(v<=0) return '';
  if(key==='damage') return `adds ${v}% to the final hit after the game finishes its own calculation (+100% doubles it)`;
  if(key==='castrate') return `adds ${v} Faster Cast Rate points to the current value`;
  if(key==='critchance'||key==='spellcritchance') return `increases the current Critical Strike Chance by ${v}% (the game's own cap still applies)`;
  return `adds ${v}% to the final value`;
}
function rareNote(key,v){
  if(v<=1) return 'off';
  if(key==='angelic'){
    if(v===2) return "the game's own angelic rate - it never rolls at all without this";
    return `the game's own angelic rate, multiplied ${v-1}x (measured: x10 works, higher breaks the game's check)`;
  }
  if(key==='heroic'){
    const p=Math.min(100,Math.round(28*v));
    return `${p}% chance for a Heroic item per drop (vanilla 28%)`;
  }
  if(key==='ceiling'){
    return `${v}x on every rare tier at once - takes effect when the NEXT map loads`;
  }
  if(key==='satanic'){
    return `monsters count as ${v}x their level for the Satanic tier roll (capped at level 200)`;
  }
  return `${v}x`;
}
function keyNote(key,dropType,v){
  if(v<=1) return '';
  if(key==='ruby') return `${v}x the key's own vanilla roll (base 1,500,000)`;
  if(key==='primeevil') return `${v}x how often bosses drop their Key of Terror part (bosses only; nothing more above x35)`;
  if(dropType===null||dropType===undefined) return `${v}x its vanilla drop rate, only where the game drops it anyway`;
  if(key==='relic'){
    // Same curve as the plugin:  probability = 0.00025 * v^2  (clamped at 1.0)
    const p=Math.min(1,0.00025*v*v);
    return (p>=1)?'rolls on every kill':`rolls on about 1 kill in ${Math.round(1/p).toLocaleString()}`;
  }
  return `${v}x its vanilla drop rate; where the game never rolls this family, the roll is opened at the normal-key chance first`;
}
async function boot(){
  setST(await j('/api/state'));
  const c=ST.cfg;
  document.querySelectorAll('.tabbtn').forEach(b=>b.onclick=()=>openTab(b.dataset.tab));
  const palette=applyTheme(c.theme);
  let initial=c.game_exe?(palette==='ember'?'overview':'modifiers'):'setup';
  try{initial=sessionStorage.getItem('forgepact_tab')||initial}catch(e){}
  try{setModsSubtab(sessionStorage.getItem('forgepact_mods_subtab')||modsSubtab)}catch(e){}
  openTab(initial,false);
  document.getElementById('autoapply').checked=!!c.auto_apply;
  document.getElementById('den_on').checked=!!c.density_on;
  const esp=+(c.enemy_speed||0), esc=(c.enemy_speed_ct!==false);
  document.getElementById('enemyspeed').value=esp;
  document.getElementById('enemyspeedval').textContent=esp>0?'+'+esp+'%':'off';
  document.getElementById('enemyspeedval').className='val '+(esp>0?'':'off');
  document.getElementById('enemyspeed_ct').checked=esc;
  document.getElementById('enemyspeedctval').textContent=esc?'CT only':'all zones';
  document.getElementById('den').value=c.density;
  document.getElementById('denval').textContent=(c.density_on?'x'+c.density:'off');
  document.getElementById('denval').className='val '+(c.density_on?'':'off');
  const mr=c.map_reveal!==false;
  document.getElementById('map_reveal').checked=mr;
  document.getElementById('mapval').textContent=mr?'on':'off';
  document.getElementById('mapval').className='val '+(mr?'':'off');
  const mrp=c.map_reveal_packs!==false;
  document.getElementById('map_reveal_packs').checked=mrp;
  const mrs=!!c.map_reveal_spawn;
  document.getElementById('map_reveal_spawn').checked=mrs;
  syncRevealPacks(mr,mrp,mrs);
  const hh=!!c.headhunter;
  document.getElementById('headhunter').checked=hh;
  document.getElementById('hhval').textContent=hh?'on':'off';
  const ty=!!c.tyrant;
  document.getElementById('tyrant').checked=ty;
  document.getElementById('tyval').textContent=ty?'on':'off';
  document.getElementById('tyval').className='val '+(ty?'':'off');
  const be=!!c.beacon;
  document.getElementById('beacon').checked=be;
  document.getElementById('beval').textContent=be?'on':'off';
  document.getElementById('beval').className='val '+(be?'':'off');
  const mfmr=!!c.mod_filter_max_relics;
  document.getElementById('mod_filter_max_relics').checked=mfmr;
  document.getElementById('mfmrval').textContent=mfmr?'on':'off';
  document.getElementById('mfmrval').className='val '+(mfmr?'':'off');
    const mor=!!c.mod_orb_pickup_radius;
    document.getElementById('mod_orb_pickup_radius').checked=mor;
    document.getElementById('morval').textContent=mor?'on':'off';
    document.getElementById('morval').className='val '+(mor?'':'off');
    const mpqp=!!c.mod_pet_quest_pickup;
    document.getElementById('mod_pet_quest_pickup').checked=mpqp;
    document.getElementById('mpqpval').textContent=mpqp?'on':'off';
    document.getElementById('mpqpval').className='val '+(mpqp?'':'off');
    const maps=!!c.mod_auto_prospect;
    document.getElementById('mod_auto_prospect').checked=maps;
    document.getElementById('autoprospval').textContent=maps?'on':'off';
    document.getElementById('autoprospval').className='val '+(maps?'':'off');
    const apbag=c.mod_auto_prospect_bag!==false;
    document.getElementById('mod_auto_prospect_bag').checked=apbag;
    syncProspectBag(maps,apbag);
    const mti=!!c.mod_toggle_indicator;
    document.getElementById('mod_toggle_indicator').checked=mti;
    document.getElementById('mtival').textContent=mti?'on':'off';
    document.getElementById('mtival').className='val '+(mti?'':'off');
    const mtg=!!c.mod_toggle_guard;
    document.getElementById('mod_toggle_guard').checked=mtg;
    document.getElementById('mtgval').textContent=mtg?'on':'off';
    document.getElementById('mtgval').className='val '+(mtg?'':'off');
    const mra=!!c.mod_restart_anytime;
    document.getElementById('mod_restart_anytime').checked=mra;
    document.getElementById('mraval').textContent=mra?'on':'off';
    document.getElementById('mraval').className='val '+(mra?'':'off');
    const mcm=!!c.mod_craft_mats;
    document.getElementById('mod_craft_mats').checked=mcm;
    document.getElementById('mcmval').textContent=mcm?'on':'off';
    document.getElementById('mcmval').className='val '+(mcm?'':'off');
    const msma=!!c.mod_stash_move_all;
    document.getElementById('mod_stash_move_all').checked=msma;
    document.getElementById('msmaval').textContent=msma?'on':'off';
    document.getElementById('msmaval').className='val '+(msma?'':'off');
    const mfs=!!c.mod_far_sleep;
    document.getElementById('mod_far_sleep').checked=mfs;
    document.getElementById('mfsval').textContent=mfs?'on':'off';
    document.getElementById('mfsval').className='val '+(mfs?'':'off');
    for(const [id,val,key] of [['mod_gem_mythic','mgmval','mod_gem_mythic'],['mod_gem_maxroll','mgrval','mod_gem_maxroll']]){
      const on=!!c[key];
      document.getElementById(id).checked=on;
      document.getElementById(val).textContent=on?'on':'off';
      document.getElementById(val).className='val '+(on?'':'off');
    }
    document.getElementById('mod_skill_timer_style').value=c.mod_skill_timer_style||'off';
  rarityLoad(c);
  document.getElementById('hhval').className='val '+(hh?'':'off');
  document.getElementById('exepath').value=c.game_exe||'';
  document.getElementById('spawners').innerHTML=ST.spawners.map(([k,i,l,mx])=>row('spawners',k,l,c.spawners[k]||1,'',mx)).join('');
  renderSatanicMods();
  document.getElementById('keys').innerHTML=ST.keys.map(([k,l,t])=>{
    const v=(c.keys&&c.keys[k])||1;
    return row('keys',k,l,v,'',100,keyNote(k,t,v));
  }).join('');
  // The mining row keeps an empty note: applyPluginModState() writes the
  // plugin's live mining status into it while the game runs. row() writes it
  // (an empty note), so it gets its id and the range's aria-describedby too.
  document.getElementById('drops').innerHTML=ST.drops.map(([k,l,h])=>
    row('drops',k,l,(c.drops&&c.drops[k])||1,h?` <span class="tag">${h}</span>`:'',k==='mining_ore'?10:100,k==='mining_ore'?'':null)).join('');
  document.getElementById('stats').innerHTML=(ST.stats||[]).map(([k,l,mx,step])=>{
    const v=(c.stats&&c.stats[k])||1;
    return row('stats',k,l,v,'',mx,statNote(k,v),step);
  }).join('');
  const percentRows=(keys)=>(ST.percentStats||[]).filter(([k])=>keys.includes(k)).map(([k,l,mx,step,mode])=>{
    const v=(c.percent_stats&&c.percent_stats[k])||0;
    return row('percent_stats',k,l,v,'',mx,percentStatNote(k,v),step);
  }).join('');
  document.getElementById('offensivestats').innerHTML=percentRows(['damage','attackspeed','castrate']);
  document.getElementById('sustainstats').innerHTML=percentRows(['lifereplenish','manareplenish','defense']);
  document.getElementById('criticalstats').innerHTML=percentRows(['critdamage','critchance','spellcritdamage','spellcritchance']);
  paintSwitches(c);
  document.getElementById('theme').value=applyTheme(c.theme);
  bind(); preparePanelUI(); refreshSavedControls(); renderEnabledMods(ST.cfg); status(); paintVersion();
  document.dispatchEvent?.(new Event('forgepact:ready'));
  document.getElementById('saveIndicator').textContent='Settings loaded';
}
function paintVersion(){
  // Rendered from /api/state, never embedded in this page: the panel and the
  // plugin DLL are installed separately and can be different builds, and a
  // bug report needs to say which one it is looking at.
  const el=document.getElementById('panelver');
  if(el&&ST&&ST.version)el.textContent=' \u00b7 v'+ST.version;
}
let launcherBusy=false;
function renderLaunchStatus(){
  const info=ST?.launch;
  const btn=document.getElementById('launchgame');
  const disabled=launcherBusy||!!ST?.gameRunning||info?.phase==='starting';
  if(btn.disabled!==disabled)btn.disabled=disabled;
  const box=document.getElementById('launchFeedback');
  if(info){setText(box,info.message);setClass(box,'launch-feedback '+info.phase);}
}
function status(){
  const g=document.getElementById('chipGame'), a=document.getElementById('chipApply');
  const ch=ST.chain||{};
  const ok=ch.patched&&ch.aurieCore&&ch.yytk&&ch.plugin;
  setText(g,ST.gameRunning?(ok?'Game open':'Game open · plugin missing'):'Game offline');
  setTitle(g,ST.gameRunning?'This detects the game process. The plugin must be installed and loaded to apply modifiers.':'Settings are saved locally. Auto-apply sends them on game launch when enabled.');
  setClass(g,'chip '+(ST.gameRunning?(ok?'on':'warn'):'off'));
  setText(a,ST.lastApplied?('commands sent: '+ST.lastApplied+(ST.queued?' (queued)':'')):'No settings sent this session');
  setClass(a,'chip '+(ST.lastApplied?'warn':'off'));
  const warning=document.getElementById('pluginWarning');
  setHidden(warning,!!ok);
  setText(document.getElementById('pluginWarningText'),ch.exeExists?
    'Plugin not installed. Your settings are saved, but modifiers cannot apply. Close the game, then install the plugin in Setup.':
    'Choose your Hero_Siege.exe in Setup, then install the plugin to use modifiers.');
  const cn=document.getElementById('chainnote');
  if(ok){setText(cn,'');}
  else{
    const miss=[];
    if(!ch.patched)miss.push('exe not patched');
    if(!ch.aurieCore)miss.push('AurieCore.dll');
    if(!ch.yytk)miss.push('YYToolkit.dll');
    if(!ch.plugin)miss.push('mod plugin');
    // The button's name is a mono run with no quotes (finish review F5), in one
    // span so #chainnote's flex row keeps it inline. Written only when the
    // words change, as setText() does, so an idle poll mutates nothing.
    const lead='mod chain incomplete: '+miss.join(', ')+' - click ';
    if(cn.textContent!==lead+'Install Mod Plugin (game must be closed)')cn.innerHTML=`<span>${lead}<span class="chain-command">Install Mod Plugin</span> (game must be closed)</span>`;
    cn.style.color='var(--color-warn)';
  }
  setText(document.getElementById('ipcnote'),ST.ipcOk?'':'bp_ipc appears after the first modded launch');
  document.getElementById('ipcnote').style.color='var(--color-text-faint)';
  const en=document.getElementById('eacnote');
  if(ST.eacStatus==='legit_eac'){setText(en,'Note: this looks like a Steam/EAC copy. If EAC is active, online play may break and the mod may not load (EAC can relaunch the clean exe). Your exe is backed up - Remove Plugin reverts it. For best results use an offline / EAC-off copy. Installing is allowed at your own risk.');en.style.color='var(--color-warn)';}
  else if(ST.eacStatus==='eac_free'){setText(en,'');}
  else{setText(en,'');}
  renderLaunchStatus();
}
function bind(){
  document.querySelectorAll('input[type=range][data-sec]').forEach(r=>{
    const valEl=r.parentElement.querySelector('.val');
    const noteEl=r.parentElement.parentElement.querySelector(`.note[data-note="${r.dataset.key}"]`);
    const tipOf=(k)=>{const e=(ST.keys||[]).find(x=>x[0]===k);return e?e[2]:undefined;};
    const swId=r.dataset.sec+'.'+r.dataset.key;
    r.oninput=()=>{const v=sliderVal(r),off=switchedOff(swId);valEl.textContent=off?'off':sliderText(r.dataset.sec,v);valEl.className='val '+(off||sliderOff(r.dataset.sec,v)?'off':'');
      if(noteEl&&r.dataset.sec==='keys')noteEl.textContent=keyNote(r.dataset.key,tipOf(r.dataset.key),v);
      if(noteEl&&r.dataset.sec==='stats')noteEl.textContent=statNote(r.dataset.key,v);
      if(noteEl&&r.dataset.sec==='percent_stats')noteEl.textContent=percentStatNote(r.dataset.key,v);
    };
    r.onchange=async()=>{
      const v=sliderVal(r);
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:r.dataset.sec,key:r.dataset.key,value:v})});
      toast((r.dataset.key)+' = '+sliderText(r.dataset.sec,v)+' - '+(res.ok||res.err));
    };
    typable(r,valEl);
  });
  const den=document.getElementById('den');
  den.oninput=()=>{const v=sliderVal(den);document.getElementById('denval').textContent=document.getElementById('den_on').checked?'x'+v:'off'};
  den.onchange=async()=>{const v=sliderVal(den);const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'density',value:v})});toast('density x'+v+' - '+(res.ok||res.err))};
  typable(den,document.getElementById('denval'));
  document.getElementById('den_on').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'density_on',value:e.target.checked})});
    document.getElementById('denval').textContent=e.target.checked?'x'+den.value:'off';
    document.getElementById('denval').className='val '+(e.target.checked?'':'off');
    toast('density '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('autoapply').onchange=async(e)=>{
    await j('/api/set',{method:'POST',body:JSON.stringify({key:'auto_apply',value:e.target.checked})});
    toast('auto-apply '+(e.target.checked?'ON':'OFF'));
  };
  const esp=document.getElementById('enemyspeed');
  const espText=(v)=>v>0?'+'+v+'%':'off';
  esp.oninput=()=>{const v=sliderVal(esp),on=v>0&&!switchedOff('enemy_speed');document.getElementById('enemyspeedval').textContent=on?espText(v):'off';document.getElementById('enemyspeedval').className='val '+(on?'':'off');};
  esp.onchange=async()=>{
    const v=sliderVal(esp);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'enemy_speed',value:v})});
    toast('enemy speed '+espText(v)+' - '+(res.ok||res.err));
  };
  typable(esp,document.getElementById('enemyspeedval'));
  document.getElementById('enemyspeed_ct').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'enemy_speed_ct',value:e.target.checked})});
    document.getElementById('enemyspeedctval').textContent=e.target.checked?'CT only':'all zones';
    toast('enemy speed scope: '+(e.target.checked?'Chaos Tower only':'all zones')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal').onchange=async(e)=>{
    // Repaint the pair BEFORE awaiting the POST. If the panel's server is
    // gone the fetch throws, and anything after the await never runs - which
    // left the child row enabled and reading "on" under a switched-off
    // parent, inviting a click that could do nothing.
    document.getElementById('mapval').textContent=e.target.checked?'on':'off';
    document.getElementById('mapval').className='val '+(e.target.checked?'':'off');
    syncRevealPacks(e.target.checked,document.getElementById('map_reveal_packs').checked,document.getElementById('map_reveal_spawn').checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal',value:e.target.checked})});
    toast('map reveal '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal_packs').onchange=async(e)=>{
    syncRevealPacks(document.getElementById('map_reveal').checked,e.target.checked,document.getElementById('map_reveal_spawn').checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal_packs',value:e.target.checked})});
    toast('pack markers '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal_spawn').onchange=async(e)=>{
    syncRevealPacks(document.getElementById('map_reveal').checked,document.getElementById('map_reveal_packs').checked,e.target.checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal_spawn',value:e.target.checked})});
    toast('spawn every pack '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('headhunter').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'headhunter',value:e.target.checked})});
    document.getElementById('hhval').textContent=e.target.checked?'on':'off';
    document.getElementById('hhval').className='val '+(e.target.checked?'':'off');
    toast('headhunter '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('tyrant').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'tyrant',value:e.target.checked})});
    document.getElementById('tyval').textContent=e.target.checked?'on':'off';
    document.getElementById('tyval').className='val '+(e.target.checked?'':'off');
    toast('tyrant '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('beacon').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'beacon',value:e.target.checked})});
    document.getElementById('beval').textContent=e.target.checked?'on':'off';
    document.getElementById('beval').className='val '+(e.target.checked?'':'off');
    toast('beacon '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('mod_filter_max_relics').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_filter_max_relics',value:e.target.checked})});
    const v=document.getElementById('mfmrval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
    toast('Remove owned relics from drop pool '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
    document.getElementById('mod_orb_pickup_radius').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_orb_pickup_radius',value:e.target.checked})});
        const v=document.getElementById('morval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Orb pickup radius '+(e.target.checked?'10x ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_pet_quest_pickup').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_pet_quest_pickup',value:e.target.checked})});
        const v=document.getElementById('mpqpval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Pet collects quest items '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_auto_prospect').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_auto_prospect',value:e.target.checked})});
        const v=document.getElementById('autoprospval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        syncProspectBag(e.target.checked,document.getElementById('mod_auto_prospect_bag').checked);
        const pmp=await pluginModsAfterSet();
        if(e.target.checked&&pmp&&pmp.autoprospect&&pmp.autoprospect.hookBlind){
          toast('The plugin has auto-prospect off this session: '+(pmp.autoprospect.reason||'it could not attach to the game'));
        }else{
        toast('Auto-prospect '+(e.target.checked?'ON - '+(document.getElementById('mod_auto_prospect_bag').checked?'the previous materials go to your materials tab':'materials stay in the grid'):'OFF')+' - '+(res.ok||res.err));
        }
    };
    document.getElementById('mod_auto_prospect_bag').onchange=async(e)=>{
        syncProspectBag(document.getElementById('mod_auto_prospect').checked,e.target.checked);
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_auto_prospect_bag',value:e.target.checked})});
        // The plugin may refuse this for the rest of the session; say what
        // it is doing, not what was asked for (review of #54).
        const pm=await pluginModsAfterSet();
        const ap=pm&&pm.autoprospect;
        if(e.target.checked&&ap&&ap.bagPreference&&!ap.movePass){
          toast('The plugin is not moving materials this session: '+(ap.reason||'it turned the move off')+' - it starts again next launch');
        }else{
          toast('Materials to your materials tab '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
        }
    };
    document.getElementById('mod_toggle_indicator').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_toggle_indicator',value:e.target.checked})});
        const v=document.getElementById('mtival');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Toggle-skill outline '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_toggle_guard').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_toggle_guard',value:e.target.checked})});
        const v=document.getElementById('mtgval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Toggle-skill double cast guard '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_restart_anytime').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_restart_anytime',value:e.target.checked})});
        const v=document.getElementById('mraval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Restart zone at any time '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_craft_mats').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_craft_mats',value:e.target.checked})});
        const v=document.getElementById('mcmval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Craft from the stash '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_stash_move_all').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_stash_move_all',value:e.target.checked})});
        const v=document.getElementById('msmaval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        applyStashMoveAllSession();
        toast('Move all into the stash '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_far_sleep').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_far_sleep',value:e.target.checked})});
        const v=document.getElementById('mfsval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Far scenery sleep '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    // Gem mod filter: drawn from /api/state's gemAffixes ([stat, category, label])
    // with the World tab's Satanic pool classes, under six category headings.
    // The search and the All mods / Enabled / Disabled filter only show and
    // hide rows; Tick all, Untick all, a heading's all / none and Save act on
    // every row, shown or hidden, and only Save sends anything (no auto-save,
    // unlike the Satanic pool: nothing ticked is refused at Save).
    const gemFilterSummary=()=>{
        const v=ST.cfg.gem_filter, n=(ST.gemAffixes||[]).length;
        document.getElementById('gemfilter_summary').textContent=Array.isArray(v)?v.length+' of '+n:'all '+n;
    };
    const renderGemFilter=()=>{
        const box=document.getElementById('gemfilter_panel'), v=ST.cfg.gem_filter, affixes=ST.gemAffixes||[];
        const on=new Set(Array.isArray(v)?v:affixes.map(a=>a[0]));
        let html='<div class="gf-actions"><button class="btn primary" type="button" data-gf="save">Save filter</button><button class="sat-button" type="button" data-gf="all">Tick all</button><button class="sat-button" type="button" data-gf="none">Untick all</button></div>';
        html+='<div class="sat-toolbar"><label class="sat-search"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></svg>'+
            '<input type="search" id="gemSearch" placeholder="Search by name or effect..." aria-label="Search Gem of Incarnation mods" autocomplete="off"></label>'+
            '<div class="sat-filters" role="group" aria-label="Filter Gem of Incarnation mods"><button type="button" data-gf-filter="all" aria-pressed="true">All mods</button><button type="button" data-gf-filter="enabled" aria-pressed="false">Enabled</button><button type="button" data-gf-filter="disabled" aria-pressed="false">Disabled</button></div></div>';
        html+='<div class="sat-list" role="group" aria-label="Gem of Incarnation mods">';
        for(const cat of (ST.gemCategories||[])){
            html+='<div class="gf-cat" data-gf-group="'+cat+'"><h3>'+cat+'</h3><span class="sat-count"></span><button class="sat-button" type="button" data-gfcat="'+cat+'" data-gfset="1">all</button><button class="sat-button" type="button" data-gfcat="'+cat+'" data-gfset="0">none</button></div>';
            for(const [stat,c,label] of affixes){if(c!==cat)continue;html+='<label class="sat-option"><input type="checkbox" data-gfstat="'+stat+'" data-gfc="'+c+'"'+(on.has(stat)?' checked':'')+'><span class="sat-name">'+label+'</span></label>';}
        }
        html+='<p class="sat-empty" hidden>No matching modifiers.<br>Try another search or filter.</p></div>';
        box.innerHTML=html;
        const boxes=()=>[...box.querySelectorAll('input[data-gfstat]')];
        const list=box.querySelector('.sat-list'), search=document.getElementById('gemSearch'), unsaved=document.getElementById('gemfilter_unsaved');
        let shown='all';
        // Repaints what the ticks and the search show; changes no tick.
        const paint=()=>{
            const q=search.value.trim().toLocaleLowerCase(), saved=ST.cfg.gem_filter;
            const savedSet=new Set(Array.isArray(saved)?saved:affixes.map(a=>a[0]));
            let visible=0, differs=false;
            for(const i of boxes()){
                const row=i.closest('.sat-option'), stat=+i.dataset.gfstat;
                row.classList.toggle('is-enabled',i.checked);
                if(i.checked!==savedSet.has(stat))differs=true;
                const match=(row.textContent.toLocaleLowerCase().includes(q)||i.dataset.gfc.toLocaleLowerCase().includes(q))&&(shown==='all'||(shown==='enabled')===i.checked);
                row.hidden=!match;
                if(match)visible++;
            }
            if(boxes().filter(i=>i.checked).length!==savedSet.size)differs=true;
            box.querySelectorAll('.gf-cat').forEach(head=>{
                const rows=boxes().filter(i=>i.dataset.gfc===head.dataset.gfGroup);
                head.querySelector('.sat-count').textContent=rows.filter(i=>i.checked).length+' enabled';
                head.hidden=!rows.some(i=>!i.closest('.sat-option').hidden);
            });
            box.querySelector('.sat-empty').hidden=visible>0;
            box.querySelectorAll('[data-gf-filter]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.gfFilter===shown)));
            unsaved.hidden=!differs;
            list.toggleAttribute('data-more-below',list.scrollHeight-list.scrollTop-list.clientHeight>1);
        };
        search.oninput=paint;
        list.onscroll=()=>list.toggleAttribute('data-more-below',list.scrollHeight-list.scrollTop-list.clientHeight>1);
        box.querySelectorAll('[data-gf-filter]').forEach(b=>b.onclick=()=>{shown=b.dataset.gfFilter;paint();});
        list.onchange=paint;
        box.querySelectorAll('[data-gfcat]').forEach(b=>b.onclick=()=>{boxes().forEach(i=>{if(i.dataset.gfc===b.dataset.gfcat)i.checked=b.dataset.gfset==='1';});paint();});
        box.querySelectorAll('[data-gf]').forEach(b=>b.onclick=async()=>{
            if(b.dataset.gf!=='save'){boxes().forEach(i=>i.checked=b.dataset.gf==='all');paint();return;}
            const ticked=boxes().filter(i=>i.checked).map(i=>+i.dataset.gfstat);
            if(!ticked.length){toast('Gem filter: tick at least one mod');return;}
            const value=ticked.length===affixes.length?'all':ticked;
            const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'gem_filter',value:value})});
            if(res.ok){ST.cfg.gem_filter=Array.isArray(value)?[...value].sort((a,b)=>a-b):value;gemFilterSummary();paint();}
            toast('Gem filter: '+(value==='all'?'every mod':ticked.length+' mods')+' - '+(res.ok||res.err));
        });
        paint();
        // Drawn from the saved filter: nothing unsaved.
        unsaved.hidden=true;
    };
    document.getElementById('gemfilter_toggle').onclick=()=>{
        const toggle=document.getElementById('gemfilter_toggle'), box=document.getElementById('gemfilter_panel'), open=box.style.display==='none';
        box.style.display=open?'block':'none';
        if(open)renderGemFilter();
        toggle.setAttribute('aria-expanded',String(open));
    };
    gemFilterSummary();
    for(const [id,val,key,label] of [['mod_gem_mythic','mgmval','mod_gem_mythic','Mythic Gems of Incarnation'],['mod_gem_maxroll','mgrval','mod_gem_maxroll','Max-roll Gems of Incarnation']]){
        document.getElementById(id).onchange=async(e)=>{
            const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:key,value:e.target.checked})});
            const v=document.getElementById(val);v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
            toast(label+' '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
        };
    }
    document.getElementById('mod_skill_timer_style').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_skill_timer_style',value:e.target.value})});
        toast('Timed skill countdown: '+e.target.value+' - '+(res.ok||res.err));
    };
  { const el=document.getElementById('angelic_items');
    el.oninput=angelicPaint;
    el.onchange=async()=>{ const v=sliderVal(el); const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'angelic_items',value:v})}); angelicPaint(); toast('angelic drops '+(v>1?'x'+v:'off')+' - '+(res.ok||res.err)); };
    typable(el,document.getElementById('angelicval')); }
  for(const key of ['rarity_rare','rarity_ancient']){
    const el=document.getElementById(key);
    el.oninput=rarityPaint;
    el.onchange=async()=>{
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({key,value:sliderVal(el)})});
      // the server may have cut the other share so the two stay within 100
      if(res.cfg) rarityLoad(res.cfg); else rarityPaint();
      toast('monster rarity: '+document.getElementById('raritynote').textContent+' - '+(res.ok||res.err));
    };
    typable(el,document.getElementById(key==='rarity_rare'?'rarityrareval':'rarityancval'));
  }
  // Every slider's switch goes through this one handler. The value box is
  // repainted first, from the range's own value (marked typed so a saved
  // decimal is not snapped to the drag step); the backend decides what the
  // game gets - the default while off, the remembered value once on again.
  document.querySelectorAll('input[data-switch]').forEach(box=>{
    box.onchange=async()=>{
      const range=switchRange(box.dataset.switch);
      if(range?.oninput){const typed=range.dataset.typed;range.dataset.typed='1';range.oninput();if(typed===undefined)delete range.dataset.typed;else range.dataset.typed=typed}
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:'switches',key:box.dataset.switch,value:box.checked})});
      toast(res.ok||res.err);
    };
  });
  // The theme is a panel setting saved in forgepact.json like any other (the
  // page's own storage is private to pywebview), painted as data-theme on the
  // root; the answer's cfg.theme is what stays painted.
  document.getElementById('theme').onchange=async(e)=>{
    applyTheme(e.target.value);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'theme',value:e.target.value})});
    e.target.value=applyTheme((res.cfg||ST.cfg).theme);
    toast(res.ok||res.err);
  };
  document.getElementById('applyall').onclick=async()=>{
    const res=await j('/api/applyall',{method:'POST',body:'{}'});
    toast(res.ok||res.err); if(!res.err)ST.lastApplied=new Date().toTimeString().slice(0,8); status();
  };
  document.getElementById('installmod').onclick=async()=>{
    const btn=document.getElementById('installmod');
    btn.disabled=true; btn.textContent='Installing...';
    const res=await j('/api/installmod',{method:'POST',body:'{}'});
    btn.disabled=false; btn.textContent='Install Mod Plugin';
    if(res.chain)ST.chain=res.chain;
    toast(res.ok||res.err); status();
  };
  document.getElementById('removeplugin').onclick=async()=>{
    const btn=document.getElementById('removeplugin');
    btn.disabled=true; btn.textContent='Removing...';
    const res=await j('/api/removeplugin',{method:'POST',body:'{}'});
    btn.disabled=false; btn.textContent='Remove Plugin';
    if(res.chain)ST.chain=res.chain;
    toast(res.ok||res.err); status();
  };
  document.getElementById('exesave').onclick=async()=>{
    const res=await j('/api/setexe',{method:'POST',body:JSON.stringify({path:document.getElementById('exepath').value})});
    if(res.ok){ST.ipcOk=res.ipcOk}
    toast(res.ok||res.err); status();
  };
  document.getElementById('launchgame').onclick=async()=>{
    if(launcherBusy||ST.gameRunning||ST.launch?.phase==='starting')return;
    const btn=document.getElementById('launchgame'),old=btn.innerHTML;
    launcherBusy=true;btn.disabled=true;btn.textContent='Starting offline...';
    const box=document.getElementById('launchFeedback');
    box.textContent='Checking the game and Steam...';box.className='launch-feedback starting';
    try{
      const res=await j('/api/launch',{method:'POST',body:'{}'});
      ST.launch=res.launch||{phase:res.err?'error':'started',message:res.err||res.ok};
      toast(res.ok||res.err);
    }finally{
      launcherBusy=false;btn.innerHTML=old;renderLaunchStatus();
    }
  };
  document.getElementById('exebrowse').onclick=async()=>{
    const btn=document.getElementById('exebrowse'); const old=btn.innerHTML;
    btn.disabled=true; btn.textContent='Choose file...';
    const res=await j('/api/browseexe',{method:'POST',body:'{}'});
    btn.disabled=false; btn.innerHTML=old;
    if(res.path) document.getElementById('exepath').value=res.path;
    if(res.ipcOk!==undefined) ST.ipcOk=res.ipcOk;
    if(res.cfg) ST.cfg=res.cfg;
    toast(res.ok||res.err); status();
  };
  bindSatanicMods();
}
function preparePanelUI(){
  const workspace=document.getElementById('workspace');
  // The tab bar is one horizontal row at every width (App.svelte declares
  // aria-orientation="horizontal"); the old sidebar's width-driven toggle is gone.
  if(!workspace.dataset.navigationReady)workspace.dataset.navigationReady='1';
  // Real DOM order matches the visual and keyboard order.
  ['densityCard','rarityCard','satanicMods','speedCard','spawnsCard'].forEach(id=>workspace.appendChild(document.getElementById(id)));
  for(const id of ['densityCard','rarityCard'])document.getElementById(id).classList.add('half');
  const summaries={
    densityCard:'Adjust the number of monster packs in newly loaded zones.',
    rarityCard:'Choose the share of normal monsters upgraded to Rare or Ancient.',
    speedCard:'Increase enemy movement speed. Choose all zones or Chaos Tower only.',
    spawnsCard:'Choose how frequently special content appears in new zones.',
    dropsCard:'Multiply drop chances. ×1 keeps a drop at its normal rate.',
    angelicCard:'Extra chances to drop Angelic / Unholy items on monster kills.'
  };
  for(const [id,summary] of Object.entries(summaries)){
    const card=document.getElementById(id);
    if(card.dataset.prepared)continue;
    card.dataset.prepared='1';
    const hint=card.querySelector('.hint'),details=document.createElement('details');
    details.className='help-details';details.innerHTML='<summary>How it works</summary>';
    hint.before(Object.assign(document.createElement('div'),{className:'hint',textContent:summary}));
    details.append(hint);
    if(id==='densityCard'){
      const note=card.querySelector('.note');if(note)details.append(note);
      const head=document.createElement('div');head.className='density-top';
      const heading=card.querySelector('h2');heading.before(head);head.append(heading);
      const toggle=card.querySelector('.switch');const row=document.createElement('div');row.className='row';
      row.innerHTML='<span class="lbl">Enabled</span>';row.append(toggle);head.append(row);
      const hero=document.createElement('div');hero.className='hero-number';hero.id='densityHero';
      card.querySelector(':scope>.row').before(hero);
    }
    card.append(details);
  }
  for(const id of ['spawners','dropSettings'])document.getElementById(id).classList.add('settings-grid');
  for(const id of ['qolCard','itemsCard']){
    const card=document.getElementById(id);
    if(card.querySelector('.mods-grid'))continue;
    const grid=document.createElement('div');grid.className='mods-grid';
    card.querySelectorAll(':scope>.row').forEach(row=>{
      row.classList.add('feature-card');
      const description=row.querySelector('.lbl>span');
      if(description){row.querySelector('.lbl>br')?.remove();description.classList.add('feature-description');row.append(description)}
      grid.append(row);
    });
    card.append(grid);
    if(id==='qolCard'){
      const parent=document.getElementById('map_reveal').closest('.row'),child=document.getElementById('map_reveal_packs_row'),spawnChild=document.getElementById('map_reveal_spawn_row');
      const group=document.createElement('div');group.className='feature-with-child';parent.before(group);group.append(parent,child,spawnChild);
      const apParent=document.getElementById('mod_auto_prospect').closest('.row'),apChild=document.getElementById('mod_auto_prospect_bag_row');
      const apGroup=document.createElement('div');apGroup.className='feature-with-child';apParent.before(apGroup);apGroup.append(apParent,apChild);
    }
    setupModsColumns(grid);
  }
  document.querySelectorAll('input[type=range]').forEach((range,index)=>{
    const row=range.closest('.row');if(!row)return;
    const label=row.querySelector('.lbl')?.textContent.trim()||'Density multiplier';
    range.setAttribute('aria-label',label);
    if(!range.id)range.id='setting-'+(range.dataset.sec||'value')+'-'+(range.dataset.key||index);
    if(range.parentElement.classList.contains('range-control'))return;
    const value=row.querySelector('.val');if(!value)return;
    const controls=document.createElement('div');controls.className='range-control';range.before(controls);controls.append(range);
    const stepper=document.createElement('div');stepper.className='value-stepper';
    for(const direction of [-1,1]){
      const button=document.createElement('button');button.type='button';button.className='step-button';button.textContent=direction<0?'−':'+';
      button.setAttribute('aria-label',(direction<0?'Decrease ':'Increase ')+label);
      button.onclick=async()=>{
        const step=parseFloat(range.dataset.step0||range.step)||1;
        range.value=Math.max(+range.min,Math.min(+range.max,+(Number(range.value)+direction*step).toFixed(2)));
        range.dispatchEvent(new Event('input',{bubbles:true}));
        if(range.onchange)await range.onchange();
        updateControlDecoration();filterControlRows();
      };
      stepper.append(button);if(direction<0)stepper.append(value);
    }
    controls.append(stepper);
    value.setAttribute('role','button');value.tabIndex=0;value.setAttribute('aria-label','Edit '+label);
    value.onkeydown=e=>{if(e.target===value&&['Enter',' '].includes(e.key)){e.preventDefault();value.click()}};
    range.addEventListener('input',updateControlDecoration);
    if(range.dataset.sec){
      const entry=document.createElement('div');entry.className='setting-entry';
      const note=row.nextElementSibling?.matches('.note')?row.nextElementSibling:null;
      row.before(entry);entry.append(row);if(note)entry.append(note);
      entry.dataset.search=(label+' '+(note?.textContent||'')).toLowerCase();
    }
  });
  document.querySelectorAll('.switch input').forEach(box=>{
    if(!box.getAttribute('aria-label'))box.setAttribute('aria-label',box.closest('.row')?.querySelector('.lbl')?.childNodes[0]?.textContent.trim()||'Enable setting');
  });
  document.getElementById('den_on').setAttribute('aria-label','Enable monster density');
  document.getElementById('exepath').setAttribute('aria-label','Hero Siege executable path');
  document.getElementById('controlSearch').oninput=filterControlRows;
  document.querySelectorAll('[data-control-filter]').forEach(button=>button.onclick=()=>{setControlFilter(button.dataset.controlFilter);filterControlRows()});
  document.querySelectorAll('.tabbtn').forEach(button=>button.onkeydown=e=>{
    const buttons=[...document.querySelectorAll('.tabbtn')].filter(b=>!b.hidden),index=buttons.indexOf(button);
    const direction=['ArrowRight','ArrowDown'].includes(e.key)?1:['ArrowLeft','ArrowUp'].includes(e.key)?-1:0;
    if(!direction&&!['Home','End'].includes(e.key))return;
    e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+direction+buttons.length)%buttons.length;
    buttons[next].click();buttons[next].focus();
  });
  bindModsSubtabs();
  updateControlDecoration();filterControlRows();decoratePanelIcons();
}
function updateControlDecoration(){
  for(const range of document.querySelectorAll('input[type=range]')){
    const fill=100*(Number(range.value)-Number(range.min))/(Number(range.max)-Number(range.min));
    range.style.background=`linear-gradient(to right,var(--color-accent) ${fill}%,var(--color-track) ${fill}%) var(--color-track)`;
    const buttons=range.parentElement.querySelectorAll('.step-button');
    if(buttons.length===2){buttons[0].disabled=+range.value<=+range.min;buttons[1].disabled=+range.value>=+range.max}
  }
  const hero=document.getElementById('densityHero');
  if(hero){
    const density=document.getElementById('den'),value=document.getElementById('denval'),on=document.getElementById('den_on').checked;
    hero.textContent=on?'×'+Number(density.value).toFixed(1):'off';
    if(!value.querySelector('input'))value.textContent=on?'x'+density.value:'off';
  }
}
export function refreshSavedControls(){
  if(!ST?.cfg||document.querySelector('.numedit'))return;
  const c=ST.cfg,map={den:'density',enemyspeed:'enemy_speed',angelic_items:'angelic_items',rarity_rare:'rarity_rare',rarity_ancient:'rarity_ancient'};
  // Switches first: each range's oninput below reads its switch to paint "off".
  paintSwitches(c);
  const painted=[];
  document.querySelectorAll('input[type=range]').forEach(range=>{
    const value=range.dataset.sec?c[range.dataset.sec]?.[range.dataset.key]:c[map[range.id]];
    if(value!==undefined){
      // Rendering saved decimal values must not snap them to the drag step.
      if(!range.dataset.step0)range.dataset.step0=range.step||'1';
      range.step='any';range.value=value;
      painted.push([range,range.dataset.typed]);range.dataset.typed='1';
    }
  });
  for(const [range] of painted)if(range.oninput)range.oninput();
  for(const [range,typed] of painted){if(typed===undefined)delete range.dataset.typed;else range.dataset.typed=typed}
  const booleans={den_on:'density_on',autoapply:'auto_apply',enemyspeed_ct:'enemy_speed_ct',map_reveal:'map_reveal',map_reveal_packs:'map_reveal_packs',map_reveal_spawn:'map_reveal_spawn',headhunter:'headhunter',tyrant:'tyrant',beacon:'beacon',mod_filter_max_relics:'mod_filter_max_relics',mod_orb_pickup_radius:'mod_orb_pickup_radius',mod_pet_quest_pickup:'mod_pet_quest_pickup',mod_auto_prospect:'mod_auto_prospect',mod_auto_prospect_bag:'mod_auto_prospect_bag',mod_toggle_indicator:'mod_toggle_indicator',mod_toggle_guard:'mod_toggle_guard',mod_restart_anytime:'mod_restart_anytime',mod_far_sleep:'mod_far_sleep',mod_craft_mats:'mod_craft_mats',mod_stash_move_all:'mod_stash_move_all',mod_gem_mythic:'mod_gem_mythic',mod_gem_maxroll:'mod_gem_maxroll'};
  for(const [id,key] of Object.entries(booleans))document.getElementById(id).checked=!!c[key];
  document.getElementById('mod_skill_timer_style').value=c.mod_skill_timer_style||'off';
  for(const [id,key] of Object.entries({hhval:'headhunter',tyval:'tyrant',beval:'beacon',mfmrval:'mod_filter_max_relics',morval:'mod_orb_pickup_radius',mpqpval:'mod_pet_quest_pickup',autoprospval:'mod_auto_prospect',mtival:'mod_toggle_indicator',mtgval:'mod_toggle_guard',mraval:'mod_restart_anytime',mfsval:'mod_far_sleep',mcmval:'mod_craft_mats',msmaval:'mod_stash_move_all',mgmval:'mod_gem_mythic',mgrval:'mod_gem_maxroll',mapval:'map_reveal'})){
    const value=document.getElementById(id);value.textContent=c[key]?'on':'off';value.className='val '+(c[key]?'':'off');
  }
  document.getElementById('enemyspeedctval').textContent=c.enemy_speed_ct?'CT only':'all zones';
  document.getElementById('denval').textContent=c.density_on?'x'+c.density:'off';
  document.getElementById('denval').className='val '+(c.density_on?'':'off');
  syncRevealPacks(!!c.map_reveal,!!c.map_reveal_packs,!!c.map_reveal_spawn);
  syncProspectBag(!!c.mod_auto_prospect,!!c.mod_auto_prospect_bag);
  applyPluginModState(ST.pluginMods);
  applyStashMoveAllSession();
  document.getElementById('theme').value=applyTheme(c.theme);
  updateControlDecoration();decoratePanelIcons();
  // Last: the list reads each entry's value from the row just repainted.
  renderEnabledMods(c);
  document.dispatchEvent?.(new Event('forgepact:settings'));
}
export function filterControlRows(){
  if(!document.getElementById('controlSearch'))return;
  const query=document.getElementById('controlSearch').value.trim().toLowerCase();
  document.querySelectorAll('[data-control-filter]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.controlFilter===controlFilter)));
  const filtering=['loot','modifiers'].includes(activeTab);
  let count=0;
  document.querySelectorAll('.setting-entry').forEach(entry=>{
    const range=entry.querySelector('input[type=range]');
    const matches=(entry.textContent.toLowerCase().includes(query))&&(controlFilter==='all'||!sliderOff(range.dataset.sec,Number(range.value)));
    entry.hidden=filtering&&entry.closest('.tab-card').dataset.tab===activeTab&&!matches;
    if(entry.closest('.tab-card').dataset.tab===activeTab&&!entry.hidden)count++;
  });
  document.querySelectorAll('.modifier-group').forEach(group=>{group.hidden=filtering&&!group.querySelector('.setting-entry:not([hidden])')});
  // Angelic drops use a standalone card but participate in the same Loot filter.
  const angelic=document.getElementById('angelicCard');
  if(angelic){
    angelic.hidden=activeTab==='loot'&&(!angelic.textContent.toLowerCase().includes(query)||(controlFilter==='modified'&&+document.getElementById('angelic_items').value<=1));
    if(activeTab==='loot'&&!angelic.hidden)count++;
  }
  // So does the Gems of Incarnation card: by its own rows' text (not the mod
  // filter's list), and under Modified while either switch is on or the saved
  // filter narrows the mods.
  const gems=document.getElementById('gemsCard');
  if(gems){
    const text=[...gems.children].filter(el=>el.id!=='gemfilter_panel').map(el=>el.textContent).join(' ').toLowerCase();
    const modified=document.getElementById('mod_gem_mythic').checked||document.getElementById('mod_gem_maxroll').checked||Array.isArray(ST?.cfg?.gem_filter);
    gems.hidden=activeTab==='loot'&&(!text.includes(query)||(controlFilter==='modified'&&!modified));
    if(activeTab==='loot'&&!gems.hidden)count++;
  }
  let empty=document.getElementById('emptySettings');
  if(!empty){empty=document.createElement('div');empty.id='emptySettings';empty.className='empty-settings';empty.textContent='No matching settings. Try another search or show all settings.';document.getElementById('workspace').append(empty)}
  empty.hidden=!filtering||count>0;
}
// These controls edit the allowed pool, never the game's roll or its minimums.
// Keep rows in place while saving so keyboard focus and list scroll do not jump.
const SAT_UI={filter:'all',busy:false};
function satData(polarity){
  return polarity==='buff'
    ? {list:ST.satanicBuffs||[],floor:ST.minEnabledSatanicBuffs||3}
    : {list:ST.satanicDebuffs||[],floor:ST.minEnabledSatanicDebuffs||2};
}
function satPool(polarity){return ST.cfg.satanic_mods?.[polarity]||{}}
function satCount(polarity){return satData(polarity).list.filter(([id])=>satPool(polarity)[id]!==false).length}
function renderSatanicMods(){
  for(const polarity of ['buff','debuff']){
    const list=document.getElementById(`sat${polarity}s`);
    list.innerHTML=satData(polarity).list.map(([id,name,desc])=>satRow(polarity,id,name,desc,satPool(polarity)[id]!==false)).join('')+
      '<p class="sat-empty" hidden>No matching modifiers.<br>Try another search or filter.</p>';
    list.querySelectorAll('.sat-option').forEach(row=>{row.dataset.search=row.textContent.toLocaleLowerCase()});
  }
  syncSatanicMods();
}
function filterSatanicMods(){
  const query=document.getElementById('satSearch').value.trim().toLocaleLowerCase();
  for(const polarity of ['buff','debuff']){
    const list=document.getElementById(`sat${polarity}s`);
    let visible=0;
    list.querySelectorAll('.sat-option').forEach(row=>{
      const enabled=row.querySelector('input').checked;
      const match=row.dataset.search.includes(query)&&(SAT_UI.filter==='all'||(SAT_UI.filter==='enabled')===enabled);
      row.hidden=!match;
      if(match)visible++;
    });
    const empty=list.querySelector('.sat-empty');
    empty.hidden=visible>0;
    if(!satData(polarity).list.length)empty.textContent='Modifier data unavailable. Restart with the complete ForgePact package.';
  }
  document.querySelectorAll('[data-sat-filter]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.satFilter===SAT_UI.filter)));
}
function syncSatanicMods(){
  let valid=true,allEnabled=true,available=true;
  document.getElementById('satanicMods').setAttribute('aria-busy',String(SAT_UI.busy));
  for(const polarity of ['buff','debuff']){
    const {list,floor}=satData(polarity),count=satCount(polarity),pool=satPool(polarity);
    valid=valid&&count>=floor;
    available=available&&list.length>0;
    allEnabled=allEnabled&&count===list.length;
    document.getElementById(`sat${polarity}Count`).textContent=`${count} enabled`;
    document.getElementById(`sat${polarity}Min`).textContent=floor;
    document.getElementById(`sat${polarity}All`).disabled=SAT_UI.busy||!list.length||count===list.length;
    document.querySelectorAll(`input[data-sat-polarity="${polarity}"]`).forEach(box=>{
      box.checked=pool[box.dataset.satId]!==false;
      const locked=box.checked&&count<=floor;
      box.setAttribute('aria-disabled',String(SAT_UI.busy||locked));
      const row=box.closest('.sat-option');
      row.classList.toggle('is-enabled',box.checked);
      row.classList.toggle('is-locked',locked);
      row.title=locked?'Enable another modifier before removing this one.':'';
    });
    const help=document.getElementById(`sat${polarity}Help`);
    if(!list.length)help.innerHTML='<strong>Modifier data unavailable</strong>Selections cannot be edited.';
    else if(count<floor)help.innerHTML=`<strong>Enable ${floor-count} more to continue</strong>At least ${floor} modifiers must stay enabled.`;
    else if(count===floor)help.innerHTML='<strong>Minimum reached</strong>Enable another mod before removing one.';
    else help.innerHTML=`<strong>${count} of ${list.length} enabled</strong>You can disable ${count-floor} more. Keep at least ${floor}.`;
  }
  document.getElementById('satRestore').disabled=SAT_UI.busy||!available||allEnabled;
  const summary=document.getElementById('satSummary');
  summary.classList.toggle('is-invalid',!valid);
  // A valid selection carries app.css's drawn check (finish review F6), not a glyph.
  summary.innerHTML=`${valid?'Selection valid':available?'Selection incomplete':'Modifier data unavailable'} <span class="sat-footer-note">${satCount('buff')} positive &middot; ${satCount('debuff')} negative enabled</span>`;
  filterSatanicMods();
}
async function saveSatanicMods(changes){
  if(SAT_UI.busy)return;
  SAT_UI.busy=true;
  const focused=document.activeElement,saveState=document.getElementById('satSaveState');
  saveState.classList.remove('is-error');
  saveState.textContent='Saving changes...';
  syncSatanicMods();
  try{
    // Bulk actions are sequential, not one request per row. Restore defaults
    // uses the existing API for each pool and preserves every unrelated setting.
    for(const change of changes){
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:'satanic_mods',...change})});
      if(res.err||!res.cfg)throw new Error(res.err||'The server did not confirm the save.');
      ST.cfg.satanic_mods=res.cfg.satanic_mods;
    }
    saveState.textContent='Saved · Changes save automatically';
  }catch(e){
    // A lost response may still have saved. Read back the real settings before
    // allowing another edit; never leave a speculative checkmark in the UI.
    let confirmed=false;
    try{const state=await j('/api/state');if(state.cfg){ST.cfg.satanic_mods=state.cfg.satanic_mods;confirmed=true}}catch(_){}
    saveState.classList.add('is-error');
    saveState.textContent=confirmed?'Save interrupted. Showing saved selections.':'Save unconfirmed. Reconnect and reload.';
    toast('Could not finish saving: '+e.message);
  }finally{
    SAT_UI.busy=false;
    syncSatanicMods();
    if(document.activeElement===document.body&&focused?.isConnected&&!focused.disabled)focused.focus({preventScroll:true});
  }
}
function bindSatanicMods(){
  document.getElementById('satSearch').oninput=filterSatanicMods;
  document.querySelectorAll('[data-sat-filter]').forEach(b=>{
    b.onclick=()=>{SAT_UI.filter=b.dataset.satFilter;filterSatanicMods()};
  });
  document.querySelectorAll('input[data-sat-polarity]').forEach(box=>{
    // aria-disabled keeps minimum-locked checkboxes reachable by keyboard so
    // their name, effect and the explanation can still be read together.
    box.onclick=e=>{
      if(box.getAttribute('aria-disabled')==='true'){
        e.preventDefault();
        if(!SAT_UI.busy)toast('Enable another modifier before removing this one.');
      }
    };
    box.onchange=()=>{
      if(SAT_UI.busy){syncSatanicMods();return}
      const polarity=box.dataset.satPolarity;
      if(!box.checked&&satCount(polarity)<=satData(polarity).floor){syncSatanicMods();return}
      saveSatanicMods([{polarity,key:box.dataset.satId,value:box.checked}]);
    };
  });
  const enableAll=polarity=>({polarity,keys:satData(polarity).list.map(m=>String(m[0])),value:true});
  for(const polarity of ['buff','debuff'])document.getElementById(`sat${polarity}All`).onclick=()=>saveSatanicMods([enableAll(polarity)]);
  document.getElementById('satRestore').onclick=()=>saveSatanicMods(['buff','debuff'].map(enableAll));
}
// Self-scheduling poll: fast while something is happening, idle when nothing
// is, suspended entirely while the window is hidden.
let pollTimer=null, pollLastChange=Date.now(), pollPrev=null;
function schedulePoll(){
  if(pollTimer){clearTimeout(pollTimer);pollTimer=null}
  const delay=pollDelayMs(document.hidden,Date.now()-pollLastChange);
  if(delay===null)return;
  pollTimer=setTimeout(pollOnce,delay);
}
function noteLocalAction(){pollLastChange=Date.now();schedulePoll()}
// One re-read after a switch is sent, so the toast reports the plugin's own
// answer. The plugin writes its state on its next frame batch, so give it a
// moment; a panel with no game running just gets the empty state back.
async function pluginModsAfterSet(){
  await new Promise(r=>setTimeout(r,700));
  try{const s=await j("/api/state");if(ST)ST.pluginMods=s.pluginMods;applyPluginModState(s.pluginMods);return s.pluginMods}catch(_){return null}
}
async function pollOnce(){
  pollTimer=null;
  try{
    const s=await j('/api/state');
    pollLastChange=pollNextChangeAt(pollPrev,s,false,Date.now(),pollLastChange);
    pollPrev=s;
    if(ST){ST.gameRunning=s.gameRunning;ST.lastApplied=s.lastApplied;ST.queued=s.queued;ST.ipcOk=s.ipcOk;ST.chain=s.chain;ST.eacStatus=s.eacStatus;ST.launch=s.launch;ST.pluginMods=s.pluginMods;ST.stash_move_all_session=s.stash_move_all_session;status();applyPluginModState(s.pluginMods);applyStashMoveAllSession();document.dispatchEvent?.(new Event('forgepact:status'))}
  }catch(e){}
  schedulePoll();
}
export function start(){
  // One listener instead of a call in every handler: any control the user
  // touches is a local action, and so is pressing Apply.
  ['input','change','click'].forEach(ev=>document.addEventListener(ev,noteLocalAction,true));
  document.addEventListener('visibilitychange',()=>{
    // Coming back: poll at once, so the first thing a returning user sees is
    // fresh, and reset the clock so the fast tier covers the time they look.
    if(document.hidden){schedulePoll()}else{pollLastChange=Date.now();pollOnce()}
  });
  // finally, not then: boot() does ~40 unguarded DOM lookups after its first
  // await, and if any of them throws, a .then() never runs - so the panel would
  // sit there forever with no poll scheduled and no message, every chip frozen on
  // its initial value. The fixed setInterval this replaced was registered
  // unconditionally and could not fail that way, so .then() alone was a
  // regression. Say so in the UI as well: a panel that stops updating silently is
  // the thing a user cannot report.
  boot().catch(e=>{try{toast('panel failed to load: '+e)}catch(_){}})
        .finally(()=>{pollPrev=ST;schedulePoll()});
}
