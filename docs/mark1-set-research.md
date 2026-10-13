# Mark 1 set (ForgePact #166): research phase

**Question.** Issue #166 asks for a four-piece item set, **Mark 1**, whose
full-set bonus casts the skills on the bar's Q, E, R and Y slots whenever the
game would let them be cast, takes away the movement penalty of those casts,
halves the final movement speed, triples the jump's distance, and lets the
jump ignore collisions while landing only somewhere walkable. Five things
behind that are not known on this build. How can the game's own cast gate be
made to cast a bar slot without a key press, so that the game keeps every
cooldown, resource and requirement check? What slows or stops the caster
while a skill is cast? Where can the player's final walking speed be scaled by
exactly 0.5? Where can the jump's length be scaled by 3? And how can a
walkable landing point be told from one past the walkable map's edge?

**Why it matters.** A mod written against a guess at any of these costs a
player build and a launch per wrong guess. Each question has several
candidate routes, and the shared references had none of the names the cast
gate reads. So this phase folds the static readings into the toolkit's shared
references, adds one research-build instrument (`mark1probe`) that holds
every candidate lever for all five questions behind one command, runs two live
sessions on that one build, and records a `## Decision`. The set itself (the
four pieces as Custom Forge entries, the worn-set read, the automation with
the toggle border, the movement and jump levers, the walkable landing check,
a panel switch that is off by default and the release notes) is phase 2,
written against that Decision.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices. This doc carries no decompiled script text, no
address and no struct offset. A reading is labelled as a reading, and it is not
a fact until a live session records it.

**Out of scope for this phase.** The mod itself and every phase 2 choice;
pressing keys from the mod, calling `TalentUse` by name as a cast route, and
writing cooldown, mana or talent state (§ Static search, "Rejected routes");
art, the Item Editor, co-op, and classes other than the live character's; and
anything that suspends the game's own loop.

## Status

- **Phase:** research (phase 1 of 2). Nothing in this phase is
  player-visible, and the player build is free of it.
- **Static reading:** done on 2026-10-13 (§ Static reading). The readings
  about the game are also in the toolkit's `docs/RUNTIME_DATA_MODELS.md`,
  § 7.7 "The cast gate and its input", § 7.8 "What a cast does to movement",
  § 19.10 "Walking speed and jump length" and § 19.11 "The walkable map and
  its edge", labelled as static readings.
- **Instrument:** `mark1probe`, research build only (§ Instrument).
- **Live procedure 1** (the cast) and **Live procedure 2** (speed, the jump
  and walkability): not run yet.
- **Decision:** every key reads `pending` (§ Decision).

## Static search

Searched on 2026-10-13 over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`
and `objects.hpp` for every script and object whose name contains `talent`,
`cast`, `input`, `queue`, `move`, `speed`, `jump`, `leap`, `path`, `grid` or
`wall` (case-insensitive; the generated anonymous functions and struct
constructors left out). Code spells each name through its SDK constant; each
script index below is its `gml_Script_` constant's index. "Role" is what the
static reading (§ Static reading) found, or "(name)" where only the name
speaks for it.

### Talent: 122 scripts, 19 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_CheckTalentUse` | 539 | the cast gate: loops over the bar's slots and calls `TalentUse` for a slot that asks; called from `PlayerMovement` |
| `gml_Script_TalentUse` | 3829 | the cast; one call per key press (RDM § 7.2, measured) |
| `gml_Script_TalentUseClass` | 3833 | the class's cast body, about 19 frames later (RDM § 7.2, measured) |
| `gml_Script_TalentUseSetSpeed` | 3830 | cast speed; also writes six `playerEffect` entries (§ Static reading) |
| `gml_Script_TalentRequirement` | 3769 | the gate's requirement check, just before `TalentUse` |
| `gml_Script_GetTalentInfo` | 1788 | talent data lookups, by the gate and by `TalentUseSetSpeed` |
| `gml_Script_GetTalentCooldown` | 1782 | called by `TalentUseSetSpeed` and the class body |
| `gml_Script_CA_playerTalentActive` | 368 | network handler; the second direct caller of `TalentUseSetSpeed` |
| `gml_Script_MobileTalentInput` | 1265 | the mobile layout's touch buttons; called from `DrawHudMobile` |
| `gml_Script_GetPlayerTalentHudObj` | 1725 | the HUD bar object, called by the gate |
| `gml_Script_NetworkSendClientTalentUse` | 2523 | network echo of a cast (RDM § 7.2 saw it beside a double cast) |
| `gml_Script_TalentsUniversal` | 3825 | universal talents; one of the callers of `IsObtainablePlace` |
| `Mobile_Talent_Direction_obj` | 2902 | writes `mobileTalentPressed`, one of the gate's per-slot flags |
| `UI_Hud_Talent_obj` | 5099 | the drawn skill bar (`row0`, RDM § 8.3) |

