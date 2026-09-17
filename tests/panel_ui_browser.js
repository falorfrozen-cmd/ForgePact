// Run with agent-browser eval --stdin against PanelSandbox only.
// Setup actions are intercepted; config writes use the real isolated HTTP API.
(async()=>{
  const passed=[],assert=(ok,message)=>{if(!ok)throw new Error(message)};
  const el=s=>document.querySelector(s),all=s=>[...document.querySelectorAll(s)];
  const wait=ms=>new Promise(r=>setTimeout(r,ms));
  const settled=async()=>{const end=Date.now()+5000;while(pendingWrites&&Date.now()<end)await wait(20);await wait(30);assert(!pendingWrites,'Save timeout')};
  const originalFetch=window.fetch;
  const read=async()=> (await (await originalFetch('/api/state')).json()).cfg;
  const baseline=await read(),changes=new Map(),actions=[];
  let failure='',delay=0,inflight=0,peak=0;
  window.fetch=async(url,options)=>{
    if(options?.method!=='POST')return originalFetch(url,options);
    if(url!=='/api/set'){
      actions.push(url);
      return new Response(JSON.stringify({ok:'Test action only',cfg:await read(),path:baseline.game_exe}),{headers:{'Content-Type':'application/json'}});
    }
    const body=JSON.parse(options.body);
    const key=JSON.stringify([body.section||'',body.key]);
    if(!changes.has(key))changes.set(key,{...body,value:body.section?baseline[body.section][body.key]:baseline[body.key]});
    inflight++;peak=Math.max(peak,inflight);
    try{
      if(delay)await wait(delay);
      if(failure==='before')throw new TypeError('Test connection failure');
      const response=await originalFetch(url,options);
      if(failure==='after')throw new TypeError('Test lost response after commit');
      return response;
    }finally{inflight--}
  };
  const tab=name=>el(`[data-tab="${name}"].tabbtn`).click();
  const tap=async selector=>{el(selector).click();await settled()};
  const typeValue=async(selector,value,key='Enter')=>{
    el(selector).click();const input=el(selector+' .numedit');assert(input,'Numeric editor did not open');
    input.value=value;input.dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true}));await settled();
  };
  const search=value=>{el('#controlSearch').value=value;el('#controlSearch').dispatchEvent(new Event('input',{bubbles:true}))};
  try{
    assert(location.hostname==='127.0.0.1'&&!ST.gameRunning&&!baseline.auto_apply,'Use isolated PanelSandbox');
    for(const name of ['setup','modifiers','world','loot','mods']){
      tab(name);
      assert(el(`[data-tab="${name}"].tabbtn`).getAttribute('aria-selected')==='true','Tab state '+name);
      assert(all('.tab-card.active').every(c=>c.dataset.tab===name),'Unrelated visible card '+name);
      assert(el('#pageTitle').textContent===PAGE_INFO[name][0],'Heading '+name);
      assert(document.documentElement.scrollWidth<=innerWidth,'Horizontal overflow '+name);
    }
    passed.push('All five pages, headings, selected states and overflow');

    tab('world');
    await tap('#densityCard .step-button:last-child');
    assert((await read()).density===3.5&&+el('#den').value===3.5,'Density increment');
    await tap('#densityCard .step-button:first-child');
    assert((await read()).density===3,'Density decrement');
    delay=80;el('#densityCard .step-button:last-child').click();el('#densityCard .step-button:last-child').click();
    await settled();delay=0;assert((await read()).density===4,'Rapid increments were dropped');
    await typeValue('#denval','2.25');
    assert((await read()).density===2.25&&+el('#den').value===2.25,'Decimal snapped after save');
    await boot();
    assert(+el('#den').value===2.25&&el('#denval').textContent==='x2.25','Decimal lost on reload');
    await typeValue('#denval','4','Escape');assert((await read()).density===2.25,'Escape saved an edit');
    await typeValue('#denval','900');assert((await read()).density===5&&el('#densityCard .step-button:last-child').disabled,'Upper bound');
    await typeValue('#denval','-10');assert((await read()).density===1&&el('#densityCard .step-button').disabled,'Lower bound');
    passed.push('Increment/decrement, exact decimal persistence, Enter/Escape and bounds');

    tab('modifiers');
    const xp=el('[data-sec="stats"][data-key="exp"]').closest('.row').querySelector('.val');
    xp.id='test-xp-value';await typeValue('#test-xp-value','2.25');
    assert((await read()).stats.exp===2.25,'Stat edit');
    el('[data-control-filter="modified"]').click();
    assert(!xp.closest('.setting-entry').hidden&&el('[data-key="magicfind"]').closest('.setting-entry').hidden,'Modified filter');
    search('nothing-matches-this');assert(!el('#emptySettings').hidden,'Missing empty state');
    search('experience');assert(!xp.closest('.setting-entry').hidden&&el('#emptySettings').hidden,'Name search');
    assert(+el('[data-key="exp"]').value===2.25,'Search changed a decimal');
    tab('loot');assert(el('#controlSearch').value===''&&controlFilter==='all','Tab did not reset filters');
    search('Angelic / Unholy');assert(!el('#angelicCard').hidden,'Angelic search');
    search('nothing-matches-this');assert(el('#angelicCard').hidden&&!el('#emptySettings').hidden,'Angelic excluded from empty state');
    search('');await typeValue('#angelicval','3');
    el('[data-control-filter="modified"]').click();assert(!el('#angelicCard').hidden,'Modified Angelic hidden');
    passed.push('Character and Loot search/Modified filters, empty results, no value mutation');

    tab('mods');
    if(!el('#map_reveal').checked)await tap('#map_reveal');
    assert(!el('#map_reveal_packs').disabled,'Parent did not enable child');
    await tap('#map_reveal');
    assert(el('#map_reveal_packs').disabled&&el('#mrpval').textContent==='n/a','Disabled child state');
    assert(getComputedStyle(el('#map_reveal_packs_row')).opacity==='1','Disabled explanatory text faded');
    el('#nav-world').focus();el('#nav-world').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));
    assert(document.activeElement===el('#nav-loot')&&activeTab==='loot','Keyboard navigation');
    passed.push('Dependent switches, readable disabled text and arrow-key tabs');

    tab('mods');delay=100;
    const hhBefore=el('#headhunter').checked,tyBefore=el('#tyrant').checked;
    el('#headhunter').click();el('#tyrant').click();
    assert(el('#saveIndicator').textContent==='Saving...','No saving state');
    await settled();delay=0;
    const saved=await read();
    assert(saved.headhunter!==hhBefore&&saved.tyrant!==tyBefore&&peak===1,'Overlapping writes or dropped setting');
    failure='before';await tap('#headhunter');failure='';
    assert(el('#headhunter').checked===saved.headhunter&&el('#saveIndicator').classList.contains('error'),'Failure did not roll back');
    failure='after';await tap('#headhunter');failure='';
    assert(el('#headhunter').checked===(await read()).headhunter,'Lost response not reconciled');
    await tap('#headhunter');assert(!el('#saveIndicator').classList.contains('error'),'Retry did not recover');
    passed.push('Sequential cross-setting saves, failure rollback, lost-response recovery and retry');

    tab('setup');
    for(const selector of ['#applyall','#exebrowse','#exesave','#installmod','#removeplugin','#launchgame'])await tap(selector);
    assert(actions.join(',')==='/api/applyall,/api/browseexe,/api/setexe,/api/installmod,/api/removeplugin,/api/launch','Setup action routing');
    assert(!el('#installmod').disabled&&!el('#removeplugin').disabled&&!el('#exebrowse').disabled,'Setup button left locked');
    passed.push('All six Setup/header actions route correctly (intercepted; no game or installation)');
    return {passed,maxConcurrentWrites:peak};
  }finally{
    failure='';delay=0;await settled();
    for(const change of changes.values())await originalFetch('/api/set',{method:'POST',body:JSON.stringify(change)});
    window.fetch=originalFetch;await boot();
  }
})()
