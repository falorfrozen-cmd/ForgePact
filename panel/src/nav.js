// Tab and Mods sub-tab navigation. Plain JS with the old page's function
// bodies unchanged: tests/test_mods_categories.py slices openTab,
// openModsSubtab, bindModsSubtabs, PAGE_INFO and the state line out of this
// file and runs them through node against a stub DOM, so keep each one
// self-contained. The state is module-local; other modules read it through
// the live exports and change it only through the two setters below.
import { filterControlRows } from './panel.js';

export const PAGE_INFO={
  overview:['Your forge. Your rules.','Shape your offline adventure.'],
  help:['Your offline workshop','Get the most out of ForgePact.'],
  setup:['Game setup','Connect your offline game and manage the mod plugin.'],
  modifiers:['Character modifiers','Tune your character and combat bonuses.'],
  world:['World settings','Shape your zones. Keep every choice in sight.'],
  loot:['Loot settings','Adjust drop rates and see exactly what each multiplier changes.'],
  mods:['Mods','Choose the features you want for your offline adventure.']
};
export let activeTab='modifiers',controlFilter='all',modsSubtab='qolCard';
export function setControlFilter(value){controlFilter=value}
export function setModsSubtab(value){modsSubtab=value}
export function openTab(name,remember=true){
  const targetTab=document.querySelector(`.tabbtn[data-tab="${name}"]`);
  if(!targetTab||targetTab.hidden)name='modifiers';
  activeTab=name;
  document.querySelectorAll('.tabbtn').forEach(b=>{const on=b.dataset.tab===name;b.classList.toggle('active',on);b.setAttribute('aria-selected',on?'true':'false');b.tabIndex=on?0:-1});
  document.querySelectorAll('.tab-card').forEach(c=>{c.hidden=false;c.classList.toggle('active',c.dataset.tab===name)});
  document.getElementById('modsSubtabs').hidden=name!=='mods';
  if(name==='mods')openModsSubtab(modsSubtab,false);
  document.getElementById('workspace').setAttribute('aria-labelledby','nav-'+name);
  document.getElementById('pageTitle').textContent=PAGE_INFO[name][0];
  document.getElementById('pageDescription').textContent=PAGE_INFO[name][1];
  // Named as its nav button reads: Ember relabels Modifiers to Character.
  const navLabel=document.querySelector(`.tabbtn[data-tab="${name}"]`)?.textContent?.trim();
  document.getElementById('breadcrumbPage').textContent=navLabel||name[0].toUpperCase()+name.slice(1);
  document.getElementById('controlToolbar').hidden=!['loot','modifiers'].includes(name);
  document.getElementById('controlSearch').value='';controlFilter='all';filterControlRows();
  if(remember){try{sessionStorage.setItem('forgepact_tab',name)}catch(e){}}
  window.scrollTo({top:0,behavior:'instant'});
  document.getElementById('wrap')?.scrollTo?.({top:0,behavior:'instant'});
  document.dispatchEvent?.(new Event('forgepact:navigate'));
}
export function openModsSubtab(id,remember=true){
  const buttons=[...document.querySelectorAll('.subtabbtn')];
  const target=buttons.some(b=>b.getAttribute('aria-controls')===id)?id:buttons[0].getAttribute('aria-controls');
  modsSubtab=target;
  buttons.forEach(b=>{
    const on=b.getAttribute('aria-controls')===target;
    b.classList.toggle('active',on);b.setAttribute('aria-selected',on?'true':'false');b.tabIndex=on?0:-1;
    if(activeTab==='mods')document.getElementById(b.getAttribute('aria-controls')).classList.toggle('active',on);
  });
  if(remember){try{sessionStorage.setItem('forgepact_mods_subtab',target)}catch(e){}}
  if(activeTab==='mods')document.getElementById('wrap')?.scrollTo?.({top:0,behavior:'instant'});
}
export function bindModsSubtabs(){
  document.querySelectorAll('.subtabbtn').forEach(button=>button.onclick=()=>openModsSubtab(button.getAttribute('aria-controls')));
  document.querySelectorAll('.subtabbtn').forEach((button,index,buttons)=>button.onkeydown=e=>{
    const direction=e.key==='ArrowRight'?1:e.key==='ArrowLeft'?-1:0;
    if(!direction&&!['Home','End'].includes(e.key))return;
    e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+direction+buttons.length)%buttons.length;
    buttons[next].click();buttons[next].focus();
  });
}
