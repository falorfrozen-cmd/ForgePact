// Use PanelSandbox only. All launch requests are intercepted: never starts a game.
(async()=>{
  const fetchBefore=window.fetch,saved=structuredClone(ST),passed=[];
  const el=id=>document.getElementById(id),wait=ms=>new Promise(r=>setTimeout(r,ms));
  const assert=(ok,message)=>{if(!ok)throw new Error(message)};
  let count=0,reply={err:'Steam was not found',launch:{phase:'error',message:'Steam was not found',pid:0}};
  window.fetch=async(url,opt)=>{
    if(url==='/api/launch'){
      count++;await wait(180);
      return new Response(JSON.stringify(reply),{headers:{'Content-Type':'application/json'}});
    }
    if(url==='/api/state'){
      const response=await fetchBefore(url,opt),state=await response.json();
      return new Response(JSON.stringify({...state,launch:count?reply.launch:state.launch}),{headers:{'Content-Type':'application/json'}});
    }
    return fetchBefore(url,opt);
  };
  try{
    clearTimeout(pollTimer);openTab('setup',false);
    ST.gameRunning=false;ST.launch={phase:'idle',message:'Ready'};renderLaunchStatus();
    const button=el('launchgame');button.click();button.click();
    assert(button.disabled,'Launch must be disabled while waiting for Steam');
    await wait(300);
    assert(count===1,'Double click must send exactly one request');
    assert(!button.disabled,'Failed launch must permit a retry');
    assert(el('launchFeedback').textContent==='Steam was not found','Persistent failure must be visible');
    await pollOnce();clearTimeout(pollTimer);
    assert(el('launchFeedback').textContent==='Steam was not found','Poll must retain server failure');
    passed.push('double-click guard, visible error, retry, poll consistency');
    reply={ok:'Offline launch requested',launch:{phase:'started',message:'Offline launch requested',pid:1234}};
    button.click();await wait(300);
    assert(count===2&&el('launchFeedback').textContent==='Offline launch requested','Successful response');
    reply.launch={phase:'verified',message:'Game process verified; EAC is inactive.',pid:1234};
    await pollOnce();clearTimeout(pollTimer);
    assert(el('launchFeedback').textContent===reply.launch.message,'One-shot verification result must reach the UI');
    ST.gameRunning=true;status();assert(button.disabled,'Already-running game must disable Launch');
    passed.push('launch response, verification result, already-running state');
    return {passed};
  }finally{
    window.fetch=fetchBefore;ST=saved;launcherBusy=false;status();schedulePoll();
  }
})();
