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
