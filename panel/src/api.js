// The panel's one road to the server. j() GETs as fetch does, and queues
// every POST behind the previous one while #saveIndicator says what is
// happening (the browser tools wait on that indicator, never on a global).
// Once the queue drains it repaints the controls from the settings the server
// answered with, so a failed or lost write never leaves a control showing a
// value that was not saved.
import { ST } from './state.svelte.js';
import { filterControlRows, refreshSavedControls } from './panel.js';

// Serialize panel writes because each server request saves the whole config.
let writeQueue=Promise.resolve();
export let pendingWrites=0;
export async function j(u,opt){
  if(!opt||opt.method!=='POST'){const r=await fetch(u,opt);return r.json()}
  pendingWrites++;
  const indicator=document.getElementById('saveIndicator');
  indicator.textContent='Saving...';indicator.className='saving';
  const run=async()=>{
    try{
      const r=await fetch(u,opt),result=await r.json();
      if(!r.ok&&!result.err)result.err='Request failed ('+r.status+')';
      if(result.cfg&&ST)ST.cfg=result.cfg;
      indicator.textContent=result.err?'Could not save':u==='/api/set'?'Saved':'Request completed';
      indicator.className=result.err?'error':'';
      return result;
    }catch(e){
      // A disconnected response may still have committed the settings. Read
      // them back before painting controls instead of assuming the write failed.
      if(u==='/api/set'){
        try{const r=await fetch('/api/state');if(r.ok){const state=await r.json();if(state.cfg&&ST)ST.cfg=state.cfg}}catch(_){}
      }
      indicator.textContent='Connection lost · retry';indicator.className='error';
      return {err:'Could not reach ForgePact. Reconnect and try again.'};
    }finally{
      pendingWrites--;
      if(pendingWrites){indicator.textContent='Saving...';indicator.className='saving'}
      setTimeout(()=>{if(!pendingWrites){refreshSavedControls();filterControlRows()}},0);
    }
  };
  const request=writeQueue.then(run,run);
  writeQueue=request.catch(()=>{});
  return request;
}
