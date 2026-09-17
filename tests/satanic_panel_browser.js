// Run through `agent-browser eval --stdin` on PanelSandbox at its 3/2 minimum.
// Real DOM, real HTTP handler, temporary config; no game connection or watcher.
(async()=>{
  const passed=[];
  const assert=(value,message)=>{if(!value)throw new Error(message)};
  const el=selector=>document.querySelector(selector);
  const box=(polarity,id)=>el(`input[data-sat-polarity="${polarity}"][data-sat-id="${id}"]`);
  const visible=polarity=>document.querySelectorAll(`#sat${polarity}s .sat-option:not([hidden])`).length;
  const settled=async()=>{
    const end=Date.now()+5000;
    while(SAT_UI.busy&&Date.now()<end)await new Promise(r=>setTimeout(r,20));
    assert(!SAT_UI.busy,'Save did not finish');
  };
  const toggle=async(polarity,id)=>{box(polarity,id).closest('label').click();await settled()};
  const search=value=>{el('#satSearch').value=value;el('#satSearch').dispatchEvent(new Event('input',{bubbles:true}))};
  const filter=value=>el(`[data-sat-filter="${value}"]`).click();
  const read=async()=> (await (await originalFetch('/api/state')).json()).cfg;
  const originalFetch=window.fetch;
  let calls=0,inflight=0,peak=0,failMode='',delay=0;
  window.fetch=async(url,options)=>{
    if(url!=='/api/set')return originalFetch(url,options);
    calls++;inflight++;peak=Math.max(peak,inflight);
    try{
      if(delay)await new Promise(r=>setTimeout(r,delay));
      if(failMode==='before')throw new TypeError('Simulated network interruption');
      const response=await originalFetch(url,options);
      if(failMode==='after')throw new TypeError('Simulated lost response after save');
      return response;
    }finally{inflight--}
  };
  try{
    assert(satCount('buff')===3&&satCount('debuff')===2,'Use a fresh minimum fixture');
    assert(visible('buff')===25&&visible('debuff')===26,'Missing SDK rows');
    const baseline=await read();
    await toggle('buff','1');await toggle('debuff','1');
    assert(calls===0&&box('buff','1').checked&&box('debuff','1').checked,'Minimum was not protected');
    passed.push('3/2 minimum blocks mouse changes before HTTP');

    await toggle('buff','4');await toggle('buff','1');
    const saved=await read();
    assert(saved.satanic_mods.buff['4']&&!saved.satanic_mods.buff['1'],'Replacement did not persist');
    assert(satCount('buff')===3&&satCount('debuff')===2,'Counts did not update');
    await boot();
    assert(box('buff','4').checked&&!box('buff','1').checked,'Reload reset saved choices');
    passed.push('Replacement, live counts and reloaded saved selections');

    search('Rune Master');assert(visible('buff')===1&&visible('debuff')===0,'Name search');
    search('life decreased');assert(visible('buff')===0&&visible('debuff')===1,'Effect search');
    search('no such modifier');assert(visible('buff')===0&&!el('#satbuffs .sat-empty').hidden,'Empty state');
    search('');filter('enabled');assert(visible('buff')===3&&visible('debuff')===2,'Enabled filter');
    filter('disabled');assert(visible('buff')===22&&visible('debuff')===24,'Disabled filter');
    assert(el('#satbuffCount').textContent==='3 enabled','Filtered count must include all enabled mods');
    passed.push('Name/effect search, both filters and empty states');

    const beforeBulk=calls;
    el('#satbuffAll').click();await settled();
    assert(calls===beforeBulk+1&&satCount('buff')===25&&satCount('debuff')===2,'Enable all must be a single pool request');
    assert(visible('buff')===0&&!el('#satbuffs .sat-empty').hidden,'Disabled filter not refreshed');
    el('#satRestore').click();await settled();
    assert(satCount('buff')===25&&satCount('debuff')===26,'Restore defaults');
    const restored=await read();
    for(const [key,value] of Object.entries(baseline))if(key!=='satanic_mods')assert(JSON.stringify(restored[key])===JSON.stringify(value),'Unrelated setting changed: '+key);
    assert(peak===1,'Bulk requests overlapped');
    passed.push('Enable all, restore defaults, untouched unrelated preferences');

    filter('all');delay=150;
    const beforeRapid=calls;
    box('buff','1').closest('label').click();box('buff','2').closest('label').click();el('#satRestore').click();
    await settled();delay=0;
    assert(calls===beforeRapid+1&&!box('buff','1').checked&&box('buff','2').checked,'Rapid edits were not locked');
    passed.push('Pending save prevents duplicate/concurrent edits');

    failMode='before';await toggle('buff','1');failMode='';
    assert(!box('buff','1').checked&&el('#satSaveState').classList.contains('is-error'),'Network failure left an unsaved checkbox');
    failMode='after';await toggle('buff','1');failMode='';
    assert(box('buff','1').checked,'Lost response did not reconcile the saved config');
    await toggle('buff','1');
    assert(!box('buff','1').checked&&!el('#satSaveState').classList.contains('is-error'),'Retry did not recover');
    passed.push('Network rollback, lost-response reconciliation, successful retry');

    // Restore the isolated fixture for keyboard and screenshot verification.
    for(const polarity of ['buff','debuff']){
      for(const [id] of satData(polarity).list){
        if(satPool(polarity)[id]===false)await toggle(polarity,String(id));
      }
      for(const [id] of satData(polarity).list.slice(satData(polarity).floor))await toggle(polarity,String(id));
    }
    assert(satCount('buff')===3&&satCount('debuff')===2,'Fixture not restored');
    return {passed,requests:calls,maxConcurrentRequests:peak};
  }finally{window.fetch=originalFetch}
})()