Not this question (talent screens, loadouts, per-class talent tables and
bodies, tooltips, network sync, mercenary talents): `ApplyMercenaryTalentReset`, `CA_playerExtraTalentUpdate`, `CA_playerTalentUpdate`, `ChangeTalentLoadout`, `DrawMobileTalentIcon`, `DrawTalentLines`, `DrawTalentUiTopButtons`, `GetMercenaryTalentStartIndex`, `GetMobileTalentPosition`, `GetSubTalentInfo`, `GetTalentDescription`, `GetTalentId`, `GetTalentLevelReq`, `GetTalentLoadoutAmount`, `GetTalentSeed`, `LoadMonsterTalentModifiers`, `LoadSubTalentDamage`, `LoadTalentDamage`, `LoadTalentDamageBase`, `NetworkSendClientAllTalents`, `NetworkSendMercenaryTalentReset`, `NetworkSendTalentUpdate`, `PopulateTalentStructMap`, `PopulateTalentStructMapAmazon`, `PopulateTalentStructMapBard`, `PopulateTalentStructMapButcher`, `PopulateTalentStructMapDemonSlayer`, `PopulateTalentStructMapDemonspawn`, `PopulateTalentStructMapExo`, `PopulateTalentStructMapIllusionist`, `PopulateTalentStructMapJotunn`, `PopulateTalentStructMapMarauder`, `PopulateTalentStructMapMarksman`, `PopulateTalentStructMapNecromancer`, `PopulateTalentStructMapNomad`, `PopulateTalentStructMapPaladin`, `PopulateTalentStructMapPirate`, `PopulateTalentStructMapPlagueDoctor`, `PopulateTalentStructMapProphet`, `PopulateTalentStructMapPyromancer`, `PopulateTalentStructMapRedneck`, `PopulateTalentStructMapSamurai`, `PopulateTalentStructMapShaman`, `PopulateTalentStructMapShieldLancer`, `PopulateTalentStructMapStormweaver`, `PopulateTalentStructMapViking`, `PopulateTalentStructMapWhiteMage`, `ReturnClassTalentTrees`, `ReturnClassTalents`, `ReturnSubTalentLevel`, `ReturnTalentLevel`, `ReturnTalentValue`, `TalentBenediction`, `TalentHonedDefenses`, `TalentPullMonster`, `TalentTaunt`, `TalentsAmazon`, `TalentsAugments`, `TalentsButcher`, `TalentsDemonSlayer`, `TalentsDemonSpawn`, `TalentsExo`, `TalentsIllusionist`, `TalentsJotunn`, `TalentsMarauder`, `TalentsMarksman`, `TalentsMercenary`, `TalentsNecromancer`, `TalentsNomad`, `TalentsPaladin`, `TalentsPirate`, `TalentsPlagueDoctor`, `TalentsPyromancer`, `TalentsRedneck`, `TalentsRelics`, `TalentsSamurai`, `TalentsShaman`, `TalentsShieldLancer`, `TalentsStormWeaver`, `TalentsViking`, `TalentsWhiteMage`, `Talents_23_Bard`, `Talents_24_Prophet`, `UiAActiveTalentSelect`, `UiAContextTalentLoadout`, `UiAContextTalents`, `UiAMercenaryTalent`, `UiAMercenaryTalentReset`, `UiAMercenaryTalentResetConfirm`, `UiAOpenTalents`, `UiATalentChange`, `UiATalentScreenAssign`, `UiATalentScreenRename`, `UiATalentScreenResetAttributes`, `UiATalentScreenResetTalents`, `UiATalentScreenTalent`, `UiATalentScreenTalentLoadout`, `UiATalentsEther`, `UiATalentsIncarnation`, `UiATalentsMercenary`, `UiATalentsPlayer`, `UiDrawTalentLoadout`, `UiHideTalentsWithNoBind`, `UiHudTalentNavigation`, `UiSetTalentSelectTopRowEnabled`, `UiSetTalentSelectTopRowEnabledGamePad`, `UiTalentNavigation`, `UiUpShowAngelicTalentTooltip`, `close_talents`, `s_TalentInfo`; objects
`Create_Char_Talent_Tooltip_obj`, `Player_Talent_Tooltip_obj`, `UI_Button_Mercenary_Talent_obj`, `UI_Button_Subtalent_obj`, `UI_Button_Talent_Demo_obj`, `UI_Button_Talent_Filler_obj`, `UI_Button_Talent_Player_obj`, `UI_Mercenary_Talent_Screen_Attributes_Container_obj`, `UI_Mercenary_Talents_obj`, `UI_Mobile_Talent_Round_Button_obj`, `UI_Sub_Talents_obj`, `UI_Talent_Button_obj`, `UI_Talent_Node_Tree_Parent_obj`, `UI_Talent_Screen_Allocate_obj`, `UI_Talent_Screen_Attribute_obj`, `UI_Talent_Screen_Attributes_Container_obj`, `UI_Talent_Screen_obj`.

### Cast: 7 scripts, 7 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_StatFasterCastRate` | 3367 | the getter behind dispatcher id 106, which `TalentUseSetSpeed` asks for |
| `gml_Script_DrawCastbar` | 1198 | the cast bar's drawing (name) |
| `Castbar_Parent_obj` | 773 | the cast bar (name) |
| `Universal_Double_Cast_obj` | 5318 | the double-cast proc's caller (RDM § 7.2, measured) |

