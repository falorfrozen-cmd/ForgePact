// Run against PanelSandbox only; no game launch or installation is performed.
(async()=>{
  const saved=structuredClone(ST),fetchBefore=window.fetch;
  const assert=(ok,message)=>{if(!ok)throw new Error(message)};
  const byId=id=>document.getElementById(id);
  const complete={exeExists:true,patched:true,aurieCore:true,yytk:true,plugin:true};
  const passed=[];
  try{
    clearTimeout(pollTimer);
    ST.gameRunning=true;ST.ipcOk=true;
    ST.chain={...complete,patched:false,aurieCore:false,yytk:false,plugin:false};
    status();
    assert(byId('chipGame').textContent==='Game open · plugin missing','Running unmodded game must show missing plugin');
    assert(byId('chipGame').classList.contains('warn'),'Missing plugin must not look connected');
    for(const name of ['setup','modifiers','world','loot','mods']){
      openTab(name,false);
      assert(byId('pluginWarning').getBoundingClientRect().height>0,'Missing warning in '+name);
    }
    byId('pluginWarning').querySelector('button').click();
    assert(activeTab==='setup','Warning must open Setup');
    passed.push('unmodded game with stale IPC folder; warning on all five tabs; Setup shortcut');
    for(const part of ['patched','aurieCore','yytk','plugin']){
      ST.chain={...complete,[part]:false};status();
      assert(!byId('pluginWarning').hidden,'Missing component ignored: '+part);
    }
    passed.push('each required installation component');
    const response={...ST,chain:complete,lastApplied:'12:34:56'};
    window.fetch=async()=>new Response(JSON.stringify(response),{headers:{'Content-Type':'application/json'}});
    await pollOnce();clearTimeout(pollTimer);
    assert(byId('pluginWarning').hidden,'Successful installation must refresh on the existing poll');
    assert(byId('chipGame').textContent==='Game open','File checks must not claim runtime acknowledgement');
    assert(byId('chipApply').textContent.includes('commands sent:'),'Sent commands must not be labelled applied');
    ST.gameRunning=false;status();
    assert(byId('chipGame').textContent==='Game offline','Offline state');
    passed.push('installation refresh, command wording and offline status');
    return {passed};
  }finally{
    window.fetch=fetchBefore;ST=saved;status();openTab('modifiers',false);schedulePoll();
  }
})();
