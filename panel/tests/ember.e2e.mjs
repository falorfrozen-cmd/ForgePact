// Ember's shortcuts drive the real 2.0 controls and API in a throwaway sandbox.
// No game process, saves or user configuration are touched.
import assert from 'node:assert/strict';
import { mkdirSync } from 'node:fs';
import { startSandbox, launchBrowser, openPanel, openTab, waitSaved } from './lib/browser.mjs';
const sandbox=await startSandbox({offline:true});
const browser=await launchBrowser();
const errors=[];
const checks=[];
mkdirSync('artifacts/ember',{recursive:true});
try {
  const page=await openPanel(browser,sandbox,{width:1600,height:1000},{routes:async page=>{
    page.on('pageerror',error=>errors.push(error.message));
    page.on('response',res=>{if(res.status()>=400&&!res.url().endsWith('/favicon.ico'))errors.push(`${res.status()} ${res.url()}`)});
  }});
  page.setDefaultTimeout(10000);
  async function saved(action) {
    await Promise.all([page.waitForResponse(r=>r.request().method()==='POST'&&r.url().endsWith('/api/set')),action()]);
    await waitSaved(page);
  }
  async function agrees(selector,value) {
    await page.waitForFunction(({selector,value})=>document.querySelector(selector)?.value===String(value),{selector,value});
  }
  assert.equal(await page.locator('html').getAttribute('data-theme'),'ember');
  // An idle visual toast still needs to exist in the accessibility tree
  // before its text changes, otherwise polite announcements can be lost.
  const cdp = await page.context().newCDPSession(page);
  const documentNode = await cdp.send('DOM.getDocument');
  const toastNode = await cdp.send('DOM.querySelector', {nodeId: documentNode.root.nodeId, selector: '#toast'});
  const toastTree = await cdp.send('Accessibility.getPartialAXTree', {nodeId: toastNode.nodeId, fetchRelatives: false});
  assert.ok(toastTree.nodes.some(node => !node.ignored && node.role?.value === 'status'), 'Idle toast must remain an accessible live region');
  await cdp.detach();
  await openTab(page,'overview');
  assert.equal(await page.locator('[data-quick-row]').count(),3);
  await saved(async()=>{await page.locator('#quick-number-magicfind').fill('5.25');await page.locator('#quick-number-magicfind').press('Tab')});
  assert.equal((await sandbox.state()).cfg.stats.magicfind,5.25);
  await saved(()=>page.locator('[data-quick-enable="magicfind"]').uncheck());
  let cfg=(await sandbox.state()).cfg;
  assert.equal(cfg.stats.magicfind,5.25);
  assert.equal(cfg.switches['stats.magicfind'],false);
  await openTab(page,'modifiers');
  assert.equal(await page.locator('#sw_stats_magicfind').isChecked(),false);
  await agrees('input[data-sec="stats"][data-key="magicfind"]',5.25);
  await saved(()=>page.locator('#sw_stats_magicfind').check());
  await openTab(page,'overview');
  assert.equal(await page.locator('[data-quick-enable="magicfind"]').isChecked(),true);
  await agrees('#quick-number-magicfind',5.25);
  checks.push('Quick controls preserve decimals and disabled values; both directions stay synchronized');

  await page.route('**/api/set',route=>route.fulfill({status:200,contentType:'application/json',body:JSON.stringify({err:'Sandbox rejection'})}),{times:1});
  await saved(async()=>{await page.locator('#quick-number-magicfind').fill('9');await page.locator('#quick-number-magicfind').press('Tab')});
  await agrees('#quick-number-magicfind',5.25);
  assert.equal((await sandbox.state()).cfg.stats.magicfind,5.25);
  assert.equal(await page.locator('#saveIndicator').textContent(),'Could not save');
  await saved(()=>page.locator('#enabledMods .quick-disable[data-for="sw_stats_magicfind"]').click());
  assert.equal((await sandbox.state()).cfg.switches['stats.magicfind'],false);
  await saved(()=>page.locator('.undo-toast-button').click());
  // The backend omits an explicit true because absence means enabled.
  assert.notEqual((await sandbox.state()).cfg.switches['stats.magicfind'],false);
  await page.waitForFunction(()=>document.querySelector('[data-quick-enable="magicfind"]')?.checked===true);
  checks.push('Rejected shortcut saves roll back; Enabled mods Turn off and Undo synchronize Overview');

  await saved(()=>page.locator('[data-quick-enable="density"]').check());
  assert.equal((await sandbox.state()).cfg.density_on,true);
  await saved(()=>page.locator('#quick-map').check());
  assert.equal(await page.locator('#quick-packs').isEnabled(),true);
  if(await page.locator('#quick-packs').isChecked()) await saved(()=>page.locator('#quick-packs').uncheck());
  await saved(()=>page.locator('#quick-packs').check());
  assert.equal((await sandbox.state()).cfg.map_reveal_packs,true);
  await saved(()=>page.locator('#quick-map').uncheck());
  assert.equal(await page.locator('#quick-packs').isDisabled(),true);
  checks.push('Density and map/pack dependent switches use native handlers');

  // The hidden-loot key is the one select that can be disabled: off, it dims to
  // 45%, refuses the pointer and keeps its fill on hover. On is the control.
  async function selectLook(id) {
    const select=page.locator('#'+id);
    await select.scrollIntoViewIfNeeded();
    const read=()=>select.evaluate(async el=>{
      await Promise.all(el.getAnimations().map(a=>a.finished));
      const s=getComputedStyle(el);
      return {disabled:el.disabled,opacity:s.opacity,cursor:s.cursor,bg:s.backgroundColor};
    });
    await page.mouse.move(1,1);
    const idle=await read();
    const box=await select.boundingBox();
    await page.mouse.move(box.x+box.width/2,box.y+box.height/2);
    await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
    const hover=await read();
    await page.mouse.move(1,1);
    return {...idle,hoverBg:hover.bg};
  }
  await openTab(page,'mods');
  if(await page.locator('#subtab-qol').isVisible()) await page.locator('#subtab-qol').click();
  const keyOff=await selectLook('mod_hidden_loot_key');
  assert.deepEqual({disabled:keyOff.disabled,opacity:keyOff.opacity,cursor:keyOff.cursor,hoverBg:keyOff.hoverBg},
    {disabled:true,opacity:'0.45',cursor:'not-allowed',hoverBg:keyOff.bg});
  await saved(()=>page.locator('#mod_hidden_loot').check());
  const keyOn=await selectLook('mod_hidden_loot_key');
  assert.deepEqual({disabled:keyOn.disabled,opacity:keyOn.opacity,cursor:keyOn.cursor},{disabled:false,opacity:'1',cursor:'pointer'});
  assert.notEqual(keyOn.hoverBg,keyOn.bg,'Hovering the enabled key select (control) must change its fill');
  await saved(()=>page.locator('#mod_hidden_loot').uncheck());
  await openTab(page,'overview');
  checks.push('The disabled hidden-loot key select dims to 45%, refuses the pointer and keeps its fill on hover');

  await page.locator('#customizeQuick').click();
  await page.locator('#emberQuickChoices input[value="movespeed"]').check();
  await page.locator('#saveQuickChoices').click();
  assert.equal(await page.locator('#emberQuickDialog').evaluate(el=>el.open),true);
  await page.locator('#emberQuickChoices input[value="density"]').uncheck();
  await page.locator('#saveQuickChoices').click();
  assert.equal(await page.locator('[data-quick-row="movespeed"]').count(),1);
  await page.reload();
  await page.waitForFunction(()=>document.getElementById('saveIndicator')?.textContent==='Settings loaded');
  assert.equal(await page.locator('[data-quick-row="movespeed"]').count(),1);
  checks.push('Favorites enforce three controls and survive reload');

  await page.keyboard.press('Control+k');
  await page.locator('#emberSearchInput').fill('Prime Evil');
  assert.ok(await page.locator('#emberSearchResults button').count()>0);
  await page.locator('#emberSearchResults button').first().click();
  assert.equal(await page.locator('#nav-loot').getAttribute('aria-selected'),'true');
  await page.keyboard.press('Control+k');
  await page.locator('#emberSearchInput').fill('incarnation');
  await page.locator('#emberSearchResults button').first().click();
  assert.equal(await page.locator('#nav-loot').getAttribute('aria-selected'),'true');
  checks.push('Global search reaches new Prime Evil and Gems of Incarnation controls');

  const missingBefore = [
    ['map_reveal', 'qolCard'], ['map_reveal_packs', 'qolCard'], ['map_reveal_spawn', 'qolCard'],
    ['mod_pet_quest_pickup', 'qolCard'], ['mod_pet_relic_pickup', 'qolCard'], ['mod_auto_prospect', 'qolCard'], ['mod_auto_prospect_bag', 'qolCard'],
    ['mod_craft_mats', 'qolCard'], ['mod_stash_move_all', 'qolCard'], ['mod_toggle_indicator', 'qolCard'], ['mod_toggle_guard', 'qolCard'],
    ['mod_restart_anytime', 'qolCard'], ['mod_far_sleep', 'qolCard'], ['density_rolling', 'qolCard'], ['mod_hidden_loot', 'qolCard'], ['mod_hidden_loot_key', 'qolCard'], ['mod_jump_scenery', 'qolCard'], ['mod_orb_pickup_radius', 'qolCard'], ['mod_filter_max_relics', 'qolCard'],
    ['mod_skill_timer_style', 'qolCard'], ['headhunter', 'itemsCard'], ['tyrant', 'itemsCard'], ['beacon', 'itemsCard'], ['boss_rarity', 'gameplayCard'],
    ['mod_gem_mythic', 'gemsCard'], ['mod_gem_maxroll', 'gemsCard'], ['den_on', 'densityCard'], ['enemyspeed_ct', 'speedCard'],
  ];
  for (const [id, card] of missingBefore) {
    await page.keyboard.press('Control+k');
    await page.locator('#emberSearchInput').fill(id.replaceAll('_', ' '));
    // IDs provide an unambiguous query while the result itself uses the UI label.
    const label = await page.locator('#'+id).evaluate(el => el.id === 'den_on' ? 'Enable monster density'
      : el.closest('.row').querySelector('.label-copy,.lbl').textContent.trim().replace(/\s+/g,' '));
    await page.locator('#emberSearchResults button').filter({has: page.locator('span', {hasText: label})}).first().click();
    assert.equal(await page.locator('#'+card).evaluate(el => el.checkVisibility()), true, id);
    await page.waitForFunction(id => {
      const el = document.getElementById(id);
      return document.activeElement === (el.disabled ? el.closest('.row') : el);
    }, id);
    assert.equal(await page.locator('#'+id).evaluate(el => document.activeElement === (el.disabled ? el.closest('.row') : el)), true,
      id + ': focus=' + await page.evaluate(() => document.activeElement.outerHTML.slice(0, 180)));
  }
  // A real search-to-control edit must use the existing backend handler.
  await page.keyboard.press('Control+k');
  await page.locator('#emberSearchInput').fill('Beacon: every monster hunts you');
  assert.equal(await page.locator('#emberSearchCount').textContent(), '1 setting found');
  await page.locator('#emberSearchResults button').click();
  await page.waitForFunction(() => document.activeElement?.id === 'beacon');
  const beaconBefore = (await sandbox.state()).cfg.beacon;
  await saved(() => page.keyboard.press('Space'));
  assert.equal((await sandbox.state()).cfg.beacon, !beaconBefore);
  for (const query of ['map', 'headhunter', 'beacon', 'pet', 'prospect', 'monster density', 'enable monster density']) {
    await page.keyboard.press('Control+k');
    await page.locator('#emberSearchInput').fill(query);
    assert.ok(await page.locator('#emberSearchResults button').count() > 0, query);
    await page.keyboard.press('Escape');
  }
  await page.keyboard.press('Control+k');
  await page.locator('#emberSearchInput').fill('theme');
  await page.locator('#emberSearchResults button').click();
  await page.waitForFunction(() => document.activeElement?.matches('.theme-picker-trigger'));
  assert.equal(await page.locator('.theme-picker-trigger').evaluate(el => document.activeElement === el), true);
  await page.keyboard.press('Control+k');
  await page.locator('#emberSearchInput').fill('Satanic Zone Mods');
  await page.locator('#emberSearchResults button').click();
  await page.waitForFunction(() => document.activeElement?.id === 'satSearch');
  assert.equal(await page.locator('#satSearch').evaluate(el => el.tabIndex), 0, 'Standalone search control must stay in the Tab order');
  await page.keyboard.press('Shift+Tab');
  await page.keyboard.press('Tab');
  assert.equal(await page.evaluate(() => document.activeElement?.id), 'satSearch');
  checks.push('Global search finds all toggles and selects, focuses the real destination, and preserves its API handler');

  for(const theme of ['ledger','graphite','sigil','ember']) {
    await openTab(page,'setup');
    await page.locator('.theme-picker-trigger').click();
    await saved(()=>page.locator(`.theme-picker-option[data-value="${theme}"]`).click());
    assert.equal(await page.locator('html').getAttribute('data-theme'),theme);
    await page.locator('#applyall').click({trial:true});
    if(theme!=='ember') assert.equal(await page.locator('#nav-overview').isVisible(),false);
    await openTab(page,'mods');
    await page.screenshot({path:`artifacts/ember/theme-${theme}.png`});
  }
  checks.push('All four themes retain accessible navigation, actions and controls');

  for(const width of [1600,1280,900,390]) {
    await page.setViewportSize({width,height:width===1600?1000:850});
    for(const tab of ['overview','modifiers','world','loot','mods','setup','help']) {
      await openTab(page,tab);
      const size=await page.evaluate(()=>({page:document.documentElement.scrollWidth,viewport:innerWidth}));
      assert.ok(size.page<=size.viewport+1,`${width} ${tab}: horizontal overflow ${size.page}`);
      await page.locator('#applyall').click({trial:true});
      if(tab==='overview'||width===1280) await page.screenshot({path:`artifacts/ember/${width}-${tab}.png`});
    }
  }
  checks.push('All seven pages fit at 1600, 1280, 900 and 390 px; Apply remains reachable');
  assert.deepEqual(errors,[]);
  checks.push('No JavaScript errors or missing application assets');
  console.log(JSON.stringify({passed:checks.length,checks},null,2));
} finally {await browser.close();await sandbox.stop();}