Not this question (town portal, an enemy's cast, a castle, a monster): `CA_temporalCast`, `CastTownPortal`, `ContextCastTownPortal`, `PathFindAggroBroadcast`, `PlayerCastTownPortal`;
objects `Dead_Sarcaster_obj`, `Fall_Castle_Town_House_01_obj`, `Fall_Castle_Town_House_02_obj`, `Fuji_Castle_01_obj`, `Sarcaster_obj`.

### Input: 17 scripts, 3 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_InputSimulate` | 1903 | the game's own input simulation: stores a value under a key in a profile slot's `inputSimMap`; no direct caller found |
| `gml_Script_InputCheck` | 1885 | reads a profile slot's `inputState`, as the gate does |
| `gml_Script_InputPressed` | 1886 | the same, for a press |
| `gml_Script_InputReleased` | 1887 | (name) |
| `gml_Script_InputCleared` | 1888 | (name) |
| `gml_Script_ClearSpecificInput` | 1734 | clears one input of a profile slot; one caller is `PlayerMovement` |
| `gml_Script_ControllerCheckInput` | 636 | gamepad input; calls `ClearSpecificInput` at eleven sites |
| `gml_Script_KeyboardMouseInput` | 1900 | (name) |
| `gml_Script_PollKeyboardInputs` | 644 | (name) |
| `gml_Script_PollGamepadInputs` | 645 | (name) |
| `gml_Script_MobileTalentInput` | 1265 | the mobile layout's touch buttons (under Talent) |
| `Input_Device_Manager_obj` | 2259 | holds `playerProfileSlot`, the profile entries whose `inputState` the gate reads |

Not this question (device names, bindings, the inventory grid, string
commands, the layout editor): `CheckDeviceInput`, `GetPlayerInputBindings`, `ProcessInventoryGridInput`, `ReturnDeviceInput`, `StringReplaceInputCommands`, `UI_Layout_Apply_Input`; objects `Office_Input_Devices_Left_obj`, `Office_Input_Devices_Up_obj`.

### Queue: no script, 1 object

No script name contains `queue`. The one object, `UI_Login_Queue_obj`, is the login
queue's interface. The cast gate's queue is a variable, `abilityQueue` (§
Static reading).

### Move: 52 scripts, 4 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_PlayerMovement` | 2774 | the walk step, called from the player's Step event; calls the cast gate (RDM § 19.6) |
| `gml_Script_PlayerMoveSkill` | 2776 | called once from `skillsLeap`; reads `pSt` and sets `x` and `y` |
| `gml_Script_StatMovementSpeed` | 3389 | the getter behind dispatcher id 64 |
| `gml_Script_CanMove` | 467 | not observed during a walk (RDM § 19.6, measured) |
| `gml_Script_CA_playerMove` | 352 | network handler (name) |
| `gml_Script_CA_playerSetMoveDirection` | 364 | network handler: reads a direction from a packet into `clientMoveDir` |
| `gml_Script_RefreshMouseMove` | 3252 | called by `PlayerMovement`, `TalentUseSetSpeed` and `playerJumpGravity` (name) |
| `Mouse_Move_obj` | 2980 | the click-to-move marker; named by the gate, `PlayerMovement` and `TalentUseSetSpeed` |

Not this question (every `Remove` name, enemy, camera, network and interface
movement): `BuffRemove`, `BuffRemoveMorph`, `CA_eacRemovePlayer`, `CA_enemyBuffRemove`, `CA_moveInstance`, `CA_moveInstancePool`, `CA_playerRemoveFlag`, `CameraMoveSmooth`, `EnemyRemoveBuff`, `EnemyRemoveBuffStack`, `GoblinMovement`, `GridRemoveItem`, `InventoryGridRemoveItem`, `InventorySocketUpdateAndRemove`, `InventoryStackUpdateAndRemove`, `MarketRemoveItem`, `MonsterClientMovement`, `NetworkSendClientMoveDir`, `NetworkSendEnemyMovePool`, `NetworkSendFlagRemove`, `NetworkSendMoveInstance`, `OnlineRemoveItem`, `RelicRemoveQuest`, `RelicRemoveSlot`, `RemoveFromRenderer_anon_1667_BulbShadowOverlay_BulbShadowOverlay`, `RemoveFromRenderer_anon_1688_BulbLightOverlay_BulbLightOverlay`, `RemoveFromRenderer_anon_1761_BulbDynamicOccluder_BulbDynamicOccluder`, `RemoveFromRenderer_anon_1816_BulbStaticOccluder_BulbStaticOccluder`, `RemoveFromRenderer_anon_610_BulbDirectionalLight_BulbDirectionalLight`, `RemoveFromRenderer_anon_909_BulbLight_BulbLight`, `RemoveItemFromMap`, `RemoveMetamorphosisSkill`, `StringRemoveCommands`, `SummonStepMoveToPlayer`, `UI_Layout_Apply_Move`, `UiAChatDropdownGuildRemove`, `UiAMarketRemove`, `UiAMarketRemovePrompt`, `UiAOptionsIngameMoveStick`, `UiAPresetMove`, `UiMoveNode`, `UiRemoveEtherNode`, `UiRemoveIncarnationNode`, `UiRemoveNode`, `UiUpOptionsIngameMoveStick`; objects `Move_Platform_Horizontal_obj`, `Move_Platform_Vertical_obj`, `UI_Preset_Move_All_obj`.

### Speed: 8 scripts, 2 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_StatMovementSpeed` | 3389 | the movement speed stat (dispatcher id 64); asked for by the player's Step event |
| `gml_Script_TalentUseSetSpeed` | 3830 | cast speed (above) |
| `gml_Script_StatAttackSpeed` | 3392 | dispatcher id 72 (name) |
| `gml_Script_StatAttackSpeedMainHand` | 3393 | dispatcher id 68, asked for by `TalentUseSetSpeed` |
| `gml_Script_StatAttackSpeedOffHand` | 3394 | dispatcher id 69, asked for by `TalentUseSetSpeed` |
| `gml_Script_SetRoomSpeed` | 3645 | the game's frame rate (`game_set_speed`), not the player's |

Not this question: `DrawInventoryAttackSpeed`, `EnemyKillBurstOfSpeed`; objects `Debug_Speedometer_obj`, `Helheim_River_Speedlines_obj`.

### Jump: 5 scripts, 9 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_StatJumpPower` | 3391 | dispatcher id 66; asked for only by the player's Alarm 8 event |
| `gml_Script_playerJumpGravity` | 2764 | runs every frame of a jump (RDM § 19.1, measured); the landing side |
| `gml_Script_CA_playerJump` | 348 | not called by the local jump (RDM § 19.1, measured) |
| `gml_Script_PlayerForceJump` | 2770 | not called by the local jump (RDM § 19.1, measured) |
| `Universal_Jump_Land_Stun_obj` | 5343 | named by `skillsLeap` and `playerJumpGravity`; a landing effect (name) |

Not this question (an enemy's jump, quest and secret jump spots, spawn
triggers): `CA_enemyJump`; objects `Monster_Jump_Spawn_obj`, `Monster_Jump_Trigger_obj`, `Quest_Jump_Spot_obj`, `Secret_Jump_obj`, `Trigger_03_Spawn_Jump_01_obj`, `Trigger_03_Spawn_Jump_02_obj`, `Trigger_03_Spawn_Jump_03_obj`, `Trigger_03_Spawn_Jump_04_obj`.

### Leap: 1 script, 7 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_skillsLeap` | 3664 | runs every frame of a jump (RDM § 19.1, measured); sets `x` and `y`, calls `PlayerMoveSkill` and `IsObtainablePlace` |

Not this question (class skills' leap effects): objects `Amazon_Leaping_Ambush_obj`, `Jotunn_Freezing_Leap_Shockwave_obj`, `Jotunn_Freezing_Leap_obj`, `Prophet_Leaping_Charge_Claw_obj`, `Prophet_Leaping_Charge_Herald_obj`, `Prophet_Leaping_Charge_Shuriken_obj`, `Prophet_Leaping_Charge_Trail_obj`.

### Path: 39 scripts, 8 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_PathFindControllerCreate` | 2714 | builds `global.pathGrid` (RDM § 22.7, measured; § 19.11) |
| `gml_Script_PathFindApplyStaticBlockers` | 2718 | adds the static rectangles to the grid |
| `gml_Script_PathFindBuildStaticBlockers` | 2717 | builds those rectangles from the zone generator's outside list |
| `gml_Script_PathFindUpdateGridCell` | 2710 | path grid upkeep (name) |
| `gml_Script_PathFindMarkInstances` | 2719 | path grid upkeep (name) |
| `gml_Script_PathFindRestoreArea` | 2720 | references `global.pathStaticRects` (name) |
| `gml_Script_PathFindStartPath` | 2708 | a monster's path start (name) |
| `Path_Blocker_obj` | 3401 | (name) |
| `Pathfinding_obj` | 3402 | (name) |

Not this question (monster pathing ticks, the Ether and Incarnation boards'
paths, path decorations): `CheckPath`, `CheckPathEther`, `FindPathEther`, `FindPathIncarnation`, `PathFindAggroBroadcast`, `PathFindAttackTick`, `PathFindChaseTick`, `PathFindCleanUp`, `PathFindControllerStep`, `PathFindCreate`, `PathFindDirectApproach`, `PathFindDisabledTick`, `PathFindDropBadTarget`, `PathFindInitTimers`, `PathFindLeashCheck`, `PathFindReceiveTarget`, `PathFindRepathToSocket`, `PathFindRetargetTick`, `PathFindScanTick`, `PathFindSeparate`, `PathFindSeparateTick`, `PathFindStep`, `PathFindStepChase`, `PathFindStepIdle`, `PathFindTakeTarget`, `PathFindTauntTick`, `PathFindTrySocket`, `PathFindValidateTarget`, `SocketDirectPath`, `SocketRepathSkip`, `UpdatePathfind`, `UpdatePathfindLight`; objects `Autotile_Dark_Path_obj`, `Autotile_Path_obj`, `Aztec_Pyramid_Blood_Path_01_obj`, `Goblin_Pathpoint_obj`, `Plague_Doctor_Plague_Master_Pathogens_obj`, `Scythe_Path_obj`.

### Grid: 42 scripts, 9 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_PathFindUpdateGridCell` | 2710 | path grid upkeep (name) |
| `gml_Script_mp_grid_free` | 2078 | (name) |
| `gml_Script_ds_grid_floodfill` | 1426 | (name) |

Not this question (inventory, stash, crafting, shop and interface grids):
`CraftEditGrid`, `GetGridTypeName`, `GetInventoryGridNode`, `GetItemPreferredGrid`, `GetStackOpLocationFromGridType`, `GridAddItem`, `GridAddToStack`, `GridClear`, `GridHasSpace`, `GridRemoveItem`, `GridSettle`, `GridStopIdentify`, `InvGridClearItemNode`, `InvGridEquipGamepad`, `InvGridEquipV2`, `InventoryGridAddItem`, `InventoryGridAddItemPos`, `InventoryGridAddItemToTab`, `InventoryGridAddToStack`, `InventoryGridCanAddToStack`, `InventoryGridHasSpace`, `InventoryGridHasSpaceMulti`, `InventoryGridRemoveItem`, `InventoryInitGrids`, `ParseItemToGrid`, `PopulateBlackMarketGrid`, `PopulateMerchantGrid`, `PopulateTravelingMerchantGrid`, `ProcessInventoryGridInput`, `PushEntryToGrid`, `PushPresetToGrid`, `SetBresenhamLineToGrid`, `StashGridAddItem`, `UiOutOfGridNavigationFunc`, `UiResetGrid`, `UiSetGrid`, `UiSetGridArray`, `ds_grid_upscale`, `s_ItemGridInfo`; objects `Customize_Grid_Item_Tooltip_obj`, `Debug_Item_Grid_obj`, `UI_Character_Customize_Grid_obj`, `UI_Emote_Grid_obj`, `UI_Grid_obj`, `UI_Inventory_Grid_obj`, `UI_Inventory_Relic_Grid_obj`, `UI_Inventory_Tarot_Grid_obj`, `UI_Shop_Grid_Item_obj`.

### Wall: 2 scripts, 76 objects

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_ZoneGenMakeZoneWalls` | 3601 | writes the zone generator's `heatGridL` (§ 19.11 of the shared references) |
| `gml_Script_ZoneWallCheckNearby` | 3600 | called by `ZoneGenMakeZoneWalls`; places zone walls and lights (name) |
| `Wall_Parent_obj` | 5701 | the wall family; a child of `Collision_Parent_obj` (RDM § 19.8) |
| `Wall_obj`, `Wall_Wooden_obj`, `Wall_Boss_obj` | 5700 and family | walls (RDM § 19.8) |
| `Invisible_Wall_obj`, `Invisible_Wall_Not_Minimap_obj` and the six other `Invisible_Wall_*` objects | 2269, 2268 and family | the map's invisible edges; walls (RDM § 19.8) |
| `Wall_Creator_obj` | 5698 | (name) |
| `Wall_Indicator_obj` | 5699 | (name) |

Not this question (decorative walls of each act, boss walls, skill effects
named for walls): objects `Architect_Vector_Wall_Horizontal_obj`, `Architect_Vector_Wall_Vertical_obj`, `Autotile_Minimap_Wall_obj`, `Aztec_Pyramid_Glyph_Wall_01_obj`, `Bad_Trip_Wall_01_obj`, `Bad_Trip_Wall_01b_obj`, `Bad_Trip_Wall_02_obj`, `Bad_Trip_Wall_02b_obj`, `Bifrost_Wall_obj`, `CT_Boss_Wall_01_obj`, `CT_Boss_Wall_02_obj`, `Cathedral_Window_Wall_01_obj`, `Chaos_Tower_Lobby_Wall_01_obj`, `Chaos_Tower_Lobby_Wall_02_obj`, `Chaos_Tower_Lobby_Wall_03_obj`, `Chaos_Tower_Lobby_Wall_04_obj`, `Cthulhu_Wall_Block_obj`, `Fallen_Realm_Ruins_Wall_01_obj`, `Fallen_Realm_Ruins_Wall_02_obj`, `Fallen_Realm_Ruins_Wall_03_obj`, `Fallen_Realm_Ruins_Wall_04_obj`, `Fallen_Realm_Ruins_Wall_05_obj`, `Fallen_Realm_Ruins_Wall_06_obj`, `Fallen_Realm_Ruins_Wall_07_obj`, `Fuji_Wall_01_obj`, `Fuji_Wall_02_obj`, `Fuji_Wall_03_obj`, `Fuji_Wall_04_obj`, `Fuji_Wall_05_obj`, `Fuji_Wall_06_obj`, `Mevius_Tentacle_Wall_obj`, `Mevius_Wall_obj`, `Niflhel_Ruins_Wall_01_obj`, `Niflhel_Ruins_Wall_02_obj`, `Niflhel_Ruins_Wall_03_obj`, `Niflhel_Ruins_Wall_04_obj`, `Niflhel_Ruins_Wall_05_obj`, `Niflhel_Ruins_Wall_06_obj`, `Niflhel_Ruins_Wall_07_obj`, `Odin_Wall_obj`, `Quest_Gladsheim_Secret_Wall_obj`, `Satan_Firewall_obj`, `Shield_Lancer_Shield_Wall_Vortex_Projectile_obj`, `Shield_Lancer_Shield_Wall_Vortex_obj`, `Shield_Lancer_Shield_Wall_obj`, `Uber_Anubis_Decaying_Wall_obj`, `Uber_Anubis_Wall_01_obj`, `Underground_Wall_Vines_obj`, `Universal_Wall_of_Eternity_obj`, `Universal_Wallbanger_obj`, `Valhalla_Halls_Wall_Torch_Lever_obj`, `Valhalla_Halls_Wall_Torch_obj`, `Valhalla_Ruins_Wall_01_obj`, `Valhalla_Ruins_Wall_02_obj`, `Valhalla_Ruins_Wall_03_obj`, `Valhalla_Ruins_Wall_04_obj`, `Valhalla_Ruins_Wall_05_obj`, `Valhalla_Ruins_Wall_06_obj`, `Valhalla_Ruins_Wall_07_obj`, `Wall_Vines_obj`, `Winter_Cliff_Wall_01_obj`, `Winter_Cliff_Wall_02_obj`.

### Found by following the reading

These match none of the words above, and the reading led to them.

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_ReturnSpecificStat` | 3344 | the stat dispatcher; id 64 is movement speed, 66 Jump Power |
| `gml_Script_IsObtainablePlace` | 2076 | the game's "can something stand here" test, over the zone generator's heat grid |
| `gml_Script_IsObtainablePosition` | 2077 | a second reader of the heat grid; used to place loot and monsters |
| `gml_Script_InstancePlaceTallest` | 599 | holds the walk (RDM § 19.6, measured); called by `playerJumpGravity` |
| `gml_Script_skillsBlink` | 3660 | another movement skill step that asks `IsObtainablePlace` |
| `objZoneGenV2` | 3259 | the zone generator; holds `heatGridL` and the outside block list |
| `Profile_Manager_obj` | 3673 | holds `inputState` (RDM § 10.4, measured) and makes `inputSimMap` |
| `Prison_Catapult_Bottom_obj` | 3629 | launches the player; references `playerSkipCollisions` |
| `Krampus_obj` | 2426 | references `playerSkipCollisions` |

### Rejected routes

- **Pressing the slot's key from the mod** (a posted key message, the way
  hs-drive's `hs_input` does for tests). The key bound to a slot cannot be
  read (`castKeyRule` is not measured), it would fight the player's own
  input, and it is the input-simulation family the pet-quest B4 plan
  explored. Not a lever. (The game's own `InputSimulate` above is a
  different route: it hands the gate a value through the game's profile, and
  the gate still decides. It stays a candidate.)
- **Calling `TalentUse` by name.** Measured to cast while skipping the
  gate's mana and requirement checks (`castByNameRoute`, skill actions
  research). Rebuilding that gate ourselves cannot promise "any condition the
  game checks". Not a lever in this phase; it would come back only as the
  owner's decision.
- **Writing cooldowns, mana or talent state directly.** The talent state is
  hash-checked (`skill-actions-research.md`, § Static readings).
- **A -50% movement speed affix on the pieces.** Additive; the issue asks for
  x0.5 on the final speed.
- **Skipping `PlayerMovement`, or any game call.** Blocking it outright
  crashed the game, because it also does collision, depth and state work (the
  co-op hook's own note); a lever wraps the original call. The toolkit's
  rule against suspending the game's own runtime says the same.

## Static reading

Made on 2026-10-13 in the build the Ghidra project imported (the one the
toolkit's `py -3 tools/decomp_index.py build-hash` names). Written in our own
words; every claim is a **static reading** unless it says measured.

**Sources.** The ghidra MCP decompiled the small functions:
`PathFindControllerCreate`, `PathFindApplyStaticBlockers`,
`IsObtainablePlace` and `InputSimulate`. `CheckTalentUse` was read from a
local headless decompile made on 2026-09-25 for this build, with the
decompile index's slot names. The large bodies (`StatMovementSpeed`,
`TalentUseSetSpeed`, `PlayerMovement`, `skillsLeap`, `playerJumpGravity`)
did not decompile: the MCP timed out on each, and earlier headless runs on
them timed out at the default limit. For those, and for callers, a local scan
of each script's compiled code listed the variable, builtin and script names
it references, whether each reference reads or writes, the direct calls it
makes and the constant ids it passes. The scan's positive control: on
`PathFindControllerCreate` it listed exactly what the MCP decompile of that
function shows, and on `CheckTalentUse` its read and write counts for
`abilityQueue` and `mobileTalentPressed` match the decompile. A scan of
direct call sites is blind to calls through the script table, a pointer or a
method value, so a caller it did not find is not observed, not absent.

### The cast gate (question 1)

- **Where it runs.** The only direct call of `CheckTalentUse` found is in
  `PlayerMovement`, which the player's Step event calls; so the gate runs
  inside the movement step. Measured elsewhere: its entry logs about 85 calls
  a second in town, with `self` equal to `other` and three arguments (the
  toolkit's x64dbg notes). Why that is more than one call a frame is not
  read.
- **Which argument names a slot: none.** All three are optional. The first
  is the player instance (when undefined, a value from the game's protected
  global data). The second is the player number (default: the global
  `mplr`), passed with the player into the talent-info and requirement
  lookups. The third (default 1) is the index into `Input_Device_Manager_obj`'s
  `playerProfileSlot`, and is handed on unchanged as `TalentUse`'s third
  argument, which the shared references measured as 1 on a key cast. Live 1's
  `ctu-args` records the values this machine passes.
- **The slots are a loop inside the gate,** a counter from 0 to 11. For each
  value the gate looks the slot's talent id up and decides whether the slot
  asks to cast.
- **Where each variable lives.** `playerProfileSlot` is an array on
  `Input_Device_Manager_obj`, indexed by the gate's third argument; its entry
  carries `inputState` (read on 15 paths, at an index an unnamed helper
  derives from the slot counter) and `inputSimMap`, so the entry is plausibly
  a `Profile_Manager_obj` instance. `abilityQueue`, `mobileTalentPressed` and
  `autoAim` are global arrays (created in the game's global definitions)
  indexed by the slot counter. `stackSkillList` is a variable of the gate's
  own `self`.
- **An `abilityQueue` entry is a 0-or-1 flag per bar slot.** When a slot's
  key state does not ask for a cast, the gate reads the slot's
  `abilityQueue` entry, and when that is unset too its `mobileTalentPressed`
  entry; a slot with all three unset is skipped. Later the gate writes both
  entries of the slot back with 0; one other path writes 1 into
  `abilityQueue`'s entry 0. `PlayerMovement` writes `abilityQueue` at five
  sites (values not read), and nothing else found references it.
- **The cast.** For a slot that asks, the gate calls `GetTalentInfo` and
  `TalentRequirement`, then `TalentUse` with the player, the slot's talent
  id, its third argument and false. It also asks the stat dispatcher for id
  199 (no case in the dispatcher's table) and calls `PlayerManaUpdate` and
  `PlayerHpUpdate`.
- **`InputSimulate`** stores a value under a key in a profile slot entry's
  `inputSimMap`. That map is read by `Profile_Manager_obj`'s and
  `Controller_obj`'s Begin Step events, which run before the Step event that
  reaches the gate. What its keys are, and how the Begin Step folds the map
  into `inputState`, were not read.
- **The earlier `inputState` negative does not cover this reader.** The
  pet-quest B4 research measured that writing `inputState` from the frame
  callback did nothing for the interact key, whose check reads the keyboard
  directly, and the write landed after the frame's readers ran. A write
  inside the gate's own call, just before it reads, is untested: not refuted.

### What a cast does to movement (question 2)

- **`TalentUseSetSpeed`** has two direct callers found, `TalentUse` and
  `CA_playerTalentActive`. It asks the dispatcher for ids 68, 69 and 106 (the
  two attack speeds and faster cast rate). Besides the cast speed it writes
  six entries of the player's `playerEffect` array (entries 40, 46, 79, 182,
  231 and 282; what each means is not read) and sets `image_speed` on an
  instance or object it names. It names no movement-speed variable
  (`hSpeedFactor`, `vSpeedFactor`, `mobileSpeedFactor`, `moveSpeed`).
- **`PlayerMovement`** calls `TalentUseClass` once, reads its `playerEffect`
  at 21 sites and takes it for writing at 19, and reads its stat values
  `pSt` at 32 sites. The player's Step event calls `TalentUseClass` at five
  more sites. Which of these holds the caster still is not established: Live
  1's `penalty-vars` diffs them across a cast.

### Walking speed (question 3)

- **The stat is read in the player's Step event.** Dispatcher id 64 goes to
  `StatMovementSpeed`, whose only direct caller found is the dispatcher. The
  scripts found asking for id 64 are the player's Step event (once),
  `StatTotalDamage`, `StatAttackDamage`, `StatMagicSkillDamage`,
  `TalentsSamurai` and the mercenary's Alarm 5. `PlayerMovement` does not ask
  for it; it asks for id 336, which has no case.
- **What `StatMovementSpeed` reads.** The player's `playerBuff` (about 380
  reads, through the buff value and stack methods), `buffArray`,
  `debuffArray`, `buffMult` and `skpDb`; dispatcher ids 106, 531, 532 and 598
  (only 106 has a case); `ReturnSubTalentLevel` and `LoadSubTalentDamage`;
  `floor` and `power`. Whether it caps is not established: this build
  compiles `min`, `max` and `clamp` inline, so a cap does not show as a name.
- **The speed names the walk uses.** `PlayerMovement` reads `hSpeedFactor`
  and `vSpeedFactor` (five sites each, no write found), `mobileSpeedFactor`,
  `roomSpd` and `pSt`. The player's Step event reads `move_dist`, `new_x`,
  `new_y`, `hSpeedFactor`, `vSpeedFactor` and another instance's
  `mountSpeed`, writes `moveOverrideHor` and `moveOverrideVer`, and uses
  `mp_linear_step`. Which of these the final step is computed from is not
  read: Live 2's `walk-mover`, `speed-move` and `speed-stat` measure it.

### Jump length (question 4)

- **Jump Power is read by the player's Alarm 8 event,** the only script found
  asking for id 66. That event asks the dispatcher about 350 times, so it is
  plausibly the routine that refreshes the player's stat values, which fits
  the measurement that `StatJumpPower` logged no call during a jump.
- **`skillsLeap`** has three direct callers found: the player's Step event,
  `ClassStepEvent` and the mercenary's Step event. It sets `x` and `y` at
  three sites each (on which instance is not read: it also creates instances
  and handles leap skills' damage), sets `direction` (four sites),
  `deltaSpeed` (two) and `max_range` (one), reads `height` and
  `jumpDirection`, and calls `PlayerMoveSkill` (which sets `x` and `y`),
  `IsObtainablePlace` (five sites), `InheritModifiers` and the collision
  builtins, naming `Collision_Parent_obj`, `Wall_Parent_obj` and `Block_obj`.
  It asks for id 337 (no case) and damage stat ids. It names no mouse
  coordinate, so the jump's target is plausibly decided before it runs (the
  Step event reads `mouse_x`, `mouse_y`, `xMouse` and `yMouse`).
- **`jumpDirection` belongs to the scenery.** The Create events of
  `Collision_Parent_obj`, `Collision_Prop_obj` and about thirty cliff objects
  reference it: plausibly the direction a cliff lets a leap go down.
- **`playerJumpGravity`** has one direct caller found, the player's Step
  event. It reads `x` and `y` (no write found), `height`, `pSt` and
  `playerEffect`, sets `stickHost`, references `unstuck`, `unstuckDir`,
  `myShadow`, `groundDepth` and `playerSkipCollisions`, calls
  `InstancePlaceTallest`, `IsInWater` and `UpdateDepth`, and asks for id 337.
  By this reading `skillsLeap` (or `PlayerMoveSkill` inside it) moves the
  player during a jump and `playerJumpGravity` handles the landing; Live 2's
  `jump-mover` measures which.
- **The game's own collision skip.** `playerSkipCollisions` is set up in the
  player's Create event and read in its Step event, and is referenced through
  another scope by `playerJumpGravity`, the player's collision event with
  `Collision_Parent_obj`, `Krampus_obj` and `Prison_Catapult_Bottom_obj`'s
  Alarm 0 (which also names `launchPlayer`, `forceMovePlayer`, `jump` and
  `zSpeed`). The player's collision event names `x`, `y`, `xprevious`,
  `yprevious`, `speed` and `height`, the shape of an event that puts the
  player back where it was. Whether the flag gates that event, and who sets
  it when, is not read. It is the game's own candidate for a jump that
  ignores collisions.

### The walkable map's edge (question 5)

- **The path grid marks blockers, not the edge.** `PathFindControllerCreate`
  destroys any earlier `global.pathGrid`, sets `global.gridSize` to 16, makes
  an mp_grid the size of the room in 16 px cells, clears it, and adds every
  instance of `Collision_Parent_obj` with precise masks, then
  `Player_Only_Passage_obj` and `Projectile_Only_Passage_obj` when any
  exist. `PathFindApplyStaticBlockers` then adds every rectangle of
  `global.pathStaticRects` when `global.pathStaticRoom` is the current room.
  Neither marks a cell because it lies beyond the floor.
- **In a generated zone the rectangles are the outside.**
  `PathFindBuildStaticBlockers`, called from the zone generator's Create
  event, fills those rectangles from the generator's `outsideBlockList`,
  scaled by `zoneOuterScale` and the grid size. So in a generated zone the
  outside is plausibly blocked in the path grid; in a room the generator did
  not build, such as a town, nothing found marks it.
- **The game's "can something stand here" test.** `IsObtainablePlace` takes
  a point. With no `objZoneGenV2` it answers yes for any point; otherwise no
  outside the room, no when the generator's `heatGridL` is negative, and yes
  only when the point's 32 px cell and its four diagonal neighbours all hold
  something other than 30000. `heatGridL` is written by the generator's
  Create event and `ZoneGenMakeZoneWalls`, so 30000 plausibly marks a wall or
  void cell. `skillsLeap` asks it at five sites, as do `skillsBlink`, the
  player's Step event, the enemy creators and the quest manager.

### What the reading means for the instrument

- `mark1probe`'s `profile` scope should resolve `playerProfileSlot` on
  `Input_Device_Manager_obj`, indexed by the gate's third argument, not on
  the player; and its `queue` scope reads `abilityQueue` and
  `mobileTalentPressed` as globals and `stackSkillList` on the player.
- A `set` of the slot's `abilityQueue` entry during `CheckTalentUse` tests
  the queue route directly; a `set` of the slot's `inputState` entry tests
  the input route.
- The `walkable` verdict's fields (`grid`, `family`, `wall-line`, `tile`)
  do not include the game's own test, `IsObtainablePlace` over the heat
  grid; in a town that test answers yes everywhere if no generator exists
  there.

## Instrument

`mark1probe`, research build only: its forms, what each reads or changes,
its route and its positive control are written here with the command.

## Live procedure 1

The cast: route, gate, slot keys, the toggle outlier and the cast's movement
penalty. Written here before the session runs.

## Live procedure 2

Speed, the jump and walkability. Written here before the session runs.

## Results

No live session has run yet.

## Decision

Every key reads `pending` until the session that decides it. Each then
takes exactly one of its listed values, on a line of its own. A
`not-observed` closes nothing unless its positive control passed in the same
session.

cast-route: pending
cast-gate: pending
slot-keys: pending
toggle-under-automation: pending
cast-penalty: pending
penalty-lever: pending
walk-mover: pending
speed-lever: pending
jump-mover: pending
jump-lever: pending
walkable-test: pending

The values each key may take (Live 1 decides the first six, Live 2 the rest):

- `cast-route`, how a bar slot can be cast through the game's own gate:
  `input`, `queue`, `mobile`, `not-observed` or `lever-not-engaged`.
- `cast-gate`, whether the game still gates such a cast: `game-gates`,
  `bypassed`, `not-observed` or `not-run`.
- `slot-keys`, which input belongs to each of Q, E, R and Y: `measured`,
  `partial` or `not-observed`.
- `toggle-under-automation`, what a toggle skill does under the lever:
  `flips`, `stays` or `not-run`.
- `cast-penalty`, the member names that change while a skill is cast, or
  `none-observed` or `not-observed`.
- `penalty-lever`, whether holding those members back restores the walk:
  `restores`, `partial`, `no-effect` or `not-run`.
- `walk-mover`, the row that moves the player on a walk: `PlayerMovement`,
  `skillsLeap`, `playerJumpGravity`, `outside-rows` or `not-observed`.
- `speed-lever`, where x0.5 lands: `move`, `stat`, `both`, `neither` or
  `not-observed`.
- `jump-mover`, the row that moves the player during a jump: `skillsLeap`,
  `playerJumpGravity`, `PlayerMovement`, `outside-rows` or `not-observed`.
- `jump-lever`, where x3 lands: `move`, `stat`, `both`, `neither` or
  `not-observed`.
- `walkable-test`, which reading tells a walkable landing from one past the
  edge: `grid`, `wall-line`, `tile`, `family`, `combined`, `none` or
  `not-observed`.

## Not established

- Which value of `inputState` means "pressed", and how the gate derives the
  index it reads from the slot counter.
- What the third argument of `CheckTalentUse` is on this machine, and why the
  gate is entered about 85 times a second.
- Whether setting a slot's `abilityQueue` or `inputState` entry, or calling
  `InputSimulate`, makes the gate cast that slot, and whether the gate then
  keeps its cooldown and mana checks.
- What each of the `playerEffect` entries `TalentUseSetSpeed` writes means,
  and which member holds the caster still.
- Whether `StatMovementSpeed` caps its result, and which value the walk's
  final step is computed from.
- Which script moves the player during a jump, and where its length is
  decided.
- Whether `playerSkipCollisions` lets the player pass collision props, and who
  sets it.
- Whether `objZoneGenV2` exists in a town, and so whether `IsObtainablePlace`
  says anything there; what marks the walkable map's edge in a room the zone
  generator did not build.
