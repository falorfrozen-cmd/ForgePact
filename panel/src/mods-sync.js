// Parent/child switch sync for the Mods tab: a child row is disabled and
// reads "n/a" while its parent is off. Plain JS with the old page's function
// bodies unchanged (the contract tests slice them from here).

// The monster half only does anything while the parent reveal is on, so the
// control is disabled and reads "n/a" rather than silently claiming to be on.
export function syncRevealPacks(parentOn,packsOn,spawnOn){
  const row=document.getElementById('map_reveal_packs_row');
  const box=document.getElementById('map_reveal_packs');
  const val=document.getElementById('mrpval');
  if(!row||!box||!val)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Reveal full map first.';
  val.textContent=parentOn?(packsOn?'on':'off'):'n/a';
  val.className='val '+(parentOn&&packsOn?'':'off');
  // The heavy spawn pass is a second child of the same parent.
  const srow=document.getElementById('map_reveal_spawn_row');
  const sbox=document.getElementById('map_reveal_spawn');
  const sval=document.getElementById('mrsval');
  if(!srow||!sbox||!sval)return;
  sbox.disabled=!parentOn;
  srow.title=parentOn?'':'Enable Reveal full map first.';
  sval.textContent=parentOn?(spawnOn?'on':'off'):'n/a';
  sval.className='val '+(parentOn&&spawnOn?'':'off');
}
// Likewise the move to the materials tab only does anything while
// Auto-prospect is on.
export function syncProspectBag(parentOn,bagOn){
  const row=document.getElementById('mod_auto_prospect_bag_row');
  const box=document.getElementById('mod_auto_prospect_bag');
  const val=document.getElementById('apbagval');
  if(!row||!box||!val)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Auto-prospect first.';
  val.textContent=parentOn?(bagOn?'on':'off'):'n/a';
  val.className='val '+(parentOn&&bagOn?'':'off');
}
// And the show key only does anything while Sleep loot your filter hides is
// on. The key stays as picked: only its select is disabled.
export function syncHiddenLootKey(parentOn){
  const row=document.getElementById('mod_hidden_loot_key_row');
  const box=document.getElementById('mod_hidden_loot_key');
  if(!row||!box)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Sleep loot your filter hides first.';
}
// And Dungeon chest opens early's countdown form only does anything while its
// switch is on. The form stays as picked: only its select is disabled.
export function syncDungeonChestCountdown(parentOn){
  const row=document.getElementById('dungeon_chest_countdown_row');
  const box=document.getElementById('dungeon_chest_countdown');
  if(!row||!box)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Dungeon chest opens early first.';
}
