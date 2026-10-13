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
  and walkability): both run on 2026-10-13 on one research DLL (§ Results,
  "Live 1 results" and "Live 2 results"). The measured facts are folded into
  the same four sections of `docs/RUNTIME_DATA_MODELS.md`, relabelled
  measured, and into the hub's `hs-game-sdk/curated/mark1_set_measurements.json`.
- **Live procedure 3** (walkability only, after Live 2's walkable control
  failed and the probe's walkable read was corrected): run on 2026-10-13 on a
  rebuilt research DLL (§ Results, "Live 3 results"), folded the same way.
- **Decision:** recorded (§ Decision). A bar slot casts through the game's
  own gate when its `inputState` entry is set to the press value during the
  gate's call, and in more than 480 forced frames Mana Orb was cast once
  (what refused the repeats was not read); the jump is moved by `skillsLeap` alone
  and scaling that displacement tripled it; halving movement speed at the
  stat dispatcher slowed the walk to 0.785 of its step, not 0.5; and in town
  a walkable landing point is one inside the room where the player would meet
  nothing of `Collision_Parent_obj`. The cast's movement penalty was measured
  but no member holding it was found (§ Not established).

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

`mark1probe` exists only in the research build (`build.bat dev`): the player
build compiles none of it, and it is not a player command. Its decisions (the
parsing, the lever state, the displacement and scale arithmetic, the walkable
verdict and every reply line) live in a game-independent core,
`plugin/include/ForgePact/Mark1Probe.hpp`, which
`tests/test_mark1_probe_behavior.py` compiles whole and runs against
controlled calls. The adapter in `ModuleMain.cpp` does the game's side, and
`tests/test_mark1_probe_contract.py` pins it. Every script is named by its
`hs-game-sdk` constant, every object by its `hs-game-sdk` name or through
`asset_get_index`, and nothing calls or reads through an address.

### The rows and `hook`

Nine rows, one per script the five questions lean on: `CheckTalentUse`,
`TalentUse`, `TalentUseSetSpeed`, `PlayerMovement`, `ReturnSpecificStat`,
`StatMovementSpeed`, `skillsLeap`, `playerJumpGravity` and `StatJumpPower`.

- **Bare `mark1probe`** reads nothing in the game. It prints
  `mark1probe: research build`, then
  `mark1probe: rows=9 hooked=<k> held=<h> lever=<form|off> watch=<on|off>`,
  then the usage. Before `hook` it reads `rows=9 hooked=0 held=0`, which is
  the sessions' `marker` check.
- **`hook`** installs every row by name through `HookOneScript`, both
  routes: the script-table swap and the inline detour at the script's own
  code. A row counts as hooked only when its inline detour went in, because
  compiled GML reaches these scripts by direct calls that a table-only hook
  never sees; a row whose detour did not go in is reported `failed` and named
  table-only. The reply is
  `mark1probe hook: <n> detoured, <f> failed, <h> held`, then the holder of
  each held row. A second `hook` leaves the rows already installed alone.
  Positive control: the sessions' `hook-native` check reads
  `9 detoured, 0 failed, 0 held`, and `show` then counts `CheckTalentUse`
  calls rising while the player stands still, since the gate runs every
  frame.
- **A row another hook holds is left to it.** A second install of a script
  only gets the table route, which is blind, so the probe reads each row's
  holder first (from the holders' own records and from the script table),
  reports the row `held` with the holder's name where it is known, and never
  installs on top. The holders known when this was written: `CheckTalentUse`
  (`tgprobe`, `jumpprobe`), `TalentUse` (co-op's own hook, installed only by
  co-op rendering), `TalentUseSetSpeed` (`tgprobe`, `projprobe`),
  `ReturnSpecificStat` (`skillslider`, `projprobe`), `skillsLeap`
  (`jumpscenery`, `jumpprobe`), `playerJumpGravity` and `StatJumpPower`
  (`jumpprobe`). `toggleguard` holds `TalentUseClass`, which is not a row but
  runs inside a cast, and a worn flight charm arms flight's collision hooks.
  So the sessions start with none of `jumpscenery`, a worn flight charm,
  `skillslider`, `toggleguard`, `tgprobe`, `jumpprobe` or `projprobe` armed.
- **Only the local player's calls are touched.** Each row compares the
  call's `self` with the local player, found by the shared resolver that
  accepts the runner's reference handle as well as an object. Every other
  call, and every call while no lever is on, runs the original once and
  returns its result unchanged: that is the baseline, and the behaviour suite
  pins it for every row.

### The reading forms

- **`off`** turns every lever, watch, `args`, `count` and `pos` off. The rows
  stay installed and pass every call through.
- **`show`** prints, per row, `calls=<local>/<all>` (the calls whose `self`
  was the local player, over all calls), the lever's counts (`applied=`,
  `missing=`, `moved=`, `still=` and the stat counters below), and `casts=`
  per talent id of the local player's `TalentUse` calls since `hook` or the
  last `show reset`. Positive control: a key cast of the ordinary skill adds
  one cast of its talent id (`key-cast-control`).
- **`args <n>`** (1 to 100) prints the next n local-player calls of
  `CheckTalentUse`, one line each:
  `ctu #<k> f=<frame> argc=<c> a0=<v> a1=<v> a2=<v>`, each argument as a
  number or bool, or as its kind (`ref:`, `str:`, `undefined` and so on), and
  `none` past the count. Positive control: the gate runs every frame, so an
  idle `args` fills within a few frames; one that does not means the row is
  not detoured.
- **`watch <scope> <frames> [after <row>]`** takes a snapshot of the scope's
  number and bool members every frame and prints one line per change,
  `watch f=<frame> <scope>.<name>=<old>-><new>`, with `none` for a member that
  appeared or went away. With `after <row>` the window opens at that row's
  next local-player call instead of now. After 400 lines the watch ends and
  its end line counts what was dropped. The scopes, as the instrument
  resolves them:
  - `player`: the local player instance's own variables.
  - `pst`: the struct the player holds in `pSt`.
  - `profile`: the entry of `Input_Device_Manager_obj` instance 0's
    `playerProfileSlot` array at the gate's third argument (the local
    player's latest `CheckTalentUse` call; 1 until one has been seen). The
    reply names the index and where it came from (`:a2` or `:default`) and
    what the entry is (`=Profile_Manager_obj` or the object's name). An
    unresolved route is refused with its reason; nothing stands in for it.
  - `input`: the same scope's `inputState` array, each element printed as
    `input[<i>]`.
  - `queue`: `abilityQueue`, `mobileTalentPressed` and `stackSkillList`, each
    as its kind, its length and its first four entries. `abilityQueue` and
    `mobileTalentPressed` read as globals and `stackSkillList` on the player;
    the watch's `scope=` names where each was read, and a member that is not
    there reads `absent`. Any other `queue.<name>` is refused, and a global
    is written only as an element (`queue.abilityQueue[<i>]`), never as a
    whole.

  Positive control: a `watch player` while the player walks shows `x` or `y`
  changing; a watch that prints nothing then is the instrument, not the game.
- **`count <object name> <frames>`** resolves the object by name and prints
  one line per change of its instance count,
  `count f=<frame> <object>=<old>-><new>`. A name that is not an object is
  refused. Its reading is checked against the screen: a toggle's aura that
  appears or goes away while `count` prints nothing means the count, not the
  game.
- **`pos <frames>`** prints the cursor in room coordinates at the start (the
  same cursor routes `jumpscenery` reads), then a line
  `pos f=<frame> <row> dx=<px> dy=<px>` for each local-player call of a row
  that moved the player, and at the end
  `pos total=<px> frames=<n> movers=<row>:<count>,...`. It also samples the
  player's position once a frame, so a move no row made shows up as an
  `outside` line and as `outside:` among the movers, rather than reading as a
  player who stood still. Positive control: the sessions' `walk-control` and
  `jump-control`, two equal walks or jumps whose totals agree within 10%.

### The levers

One lever is on at a time; a lever form while another is on replaces it and
says so. Every lever runs for `frames` frames (600 when not given, at most
3600) and then ends on its own, printing its counts. A lever on a row that is
not hooked is refused, and a lever on a row that is hooked table-only says
that only the table's calls reach it.

- **`set <scope>.<name>[<i>] <value> during <row> [frames]`**: before each
  local-player call of the row, write the value into that member (or array
  element); after the original returns, or throws, put back what was there.
  Only a member or element that already exists is written; nothing is
  created, and a write that finds nothing counts `missing=`. The reply names
  the scope it resolved, the value supplied and the value it will restore;
  `show` counts `applied=` only for a write that reads back as the value
  written. This is the route that tests whether the gate
  casts a slot from its input entry or its queue entry, written inside the
  gate's own call, just before the gate reads it.
- **`scale <scope>.<name>[<i>] <mult> during <row> [frames]`**: the same,
  multiplying the member's number.
- **`move <row> <mult> [frames]`**: after each local-player call of the row
  that changed the player's `x` or `y`, set them to where the call started
  plus mult times the call's own displacement. A call that did not move the
  player is left alone. `show` counts `moved=` and `still=`.
- **`stat <id> <mult> [frames]`**: a `ReturnSpecificStat` call whose `self` is
  the local player and whose stat id is the one given returns the original's
  number times mult. For the two ids with a getter row (64 movement speed,
  `StatMovementSpeed`; 66 jump power, `StatJumpPower`), a local-player call of
  that getter is scaled too, because compiled GML can call a getter directly
  as well as through the dispatcher. The counts keep the two routes apart:
  `changed-dispatch=`, `changed-getter=`, `getter-direct=` (getter calls no
  dispatcher call was around) and `skip=` (a matching call whose result was
  not a number). Both changed counts at 0 means the lever never engaged where
  the game reads the stat, which is not the same as engaged and changed
  nothing.

### The walkable verdict

`walkable here <dx> <dy>` reads one point relative to the player,
`walkable at <x> <y>` one point in room coordinates, and
`walkable scan <deg> <step> <count>` a line of points from the player in that
direction. Each point prints
`walkable x= y= room=<in|out> grid=<free|blocked|unreadable> family=<free|blocked|unreadable> wall-line=<clear|crossed|unreadable> tile=<layer>:<value>,... grid-raw=<raw> family-raw=<raw> wall-line-raw=<raw>`,
followed by ` met=<object name>:<instance id>` when `family` reads blocked:

- `room`: whether the point lies inside the room.
- `grid`: the point's 16 px cell in `global.pathGrid` (`mp_grid_get_cell`,
  -1 for a blocked cell).
- `family`: whether the player, placed at the point, would meet the
  `Collision_Parent_obj` family (`place_meeting`).
- `wall-line`: whether a line from the player to the point crosses the
  `Wall_Parent_obj` family (`collision_line`).
- `tile`: up to four tile layers' cell values at the point, by layer name.
- `grid-raw`, `family-raw`, `wall-line-raw`: the builtin's answer as the
  probe received it, `<kind>:<value>`, the kind in lower case without its
  prefix (`bool`, `real`, `int32`, `int64`, `ref`, `undefined`, ...);
  `error` when the call itself failed, `none` when it was not made.
- `met`: what a blocked `family` met, `instance_place` on the same family
  at the same point, by object name and instance id.

Each field reads its builtin's answer by that builtin's meaning. `family` is
a meeting answer: true or a non-zero number is blocked, false or 0 is free.
`wall-line` is an instance answer: an instance (an id or ref of 0 or more)
is crossed, `noone` (-4) is clear. Live 2's build read both as an instance
answer, so `place_meeting`'s numeric 0 counted as a meeting and `family`
printed blocked everywhere (Live 2 results, `walkable-control`). Live 3
measured that `place_meeting`, called this way, answers a `real` 0 or 1, not
a `bool`, and `collision_line` a `ref` (-4 for `noone`); ForgePact `61243b2`
made the correction and added the raw fields and `met`.

Every field starts unreadable and only a read that succeeded replaces it, so
a failed read never turns into either answer. The game's own test,
`IsObtainablePlace` over the zone generator's heat grid, is not one of the
fields (§ Static reading, "What the reading means for the instrument").
Positive control: the sessions' `walkable-control`, a point 32 px from the
player on open floor reading `room=in grid=free family=free wall-line=clear`
(Live 3 reads the player's own position and 32 px to either side).

### Where the `profile` and `queue` scopes read

The instrument follows the reading (§ Static reading, "The cast gate"):
`profile` and `input` resolve `Input_Device_Manager_obj.playerProfileSlot[<third
argument>]`, and `queue` reads `abilityQueue` and `mobileTalentPressed` as
globals and `stackSkillList` on the player. The `profile` reply names the
index and the entry's object, so Live 1 records whether the entry is a
`Profile_Manager_obj` instance.

### The debugger logpoints

Both sessions attach the headless debugger to the running game after
`mark1probe hook` and the game's own control, and log hits at script entries
without stopping the game. The precondition is that the toolkit's decompile
index still names the build the Ghidra project imported; otherwise the
locations are stale and the session is `LIVE-ABORTED`. The locations come
from that project and stay in the session's local notes, never in this doc.

- `dbg-control`: `CheckTalentUse`'s entry, counting hits. Its hits rise while
  the player stands still. It is the control for every other logpoint in the
  same session.
- `dbg-cast-speed` (Live 1): `TalentUseSetSpeed`'s entry, counting hits and
  recording where each call returns to.
- `dbg-stat-movement` and `dbg-stat-jump` (Live 2): the entries of
  `StatMovementSpeed` and `StatJumpPower`, the same way.

`mark1probe hook` detours these entries itself, so the logpoints are armed
without matching the file's bytes, and the first bytes each logpoint reports
are recorded in the capture. A candidate whose first byte differs from the
control's is `not-run (instrument: bytes)`. Return locations are written in
the capture as module offsets only; the record names their callers in words.
The debugger is detached before the game is stopped.

## Live procedure 1

The cast: its route, whether the game still gates it, which input belongs to
each slot key, the toggle outlier and the cast's movement penalty. This is
the procedure the session runs, written here before it runs. The capture is
the workorder's live capture 1.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  branch's head once this procedure is written. The owner is asked before it
  is installed, and its SHA-256 is recorded.
- **Character:** save slot 14, "Sorak" (White Mage), in `Town_01_rm`, with no
  flight charm worn (a worn flight charm arms flight's hooks). The saves are
  backed up first and restored at the end.
- **Control:** `ping` answers `pong (YYTK 4.0.1)`; bare `mark1probe` answers
  `mark1probe: research build`, then `rows=9 hooked=0 held=0`.
- **Cases:** `skillstate` is read first. The ordinary case is the first of
  bar slots 0,3 to 0,6 whose skill shows an `effect=` count and is not a
  toggle (Mana Orb on 0,3 when the character was last read). The toggle
  outlier is Soul Spurn on any of those slots (0,5 when last read); without
  it, `toggle-flips` is `not-run`. The empty-slot outlier is any of them with
  talent id 0; without one, `empty-slot` is `not-run`. No other skills.
- **Relies on:** `CheckTalentUse` and `TalentUse` (the toolkit's
  `docs/RUNTIME_DATA_MODELS.md` § 7.2 and § 7.7); `TalentUseSetSpeed`
  (`skill-sliders-research.md` § Static reading, and § 7.8);
  `PlayerMovement` (§ 19.6); `inputState` (§ 10.4); `playerProfileSlot`,
  `abilityQueue`, `mobileTalentPressed` and `stackSkillList` (§ 7.7, a static
  reading); `UI_Hud_Talent_obj`'s `row0` and `skillstate` (§ 8.3,
  `skill-actions-research.md` § Decision); `White_Mage_Soul_Spurn_AOE_obj` as
  Soul Spurn's on-state (§ 7.2, and the toggle research).
- **Debugger:** attached at S3, after S2's `hook` and the game's own control,
  then its module list read; `dbg-control` armed first, `dbg-cast-speed`
  before S15 (§ Instrument, "The debugger logpoints"). Detached in S18,
  before the game stops.
- **Keys:** pressed through hs-drive's key input, one second apart where a
  step presses several.

**Checks.** `dll-hash`, `marker`, `control`, `hook-native` and `dbg-control`
establish that the session is valid, and must pass. Each part's own control
gates its research checks: `bar-read` and `key-cast-control` gate S6 to S13,
and `walk-control` gates S15 to S17. When a part's control fails, its
research checks are `not-run (instrument: <control>)`, never `fail` or
`not-observed`. `cast-input` or `cast-other` must have cast for S10, S12 and
S13 to run; when neither cast they are `not-run (instrument: cast-route)`,
because with no lever that casts, their counts measure the lever, not the
game. For the rest, a `fail` or a `not-observed` is itself the finding, as
long as its control passed in the same session.

| Step | Check | What is done | Expected |
| --- | --- | --- | --- |
| S0 | `dll-hash` | the installed `BloodPactPlugin.dll`'s SHA-256 | equals the built research DLL's |
| S1 | `control`, `marker` | `ping`, then bare `mark1probe` | as **Control** above |
| S2 | `hook-native` | `mark1probe hook` | `9 detoured, 0 failed, 0 held` |
| S3 | `dbg-control` | attach the debugger; arm `dbg-control` | its hits rise while the player stands still |
| S4 | `bar-read` | `skillstate`; pick the cases | the bar is listed |
| S5 | `key-cast-control` | cast the ordinary slot by its key (hs-drive's skill cast with the Q key, 81, when it is the Q slot) | `confirmed: true` |
| S6 | `ctu-args` | `mark1probe args 120` while idle, then again while S5's key is pressed | which argument changes with the slot, and its value for the pressed slot; the values this machine passes |
| S7 | `slot-keys` | `mark1probe watch input 90`, then press Q, E, R and Y one second apart | one `input[<i>]` change per key, with its pressed and resting values |
| S8 | `queue-observed` | `mark1probe watch queue 120`, then press the ordinary key twice, 10 frames apart | what `abilityQueue`, `mobileTalentPressed` and `stackSkillList` hold during a cast, or no change (read with § Instrument, "Where the `profile` and `queue` scopes read") |
| S9 | `cast-input` | once the ordinary skill's `effect=` reads 0: `mark1probe set profile.inputState[<its i>] <S7's pressed value> during CheckTalentUse 600`, no key pressed; `skillstate` after two seconds | `effect=` rises; `show` counts `applied=` above 0 and a cast of that talent |
| S10 | `cast-gate` | with the lever that cast: S9's (`profile.inputState[<S7's i>]`), or else S11's (`queue.abilityQueue[i]` or `queue.mobileTalentPressed[i]` at the i S11 mapped to that slot), run again for 600 frames on the ordinary slot, no key pressed; during that window, `show` every two seconds, and `mark1probe watch player 120` | that talent is cast no more often than its cooldown allows, and a mana member falls with each cast |
| S11 | `cast-other` | only if S9 did not cast: for each i in 0..11, one at a time with no key pressed, `mark1probe show reset`, `mark1probe set queue.abilityQueue[i] 1 during CheckTalentUse 60`, then `mark1probe show`, recording `applied=` and the `casts=` talent id for each i; then the same sweep for `queue.mobileTalentPressed[i]`; then a `set` of each `profile` or `player` member S8 showed moving, one at a time | which i casts which bar slot (the slot-counter mapping); the queue route reads as a negative only if no i cast while its `applied=` was above 0 |
| S12 | `toggle-flips` | toggle outlier: `mark1probe count White_Mage_Soul_Spurn_AOE_obj 600`, with the lever that cast: S9's (`profile.inputState[<S7's i>]`), or else S11's (`queue.abilityQueue[i]` or `queue.mobileTalentPressed[i]` at the i S11 mapped to that slot), on Soul Spurn's slot for 600 frames | the on and off flips, counted from the aura's instance count; `show` counts `applied=` above 0 and at least one `casts=` of Soul Spurn's talent id, else `not-run (instrument: lever-not-on-slot)`; `stays` only when `show` counts at least two `casts=` of Soul Spurn's talent id in the window while the aura's count rose and then held; one cast is `not-run (instrument: single-cast)`; record the `casts=` count beside the flip count |
| S13 | `empty-slot` | empty-slot outlier: the lever that cast: S9's (`profile.inputState[<S7's i>]`) on the empty slot's index for 120 frames, or else S11's lever (`queue.abilityQueue[i]` or `queue.mobileTalentPressed[i]`) at the empty slot's i, only when the other slots' mapped i values fix it unambiguously; otherwise `not-run (instrument: empty-slot-index)` | `applied=` above 0 and no `TalentUse` call |
| S14 | `walk-control` | `mark1probe pos 240` while hs-drive holds D for three seconds; once more | the two totals within 10% of each other |
| S15 | `penalty-baseline`, `dbg-cast-speed` | arm `dbg-cast-speed`; `pos 240` while D is held for three seconds and E is pressed at one second | the total against S14's; `dbg-cast-speed`'s hits and return locations |
| S16 | `penalty-vars` | `mark1probe watch player 90 after TalentUse`, then `watch pst 90 after TalentUse`, each during an E press in the middle of a walk | the members that change after the cast and settle back |
| S17 | `penalty-lever` | for each S16 member: `set player.<name> <its resting value> during PlayerMovement 240` (or the `pst` member), then S15's walk | the total back near S14's |
| S18 | none | `mark1probe off`; detach the debugger; stop the game; restore the backup | a clean restore |

A mover that `pos` reports as `outside` means the player moved with no row's
call moving it: the row that moves the walk is not among the nine.

The checks feed § Decision: `cast-route` from S9 and S11 (with S8),
`cast-gate` from S10, `slot-keys` from S7, `toggle-under-automation` from S12,
`cast-penalty` from S16 and `penalty-lever` from S17.

## Live procedure 2

Walking speed, the jump and walkability. The capture is the workorder's live
capture 2. It runs after Live procedure 1, in the same sitting or later,
with its own backup: its starting state is the save as it was before Live 1.

**Setup:**

- **Build, character, control and debugger precondition:** as Live
  procedure 1. The session starts in town with `jumpscenery` off.
- **Cases:** a walk on open ground; a jump on open ground with the cursor
  about 100 px away (inside the jump's measured reach of about 246 px, the
  toolkit's `docs/RUNTIME_DATA_MODELS.md` § 19.5) and about 300 px away
  (beyond it); walkability at a point of open floor, at a point inside a prop,
  and along a line from the town's walkable floor out past its edge.
- **Relies on:** `PlayerMovement` (§ 19.6); `ReturnSpecificStat`'s cases 64
  and 66 and `StatMovementSpeed` (§ 19.10, a static reading); `skillsLeap`,
  `playerJumpGravity` and `StatJumpPower` (§ 19.1); the jump's cursor read
  (§ 19.5); `global.pathGrid` (§ 22.7); `Collision_Parent_obj` and
  `Wall_Parent_obj` (§ 19.2, § 19.8); the edge (§ 19.11); `playerwarp` (the
  module guide's Command Reference) to stand at a point.
- **Debugger:** as Live 1, with the same control, `dbg-control`; the
  candidates are `dbg-stat-movement` and `dbg-stat-jump` (§ Instrument, "The
  debugger logpoints"), under the same bytes rule.

**Checks.** The same five validity checks must pass. `walk-control` gates T5
to T7, `jump-control` gates T9 to T11 and `walkable-control` gates T13 and
T14, the same way as in Live procedure 1.

| Step | Check | What is done | Expected |
| --- | --- | --- | --- |
| T0 to T3 | `dll-hash`, `control`, `marker`, `hook-native`, `dbg-control` | as S0 to S3 | as there |
| T4 | `walk-control` | `mark1probe pos 240` while hs-drive holds D for three seconds, the inventory closed; twice | the two totals within 10% |
| T5 | `walk-mover`, `dbg-stat-movement` | arm `dbg-stat-movement`; one T4 walk; `pos` lists the movers | which row moved the player; the stat's calls and return locations |
| T6 | `speed-move` | `mark1probe move <T5's mover> 0.5 240`, then T4's walk | a total about half of T4's |
| T7 | `speed-stat` | `mark1probe stat 64 0.5 240`, then T4's walk | the total against T4's, with the lever's `changed-dispatch=` and `changed-getter=` |
| T8 | `jump-control` | a jump (Space, key 32) on open ground with the cursor about 100 px away, `pos 150` armed; twice | two lengths within 10%; the cursor printed |
| T9 | `jump-mover`, `dbg-stat-jump`, `jump-vars` | arm `dbg-stat-jump`; T8's jump; then `watch pst 110 after skillsLeap` over a second jump | the movers; the stat's calls; the members that hold the jump's state |
| T10 | `jump-move` | `move <T9's mover> 3 150`, T8's jump, then the same with the cursor 300 px away | the lengths against T8's and against the far jump's baseline |
| T11 | `jump-stat` | `stat 66 3 150`, the same two jumps | the lengths against the baselines |
| T12 | `walkable-control` | `mark1probe walkable here 32 0` on open floor | `room=in grid=free family=free wall-line=clear` |
| T13 | `walkable-prop` | `walkable here` at a point inside a prop | which fields read blocked |
| T14 | `walkable-edge` | `playerwarp` to the town's walkable edge; `walkable scan <degrees toward the edge> 16 24`, with a screenshot | where each field flips, against the edge seen on screen |
| T15 | none | `mark1probe off`; detach the debugger; stop the game; restore the backup | a clean restore |

The checks feed § Decision: `walk-mover` from T5, `speed-lever` from T6 and
T7, `jump-mover` from T9, `jump-lever` from T10 and T11, and `walkable-test`
from T13 and T14.

## Results

Live 1 and Live 2 ran on 2026-10-13, one after the other, on one research DLL
(sha256 `0557663379cfbb08d1a976896f1b79712eae72dff47138e8f38194c08b02b97f`,
plugin v2.2.1 banner, YYTK 4.0.1), built with `plugin_build\build.bat dev`
from ForgePact `2feb9b4`, whose `plugin/` tree is unchanged at the head of
the `166-mark1-set` branch. Save slot 14 "Sorak" (White Mage), in
`Town_01_rm`, on the game build whose executable hashes to
`498d588550d80d0d100a4ce6958bcc044e6b79339920d3d8a931bca9a1b142c0` (the copy
the Ghidra project imported, so the debugger logpoints belong to this build).
No flight charm was worn and no config file existed, so every shipped mod was
off. The records are the session captures
`forgepact-166-mark1-set-live-1.md` and `forgepact-166-mark1-set-live-2.md`,
kept with the hub's workorder files on the researcher's machine and not
tracked. Every verdict below is what the toolkit's `tools/live_checks.py`
reads from them. Each session restored its own save backup at teardown and
read it back clean. Live 3, walkability only, ran later the same day on a
rebuilt research DLL (§ Live 3 results).

### Live 1 results

The cast, the slot keys, the toggle and the cast's movement penalty (Live
procedure 1, 18:46 to 18:56 UTC). `tools/live_checks.py` reads 20 checks from
`forgepact-166-mark1-set-live-1.md`: 15 pass, none fail, 3 not-observed, 2
not-run. The five validity checks passed, and so did each part's own control
(`bar-read` and `key-cast-control` for the cast, `walk-control` for the
penalty), so each not-observed below is a negative of its part within what
the instrument reads, not a broken instrument.

Three things were done differently from the procedure, each recorded in the
capture: `args` takes at most 100 calls, so S6 ran `args 100` (not 120); S7
used one 90-frame `watch input` per key, since one window cannot span four
presses a second apart; and one of S14's walks was stopped by a building, so
the player was moved back and walked again.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, then `rows=9 hooked=0 held=0`. |
| `hook-native` | pass | `mark1probe hook` detoured all nine rows; none failed and none was held. |
| `dbg-control` | pass | The x64dbg logpoint on `CheckTalentUse`'s entry kept logging while idle and up to teardown, and the debugger's control hold proved the attach: the game's frame counter stopped answering during the hold and moved on after it. |
| `bar-read` | pass | `skillstate` listed the bar: Chain of Holy Light on 0,2, Mana Orb (talent 253) on 0,3, Healing Zone (252) on 0,4, Soul Spurn (240) on 0,5, and nothing on 0,6. The cases: Mana Orb ordinary, Soul Spurn the toggle, 0,6 the empty slot. |
| `key-cast-control` | pass | hs-drive's Q press on slot 0,3 cast Mana Orb (`effect=` went from 0 to 1). |
| `ctu-args` | not-observed | Over 100 local `CheckTalentUse` calls idle, and 100 more twice with Q held down, every call carried the same three arguments: the player (as a `VALUE_REF`), 1 and 1, one call per frame. No argument names a slot, as the static reading said. Holding Q down cast nothing in either window, so the value for a pressed slot was not seen. |
| `slot-keys` | pass | Each key moved one entry of the profile slot's `inputState`: Q entry 3, E entry 4, R entry 5 and Y entry 6 (entries 26 and 25 moved together with Q and E). Each entry rests at 1, reads 4 on the press frame, 3 while held, 2 on the release frame and then 1 again. The entries are the bar columns the four slots sit on. The profile slot read was entry 1 of `Input_Device_Manager_obj` instance 0's `playerProfileSlot`, the index `CheckTalentUse`'s third argument names. |
| `queue-observed` | pass | Over two Q presses ten frames apart (one cast counted), none of the first four entries (0-3) of `abilityQueue`, `mobileTalentPressed` or the player's `stackSkillList` changed between end-of-frame samples; later slots, and an entry set and cleared within one gate call, were not visible, so whether a key cast uses the queue is not established. |
| `cast-input` | pass | With no key pressed, `mark1probe set profile.inputState[3] 4 during CheckTalentUse 600` cast Mana Orb: `effect=` went from 0 to 1, the lever had applied 210 times by the first `show`, and one Mana Orb cast was counted. Writing the press value into a slot's entry just before the gate reads it, and restoring it after, casts that slot. |
| `cast-gate` | not-observed | Across more than 480 applied frames of that lever, with the press value in place on every gate call, Mana Orb was cast once. What refused the repeats (cooldown, mana, the orb still alive) and Mana Orb's cooldown were not read, so whether the cooldown applies to a forced press is not established. The mana half was not seen: in a 120-frame watch of the player no member named for mana fell (`manaArrayPos` moved, upwards). Where the player's mana is kept was not read. |
| `cast-other` | not-run | Not needed: `cast-input` cast. |
| `toggle-flips` | pass | The same lever on Soul Spurn's entry 5 for 600 frames: the count of `White_Mage_Soul_Spurn_AOE_obj` went from 0 to 1 and back 13 times, ending at 0 (off), and 22 Soul Spurn casts were counted. A toggle held "pressed" this way switches on and off again every 10 to 30 frames; it does not stay on. |
| `empty-slot` | pass | The lever on the empty slot's entry 6 for 120 frames: no `TalentUse` call at all. |
| `walk-control` | pass | Two three-second walks to the right (D) from the same spot covered 1049.3 and 980.5 px in the 240-frame window, 6.6% apart. Every walk's movement came from outside the nine rows (`in-rows=0`). |
| `penalty-baseline` | pass | The same walk with E (Healing Zone) pressed after one second covered 949 px, 6.5% below the two controls' mean. From the cast the step per frame fell from about 6.0 px to about 3.9 px for about 21 frames, then climbed back to 6.0 px. |
| `dbg-cast-speed` | pass | The logpoint on `TalentUseSetSpeed`'s entry (its first byte the same as the control's, so the same instrument) logged 17 hits by teardown, each returning to one location, which lies in `TalentUse` (below). The probe's row counted one local call during the walk's cast. |
| `penalty-vars` | not-observed | A 90-frame watch of the player from the cast on saw `playerSequenceTopPrev`, `playerSequenceBottomPrev`, `playerSequenceDirection` and `manaArrayPos` change, and none settled back. The `pst` watch read nothing: `pSt` is an array of 54 entries, neither a struct nor an instance, and the probe's `pst` scope reads named members only. |
| `penalty-lever` | not-run | No member to hold back: `penalty-vars` listed none that settles back. |

**The caller behind `dbg-cast-speed`.** Every logged return location of the
`TalentUseSetSpeed` logpoint was the same one. Looked up in the Ghidra project
through the ghidra MCP, it lies past `TalentUse`'s entry and before the next
function entry the project knows, named or unnamed, and the five bytes before
it in the game file are a direct call to `TalentUseSetSpeed`. That is the one
direct call site the static reading found in `TalentUse` (§ Static reading,
"What a cast does to movement"). One caller, `TalentUse`, 17 hits;
`CA_playerTalentActive`, the other direct caller the reading found, was not
observed in this session.

### Live 2 results

Walking speed, the jump and walkability (Live procedure 2, 18:58 to 19:11
UTC). `tools/live_checks.py` reads 19 checks from
`forgepact-166-mark1-set-live-2.md`: 13 pass, 1 fails, 1 not-observed, 4
not-run. The five validity checks passed, and so did `walk-control` and
`jump-control`. `walkable-control` failed, so the two walkability checks it
gates did not run.

Recorded deviations: T4's `pos` window and its D press had to go out in one
batch (two windows sent first expired before the press and read 0, not
counted); T5's second per-frame logpoint slowed the frame, so that logpoint
was cleared before T7; T8's window was lengthened to `pos 300` after two
`pos 150` windows ended mid-jump (not counted), and T10's and T11's levers ran
for 300 frames instead of 150 for the same reason; T12 read two extra points
and one more spot beside the control point.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The same research DLL as Live 1. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, then `rows=9 hooked=0 held=0`. |
| `hook-native` | pass | All nine rows detoured, none failed or held. |
| `dbg-control` | pass | As in Live 1: the `CheckTalentUse` logpoint logged while idle and to teardown, and the control hold proved the attach. |
| `walk-control` | pass | Two three-second walks to the right from the same spot: 932.7 and 855 px, 8.3% apart, both windows ending with D still held. All movement came from outside the nine rows. |
| `walk-mover` | pass | During a walk, `PlayerMovement` ran 810 times (once a frame) and no call of any of the nine rows moved the player: `pos` counted every moving frame as `outside`. The walk's displacement is applied somewhere none of the nine rows covers. |
| `dbg-stat-movement` | pass | The logpoint on `StatMovementSpeed`'s entry (first byte as the control's) logged one hit for each `CheckTalentUse` hit, so once a frame, standing or walking: 2008 hits, each returning to one location, which lies in `ReturnSpecificStat` (below). The probe's row agrees: 810 local calls in the 810 frames of the walk's window, none called directly. The second per-frame logpoint slowed the frame from about 13.4 ms to 23.2 ms and the step per frame grew with it (about 9 and 12 px against 6), so this walk's total is not compared with T4's. |
| `speed-move` | not-run | Instrument: no row moved the walk, so there was no row for `move` to scale. |
| `speed-stat` | pass | `mark1probe stat 64 0.5 240` halved id 64's result on all 240 dispatcher calls (`changed-dispatch=240`; `changed-getter=0`, `skip=240`: every `StatMovementSpeed` call inside the dispatcher returned a non-number to the probe, so the getter route never scaled, against the core's tested model). The walk covered 696.9 px, 22% below T4's mean of 893.9 and below the 878.9 px of a walk without the lever straight after. The step per frame fell from 5.979 px to 4.693 px: 0.785 of the unscaled step, not 0.5. The walk does read movement speed through the dispatcher every frame, but halving the dispatcher's id-64 answer gave 0.785 of the step. |
| `jump-control` | pass | Two jumps on open ground with the cursor about 100 px away (100.95 and 100.31 px as printed) both covered 96.9 px. |
| `jump-mover` | pass | The jump was moved by `skillsLeap` alone: 102 calls in a jump, every one a moving call, and nothing outside the rows. `playerJumpGravity` ran 101 times in a jump and never moved the player. |
| `dbg-stat-jump` | not-observed | The logpoint on `StatJumpPower`'s entry (first byte as the control's) logged nothing over two jumps, nor over the four jumps of T10 and T11, while the control kept logging; the probe's row counted 0 calls too. A jump does not call `StatJumpPower`, as the toolkit's § 19.1 measured with another instrument. |
| `jump-vars` | not-run | Instrument: `pSt` is an array of 54 entries and the `pst` scope reads no members of it. |
| `jump-move` | pass | `mark1probe move skillsLeap 3 300` tripled the displacement of all 102 `skillsLeap` calls of a jump. With the cursor about 100 px away the jump covered 290.7 px against 96.9 px (3.00 times); with it about 300 px away, 738.8 px against an unscaled 250.3 px (2.95 times), so the scaled jump goes well past the jump's usual reach. |
| `jump-stat` | pass (the lever never engaged) | `mark1probe stat 66 3 300` found no call to scale (`changed-dispatch=0`, `changed-getter=0`). The jumps covered 96.9 px with the cursor about 100 px away and 246.3 px with it about 300 px away (250.3 without the lever). Jump Power is not read during a jump. |
| `walkable-control` | fail | On open floor the player stood on and walked across, `mark1probe walkable here 32 0` read `room=in grid=free family=blocked wall-line=clear`. `family` read `blocked` at the player's own position too, 32 px to either side, and at a second open spot. So the `family` field, the player's `place_meeting` against `Collision_Parent_obj` as the probe asks it, does not tell open floor from anything else; why it reads blocked there is not established. |
| `walkable-prop` | not-run | Instrument: `walkable-control` failed. |
| `walkable-edge` | not-run | Instrument: `walkable-control` failed. |

**The caller behind `dbg-stat-movement`.** Every one of the 2008 logged return
locations was the same. Looked up the same way as Live 1's, it lies past
`ReturnSpecificStat`'s entry and before the next function entry the Ghidra
project knows, and the five bytes before it in the game file are a direct
call to `StatMovementSpeed`: the dispatcher's case for id 64, the only direct
caller the static reading found. One caller, `ReturnSpecificStat`, 2008 hits.
`dbg-stat-jump` logged no hit, so it has no caller to name.

Locating a return by the nearest function entry before it can be wrong when
an object event the symbol dump does not name sits in between (the flight
charms research found that once, `flight-charms-research.md`, Live 1
results). Here each location sits right after a direct call to the logged
script, and that call is the one the static reading found in the named
script, so the two agree.

### Live 3 results

Walkability only (Live procedure 3, 20:59 to 21:03 UTC). It ran because
Live 2's `walkable-control` failed, after ForgePact `61243b2` corrected how
the probe reads `place_meeting`'s answer and made the walkable line print
each builtin's raw answer and what a blocked `family` met (§ Instrument,
"The walkable verdict"). Research DLL sha256
`8c47459b15dd0691336efd65e5c9ded6ebd1a46b23e578e9688f71d023ad516e`, built
with `plugin_build\build.bat dev` from `61243b2`, plugin v2.2.1 banner, YYTK
4.0.1, on the same game build. Same character, room and starting save as
Live 1 and Live 2, no flight charm worn, `jumpscenery` off; no debugger and
no `mark1probe hook`, since the walkable read hooks nothing.

The procedure, in the order of Live procedure 2's T12 to T14: `walkable
here 32 0`, `here 0 0` and `here -32 0` on open floor by the town's
waypoint (`walkable-control`, which gates the other two); points inside a
prop (`walkable-prop`); and, after `playerwarp` to walkable floor near the
town's south edge, `walkable scan 270 16 24` toward that edge, with a
screenshot (`walkable-edge`). The record is the capture
`forgepact-166-mark1-set-live-3.md`, kept beside the other two and not
tracked. `tools/live_checks.py` reads 6 checks from it: all pass, the two
research checks as recorded. The session restored its own save backup and
read it back clean.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL hashed to the built research DLL above. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, then `rows=9 hooked=0 held=0`. |
| `walkable-control` | pass | At the player's own position on open floor and 32 px to either side, every point read `room=in grid=free family=free wall-line=clear`, each with `grid-raw=real:0 family-raw=real:0 wall-line-raw=ref:-4`. `place_meeting`'s "no meeting" reached the probe as a `real` 0, not a `bool` false, so Live 2's `family=blocked` everywhere was the old read counting that 0 as a hit, not anything at the points. |
| `walkable-prop` | pass (research: recorded) | Inside the Bounty Board (`Bounty_Board_obj`), on a scan west of the player and at two points read with `walkable at`, only `family` read blocked (`family-raw=real:1`, `met=Bounty_Board_obj`); the path grid's cell stayed free (`grid-raw=real:0`) and the wall line clear. The board blocks about 64 px of that line, five points 16 px apart, at its base; three points picked on its drawn sprite above the base read free on every field. Further along the same line, `family` met an `Invisible_Wall_Not_Minimap_obj` one point before the grid cell turned blocked and the wall line crossed. |
| `walkable-edge` | pass (research: recorded) | From walkable floor south of the town, scanning south in 16 px steps: `family` turned blocked first, meeting an `Invisible_Wall_Not_Minimap_obj`; one point (16 px) later the grid cell read blocked (`real:-1`) and the wall line crossed (its answer a `ref` to that same instance); all three stayed blocked to the end of the scan, still inside the room. The edge is that invisible wall, and nothing on screen draws it. A coarse scan from the waypoint at 96 px steps, past the room's bottom, showed the rest of the way: water (`Block_Dynamic_Sprite_obj`) and fallen rocks and trees (`Fall_Rock_01_obj`, `Fall_Dead_Tree_02_obj`) read blocked on both `grid` and `family` with the wall line clear; open pockets between them read free on every field; and past the room's bottom (`room=out`) the grid stayed blocked and the wall line crossed, but `family` read free again once the points were past the invisible walls. |

What the session settles about the four fields, in town:

- **`family` is the one field that sees a prop.** The Bounty Board blocked
  `family` alone. Why its cells read free in the path grid, when the static
  reading has the grid built from every `Collision_Parent_obj` instance, is
  not established.
- **At the edge `family` leads.** It turned blocked 16 px before the grid
  and the wall line, at the invisible wall the other two then saw too.
- **`family` says nothing past the room.** Beyond the room's bottom it read
  free; only `room` (and the grid) still said no.
- **None of the four tile layers read (`Darkness`, `Hay_Ground`,
  `Leaves_Ground`, `Water`) marks the edge; the room's later tile layers were
  not read.** The layers' values vary across open
  floor, the `Water` layer is non-zero only on the water, and every layer
  reads -1 outside the room.

## Decision

Recorded on 2026-10-13 from the three sessions (§ Results). Each key takes
exactly one of its listed values, on a line of its own, and the check that
decides it is on the line below. A `not-observed` closes nothing unless its
positive control passed in the same session; each one below says whether it
did.

cast-route: input
Decided by `cast-input` (Live 1, pass): writing the press value 4 into the slot's `inputState` entry during the gate's own call cast Mana Orb with no key pressed, and the same lever cast Soul Spurn (`toggle-flips`). `queue-observed` saw the queue arrays unchanged by a key cast; the `queue` and `mobile` levers were not tried (`cast-other` not-run, as the input route had cast).

cast-gate: not-observed
Decided by `cast-gate` (Live 1, not-observed; its control `key-cast-control` passed): one Mana Orb was cast in more than 480 forced frames, with what refused the repeats not read, and an empty slot cast nothing (`empty-slot`), but no mana member was seen to fall, so whether the gate still charges mana is open.

slot-keys: measured
Decided by `slot-keys` (Live 1, pass): Q, E, R and Y are the profile slot's `inputState` entries 3, 4, 5 and 6, resting at 1, 4 on the press frame, 3 while held and 2 on the release frame.

toggle-under-automation: flips
Decided by `toggle-flips` (Live 1, pass): Soul Spurn held "pressed" through the lever switched on and off 13 times in 600 frames, ending off.

cast-penalty: not-observed
Decided by `penalty-vars` (Live 1, not-observed; its control `walk-control` passed): the penalty itself was measured (`penalty-baseline`: the step fell to about 0.65 of itself for about 21 frames after the cast), but no player member that changes at the cast and settles back was found, and `pSt` could not be read by the watch.

penalty-lever: not-run
Decided by `penalty-lever` (Live 1, not-run): there was no member to hold back.

walk-mover: outside-rows
Decided by `walk-mover` (Live 2, pass): `PlayerMovement` runs once a frame during a walk, but no call of any of the nine rows moved the player.

speed-lever: stat
Decided by `speed-stat` (Live 2, pass): x0.5 on id 64's result at the dispatcher slowed the walk, to 0.785 of its step rather than 0.5, so the stat route reaches the walk but does not halve it; `speed-move` did not run (instrument: no row moves the walk). Only the dispatcher's answer was scaled: the getter route is an open instrument question for the choice of lever, since every `StatMovementSpeed` call inside the dispatcher returned a non-number to the probe (`skip=240`, `changed-getter=0`), so a lever at the getter is untested, not ruled in or out.

jump-mover: skillsLeap
Decided by `jump-mover` (Live 2, pass): all 102 moving calls of a jump were `skillsLeap`'s; `playerJumpGravity` ran alongside and never moved the player.

jump-lever: move
Decided by `jump-move` (Live 2, pass): x3 on `skillsLeap`'s displacement tripled the jump (3.00 times near, 2.95 times far); `jump-stat` showed the stat route never engages, since a jump does not read Jump Power (`dbg-stat-jump` not-observed with the control logging).

walkable-test: combined
Decided by `walkable-prop` and `walkable-edge` (Live 3, both recorded; their control `walkable-control` passed): a point is a walkable landing when it lies inside the room and the player placed there meets nothing of `Collision_Parent_obj` (`room=in` and `family=free`). `family` alone saw the prop, which the path grid and the wall line missed, and turned blocked at the town's edge 16 px before them, but read free again past the room's bottom, where only `room` and the grid still said no; none of the four tile layers read marks the edge (the later layers were not read). Measured in a town only (§ Not established). Live 2's `walkable-control` had failed on the old read of `place_meeting` (§ Live 3 results).

The values each key may take (Live 1 decides the first six, Live 2 the next four, Live 3 the last):

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

Settled by the sessions, and so off this list: which `inputState` value
means pressed (4, on the press frame); which entries Q, E, R and Y use (3 to
6); what `CheckTalentUse`'s arguments are on this machine (the player, 1 and
1, once a frame); that a write into the slot's entry inside the gate's call
casts the slot; which script moves the
player during a jump (`skillsLeap`); why Live 2's walkable control failed
(the probe read `place_meeting`'s `real` 0 as a meeting); and, in town, how a
walkable landing point is told from one past the edge (inside the room, and
`family` free). Still open:

- **Whether the gate still charges mana** for a cast made through the input
  lever (`cast-gate`, not-observed: no mana member was found to watch), and
  whether it checks the other requirements `TalentRequirement` covers.
- **Whether the cooldown applies to a forced press, and Mana Orb's
  cooldown.** Over more than 480 forced frames Mana Orb was cast once
  (`cast-gate`), but what refused the repeats (cooldown, mana, the orb still
  alive) was not read.
- **The other bar columns.** Only the entries of Q, E, R and Y (3 to 6) were
  measured; how the gate derives the entry for the other columns, and why
  entries 26 and 25 move with Q and E, were not read. Whether a slot's
  `abilityQueue` or `mobileTalentPressed` entry, or `InputSimulate`, would
  also cast it was not tried.
- **What holds the caster back during a cast.** The penalty was measured
  (`penalty-baseline`), but no player member holding it was found
  (`penalty-vars`), `pSt` is an array the probe's watch cannot read, and what
  each `playerEffect` entry `TalentUseSetSpeed` writes means was not read. So
  where a lever could take the penalty away is open (`penalty-lever`
  not-run).
- **Where the walk's displacement is applied, and why x0.5 on movement speed
  gives 0.785.** No row moves the walk (`walk-mover`), and halving id 64 at
  the dispatcher slowed the step to 0.785 of itself (`speed-stat`). Three
  questions are open: whether the step is not proportional to the
  dispatcher's id-64 answer (a base the stat adds to, a cap or a floor);
  whether the walk also reads movement speed by a route the dispatcher lever
  does not reach, such as the cached `pSt` values `PlayerMovement` reads; and
  why `StatMovementSpeed`'s own result was not a number to the probe
  (`skip=240`), which is an open instrument question for the choice of speed
  lever. Which value the final step is computed from was not read; the
  factor that gives an exact 0.5 is not known.
- **Where the jump's length is decided.** Scaling `skillsLeap`'s own
  displacement tripled the jump (`jump-move`), and a jump does not read Jump
  Power (`jump-stat`, `dbg-stat-jump`), but how `skillsLeap` turns the
  cursor distance into a step, and the jump's state members (`jump-vars`
  not-run: `pSt` unreadable), were not read. A tripled jump's landing check
  was not measured: the walkable test was read by command at chosen points,
  never at a jump's landing.
- **Whether `playerSkipCollisions` lets the player pass collision props**,
  and who sets it.
- **The walkable test outside a town.** `walkable-test: combined` (inside
  the room, and `family` free) was measured in `Town_01_rm` only, at one
  prop and one stretch of its south edge, an invisible wall. Whether a
  generated zone marks its edge the same way, with a `Collision_Parent_obj`
  instance, or only in the path grid through the zone generator's outside
  rectangles, was not read live; nor whether `objZoneGenV2` exists in a town,
  and so whether `IsObtainablePlace` says anything there.
- **Why the path grid missed the Bounty Board.** The board blocked `family`
  but its cells read free in `global.pathGrid` (`walkable-prop`), against the
  static reading that the grid is built from every `Collision_Parent_obj`
  instance. Whether the board came after the grid was built, or its mask
  covers no cell centre, was not read.
- **Pockets the floor does not reach.** Points between the water and the
  fallen rocks south of the waypoint read free on every field; whether the
  player could walk to them was not checked, and the test does not say.
