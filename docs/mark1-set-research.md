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
- **Phase 2 (the mod):** its research session, Mod live procedure 1, ran on
  2026-10-19 (§ Phase 2: the mod). The walk's step is affine in the speed
  stat, so an answer of (151 - 200) / 2 halves Sorak's walk
  (`speed-route: stat-model`); the cast's slowdown follows
  `playerEffect[199]` only through its ramp, and nothing found removes it
  (`penalty-route: none`). The facts are folded into the same four sections
  of `docs/RUNTIME_DATA_MODELS.md` and into `mod_live_1` of the curated
  measurements.
- **Mod live procedure 2** (the built set with its switch on): run on
  2026-10-19 in two attempts, the second after the worn read was fixed
  (§ Phase 2: the mod, "Mod live 2 results"). The pieces, their art, the
  full-set read, the border and the jump passed; the automation failed,
  because pressing every column let Shadow Bolt keep Heavenly Fire from ever
  casting; and the half walk and the cast's slowdown on it were not observed,
  because the automated casts kept the slowdown on through every window. The
  facts are folded into `docs/RUNTIME_DATA_MODELS.md` (the equipment slots,
  § 7.7, § 7.8, § 19.10, § 19.11 and § 21.7) and into `mod_live_2` of the
  curated measurements.
- **Mod live procedure 3** (the turn rule, and the half walk re-measured with
  Healing Zone alone on the bar): run on 2026-10-20 (§ Phase 2: the mod, "Mod
  live 3 results"). The turns passed: on Mod live 2's own bar Shadow Bolt,
  Heavenly Fire and Healing Zone were each cast with no key, 31, 30 and 3
  times, every landing attributed to its column. The half walk measured 0.500
  of the control, and a cast's 0.65 slowdown stacks on it
  (`penalty-stack: stacks`). `landed-control` failed on one clause only: one
  Healing Zone cast can leave two zone instances. The facts are folded into
  `docs/RUNTIME_DATA_MODELS.md` § 7.7, § 7.8 and § 19.10 and into
  `mod_live_3` of the curated measurements.

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
`family` free). Mod live 3 (2026-10-20) settled two more: a cast's slowdown
stacks on the half walk (`penalty-stack: stacks`, the cast frames' step 0.650
of the bonus-on step), and Heavenly Fire casts by key on Sorak in town with
Shadow Bolt and Healing Zone beside it (Part B's positive control), which Mod
live 2's starvation could not tell from a skill that does not cast there.
Still open:

- **Whether the gate still charges mana** for a cast made through the input
  lever (`cast-gate`, not-observed: no mana member was found to watch), and
  whether it checks the other requirements `TalentRequirement` covers.
- **What unit `abilityCooldown` is in, and Mana Orb's cooldown.** Over more
  than 480 forced frames Mana Orb was cast once (`cast-gate`), but what
  refused the repeats (cooldown, mana, the orb still alive) was not read. Mod
  live 2 showed the game refusing forced presses of Healing Zone between
  casts (`gate-kept`, `cooldown-recast`), so a forced press does wait for
  something, but its recasts came 501 to 1071 frames apart against an
  `abilityCooldown` of 14, which in seconds would be 2016 frames at that
  session's frame rate; what the value counts, and what makes the intervals
  differ, was not read. Mod live 3 widened the spread: with Healing Zone
  alone on the bar its automated recasts came 1575 frames apart in both
  windows, and beside Shadow Bolt and Heavenly Fire 587 to 1627 frames apart.
- **Why one Healing Zone cast can leave two zone instances.** In Mod live 3
  (`landed-control`) a rise of `White_Mage_Healing_Zone_obj`'s count went
  from 0 to 2 at one cast in each of two windows while the probe counted one
  cast, and in Part B four rises stood against three casts, the extra zone
  lasting 88 frames. So the zone count is at least the casts, not equal to
  them; what the second instance is, and when the game makes one, was not
  read.
- **In which order the gate serves the bar slots.** With Q, E and R all
  pressed in every gate call, Heavenly Fire on R never cast while Shadow Bolt
  on Q cast about once in 60 frames (Mod live 2, `on-automation`). Whether
  the gate serves one asking slot a call, and in which order, was not read.
  The turn rule presses one column a call, so the set no longer depends on
  it (Mod live 3, `on-automation`).
- **Why the cast's flat slowdown ran longer under the half walk.** In Mod
  live 3 (`on-penalty`) an automated Healing Zone cast held the step at
  0.650 of the bonus-on step for 76 frames before the 16-frame ramp, against
  Mod live 1's 57 flat frames for a key cast without the set at the same
  frame time. Whether the flat part depends on the walk's speed, the cast's
  route or something else was not read.
- **The gear slots the set does not use.** Mod live 2 measured the helmet at
  gear slot 0, body armour 1, boots 2 and gloves 4 on Sorak; with the off hand
  (9) and the relics (10 to 14) measured earlier, slots 3 and 5 to 8 hold
  nothing measured, and whether another character wears its gloves at 4 too
  was not read.
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

## Phase 2: the mod

Phase 2 builds the set itself (`mark1set`, off by default) on what phase 1
found. Two of its levers were left open by phase 1 (§ Not established): what
halves the walk exactly, and what could take the cast's slowdown away. So
phase 2 opened with one more research session, on `mark1probe` extended for
the walk: eleven builtin rows beside the nine script rows, `statlog`,
`statpin`, `hold`, the `effect`, element-wise `pst` and `builtin` watch
scopes, `pos`'s modal step and mean frame time, and `base` (the toolkit's
ForgePact guide documents each form). Each lever is then picked from what
that session measured (§ Phase 2 decision). The set's switch was never
turned on in it.

### Mod live procedure 1

The walk's speed curve, the cast penalty, the gate's mana, the toggle's
settle time, the piece bases, a generated zone's edge and the debugger's
walk callers. The capture is the workorder's mod live capture 1.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at
  ForgePact `109c908`, DLL SHA-256
  `a2f64cf3889a4333e03c4fb0667045799b5fcb05f91e535c9fc171ff154ea493`. The
  owner was asked before it was installed.
- **Character:** save slot 14, "Sorak" (White Mage), in `Town_01_rm`, later
  the Outskirts of Inoya (`Act_01_01`), reached through the town waypoint's
  map. No flight charm worn, `jumpscenery` off, the saves backed up first and
  restored at the end.
- **Control:** `ping` answers `pong (YYTK 4.0.1)`; bare `mark1probe` answers
  `mark1probe: research build` and a rows line with `hooked=0 held=0`.
- **Cases:** walking right (D) on open ground from one fixed spot, recorded,
  and walking down (S) for the vertical factor; Healing Zone (E, bar column
  4) as the ordinary cast; Soul Spurn (R, column 5) as the toggle. No other
  skills.
- **Walk ratios:** a walk's ratio is its modal step over the walk control's
  mean modal step (the vertical walks over their own two S controls). Modal
  steps are compared only at matched frame time: a walk whose mean frame time
  differs from its control's by more than 10% is repeated, then taken against
  a fresh control pair, and a third mismatch is recorded as not comparable.
  A single step of half or one and a half times the mode is the frame clock,
  not a lever or the penalty (§ Live 2 results).
- **Debugger:** only after every walk measurement, because a per-frame
  logpoint lengthens the frame and moves the modal step (§ Live 2 results).
  The build is first checked against the decompile index's build hash. A
  control logpoint on `CheckTalentUse`'s entry comes first; then logpoints on
  `PlayerMoveSkill`, `PlayerMoveSkillFunc` and `CA_playerMove`, each one's
  bytes compared with the research project's before it is armed, and later
  `CA_playerTalentActive` in place of a candidate with no hit. A hit's return
  location stays in the capture; the callers are named here, never located.

**Checks**, in the order they run:

- `dll-hash`, `control`, `marker` (validity): the installed DLL is the build
  above, and the game and the probe answer as under Control.
- `hook-native` (validity): `mark1probe hook` detours every script row and
  every builtin row the runtime knows, none failed or held.
- `walk-control` (the control for every walk check): two D walks whose modal
  steps lie within 3% and whose frame times lie within 10% of each other.
- `stat-trace`: `statlog 64` over a D walk: about one dispatcher call for id
  64 a frame, its original answer a number, and the rows each call sits in.
- `speed-sweep`: id 64's answer multiplied by 1.5, 0.75, 0.5, 0.25, 0 and
  -0.5 in turn, a walk under each; the ratio of each, and the lever's counts
  showing it changed the dispatcher's answer.
- `speed-half`: the multiplier interpolated between the two whose ratios
  bracket 0.5, and a walk under it: a ratio in 0.47 to 0.53 passes.
- `speed-builtin`: which rows `pos` credits with moving an unlevered walk,
  with `place_meeting`'s calls during it as the builtin rows' control; only
  if a builtin moved the walk, that builtin's displacement halved.
- `speed-factor`: `hSpeedFactor` held at half during `PlayerMovement` on a D
  walk, and `vSpeedFactor` the same on an S walk against two S controls.
- `penalty-control` (the control for the penalty checks): a D walk with E
  pressed after one second shows a run of at least 10 frames below 0.8 of
  the mode.
- `penalty-stat`: `statlog 64` through that walk's cast: whether the answer
  falls at the cast and recovers with the step.
- `penalty-watch`: three such walks under `watch effect`, `watch pst` and
  `watch builtin`, each for 90 frames from the next `TalentUse`: every
  element that changes within 2 frames of the cast and back within 40.
- `penalty-lever`: `statpin 64` if the answer fell; otherwise the watched
  element whose settle is closest to the penalty's end, held at its resting
  value during `PlayerMovement`. It passes when, through the frames the
  penalty covered, the modal step stays at least 0.95 of the mode with no run
  of 3 or more frames below 0.8 of it, and the cast still happens.
- `cast-mana`: `watch pst` on a key cast of E, then on a forced cast (the
  probe writing the press value into the slot's `inputState[4]` during one
  `CheckTalentUse` call): the element that fell on the key cast falls by the
  same amount on the forced one (`charged`), does not (`not-charged`), or no
  element fell (`not-observed`).
- `toggle-settle`: `count` on Soul Spurn's on-state object,
  `White_Mage_Soul_Spurn_AOE_obj`, beside one forced press of `inputState[5]`:
  the frames from the press to the object's appearance; a second pair should
  switch it off.
- `base-control` (the control for `base-check`): `base 0 7` builds a
  helmet with a name.
- `base-check`: `base` for body armour, boots and gloves at base 0, and for a
  class that does not build, bases 1, 2 and 3 in turn: the lowest base that
  builds, per class.
- `zone-control` (the control for `zone-edge`): in the Outskirts, `walkable
  here` 32 px either side of the player and at it, on open floor, every field
  free.
- `zone-edge`: `walkable scan` in steps of 96 px for 60 points, up, down,
  left and right of the arrival point; a 16 px scan across the first point
  where a field turns blocked toward open void; a screenshot.
- `dbg-control` (validity): the control logpoint logs about once a frame, and
  the attach is proven by the game's own frame counter.
- `dbg-walk-movers`: each candidate's hits over a D walk and over the
  `penalty-control` walk with its cast.
- `dbg-cast-active`: `CA_playerTalentActive`'s hits over one E cast made
  standing.

A research part whose control failed is recorded `not-run (control)` for its
checks, never `not-observed`.

### Mod live 1 results

The walk, the cast penalty, the toggle, the bases, the zone's edge and the
debugger (Mod live procedure 1, 2026-10-19, 16:04 to 16:23 UTC).
`tools/live_checks.py` reads 23 checks from
`forgepact-166-mark1-set-mod-live-1.md`: 19 pass, 3 fail (`penalty-watch`,
`penalty-lever`, `toggle-settle`), 1 not-observed (`cast-mana`), none
not-run. The validity checks passed, and so did every part's own control
(`walk-control`, `penalty-control`, `base-control`, `zone-control`), so no
part is `not-run (control)` and each fail or not-observed below is a result
within what the instrument reads.

The session ran at 6.94 ms a frame (about 144 frames a second), against
phase 1's 13.4 ms, and the walk's modal step was 2.989 px, half of phase 1's
5.979 px. So a 240-frame window lasted 1.67 s, too short for a separate input
call to start inside it, and the operator changed four things, each recorded
in the capture. The lever walks held D down across the command, so the lever
and the `pos` window both sat inside the walk. The cast walks ran `pos` for
2400 frames with the input in the following call. The `penalty-lever` hold
ran 1200 frames instead of 150. And the `penalty-stat` cast was forced by the
probe (the press value into `inputState[4]` for one gate call, in the same
command as the `statlog`) instead of keyed, because a `statlog` prints every
frame and the command only returns once it stops. The forced cast gave the
same slowdown as the keyed one. Soul Spurn was left on after
`toggle-settle`; the saves were restored.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, `rows=20 hooked=0 held=0`. |
| `hook-native` | pass | `hook`: 20 detoured, none failed, held or not found. |
| `walk-control` | pass | Two D walks from the town waypoint, modal step 2.989 px both, 6.94 and 6.95 ms a frame. |
| `stat-trace` | pass | 60 dispatcher calls for id 64 in 60 frames, every answer 151, every call outside all 20 rows, and no call of the getter outside the dispatcher. |
| `speed-sweep` | pass | Ratios 1.215, 0.893, 0.785, 0.678, 0.570 and 0.355 for multipliers 1.5, 0.75, 0.5, 0.25, 0 and -0.5; the lever changed all 240 dispatcher answers in each window. |
| `speed-half` | pass | The interpolated multiplier, -0.162 (an answer of about -24.5), gave a modal step of 1.495 px: a ratio of 0.500. |
| `speed-builtin` | pass | `pos` credited no row with the walk's movement; `place_meeting` was called 8270 times by the player during it, and no `mp_*`, `move_*` or `motion_*` row was called at all. No builtin to halve, so its second half did not run. |
| `speed-factor` | pass | Holding `hSpeedFactor` (D walk) and `vSpeedFactor` (S walk) at half during `PlayerMovement` left both ratios at 1.000, with the hold applied on all 240 frames. |
| `penalty-control` | pass | After the E cast the step fell to 1.943 px (0.650 of the mode) for 57 frames, then climbed back over 16 frames; 64 consecutive frames were below 0.8 of the mode. |
| `penalty-stat` | pass | Id 64's answer did not fall: 151 on all 120 frames through the slowdown and its recovery. |
| `penalty-watch` | fail | `playerEffect[199]` fell from 1 to 0.65 on the cast frame and rose back to 1 over the 16 ramp frames, 57 to 73 frames after the cast, in step with the walk; nothing came back within 40 frames. The `pst` watch saw no change in the number and bool entries it reads, which is not observed rather than a negative: how many of the 54 entries it read was not counted, and with no `pSt` change in any watch of the session it had no positive control there. Of the builtin variables only `image_index` moved. |
| `penalty-lever` | fail | Holding `playerEffect[199]` at 1 during `PlayerMovement` took the 16-frame ramp away, but the 57 frames at 0.650 of the mode stayed; the cast was made (Healing Zone's count rose by one). |
| `cast-mana` | not-observed | None of the `pSt` entries the watch reads (number and bool entries only) changed on the key cast nor on the forced one, though both cast (Healing Zone's count rose by one each). A forced press made too soon after the key cast cast nothing. |
| `toggle-settle` | fail | One forced press switched Soul Spurn on 19 frames later. The second pair did not end off: the toggle went off 20 frames after the press and on again 57 frames after it, and was still on minutes later. |
| `base-control` | pass | Helmet base 7 built, "Arcane Resisting Great Helm". |
| `base-check` | pass | Body armour, boots and gloves all built at base 0: "Steel Armor of Vitality", "Boots of Sorcerer", "Heavy Gloves of Energy". |
| `zone-control` | pass | At the Outskirts' arrival point and 32 px either side, every field read free and the line clear. |
| `zone-edge` | pass | Upward from the arrival point the floor ended at an `Invisible_Wall_obj`: `family` met it one 16 px step before the grid read blocked and the wall line crossed it; past the wall `family` read free again while the grid stayed blocked to the room's edge. |
| `dbg-control` | pass | The control logged about once a frame, and the attach was proven by the game's frame counter stopping at the control hold and running after. |
| `dbg-walk-movers` | pass | `PlayerMoveSkill`, `PlayerMoveSkillFunc` and `CA_playerMove` logged no hit over either walk, while the control logged 920 and 1056 lines in the same windows; each candidate's bytes matched the research project's. |
| `dbg-cast-active` | pass | `CA_playerTalentActive` logged no hit over one standing E cast, while the control logged 1187 lines. |

**The walk's speed is affine in the speed stat.** The six swept ratios and
the half fall on one straight line in the multiplier to within 0.02% at every
point: the modal step is proportional to id 64's answer plus about 200
(fitted 200.0), with Sorak's 151 standing at 1.755 of the step an answer of 0
gives. No cap was reached up to an answer of 226.5, negative answers slowed
the walk further, and phase 1's 0.785 at half the answer is the same point
again at a different frame rate. So halving the walk means answering
(151 - 200) / 2, about -24.5, which is what `speed-half` measured as 0.500.
`stat-trace` found the id-64 call outside every row the probe wraps,
`PlayerMovement` included, as phase 1's reading placed it in the player's
Step event; where inside the Step event it is read was not read.

**The walk is moved outside all twenty rows.** No script row and no builtin
row moved it, `mp_linear_step` and the other movement builtins were not
called at all during `speed-builtin`'s walk, and the debugger saw `PlayerMoveSkill`,
`PlayerMoveSkillFunc` and `CA_playerMove` never run on a walk. The
`speed-factor` result is weaker than its pass suggests: the values the
probe read and halved were 163556 for `hSpeedFactor` and 163557 for
`vSpeedFactor`, consecutive whole numbers that do not look like speed
factors, so whether the walk scales with those members is **not
established**. Against phase 1's Live 2 the modal step's ratio was 0.500
(2.989 against 5.979 px) at a frame-time ratio of 0.518 (6.94 against
13.4 ms): the step halved exactly while the frame time did not quite, so
whether and how the step follows the frame clock is **not established**.

**The slowdown is two parts, and only the ramp follows `playerEffect[199]`.**
The cast drops the walk to 0.650 of its step for 57 frames, then a ramp of
16 frames brings it back. `playerEffect[199]` falls to 0.65 on the cast frame
and climbs to 1 frame for frame with that ramp, so it is the cast's speed
factor. Holding it at 1 during `PlayerMovement` removed the ramp and nothing
else, so during the flat part the walk takes its 0.65 from somewhere the hold
does not reach (written back after `PlayerMovement`, or read elsewhere).
`playerEffect[204]` turned from true to false 56 frames after the cast, the
frame before the ramp began, which makes it a candidate for what holds the
flat part; it was not held, and what it means is **not established**. The
cast also turned `playerEffect[205]` from true to false and changed
`playerEffect[196]` and `playerEffect[250]` 18 frames after it, and
`playerEffect[54]`, `[56]` and `[192]` count down by one a frame. In frames
the slowdown is longer than phase 1's 21 at about half this frame rate;
whether it is timed in frames or in time is **not established**.

**No caller to name.** None of the four candidate logpoints hit, so the
capture holds no return location, and there is no caller to look up. With the
control logging through the same windows, these are negatives with a working
instrument: the walk and a standing cast do not reach those four scripts in
this build (not observed).

### Mod live procedure 2

The built set, verified on the research build with its switch on: the
pieces, their art, the full-set read, the automation, the half walk, the
cast's slowdown on it, the jump and the switch's teardown. The capture is the
workorder's mod live capture 2.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  ForgePact commit that completes the worn read over the gear slots
  (`c598b5b`), DLL SHA-256
  `0a44a9d06ecd3780f1b4b4117e3178b4115f1094531e7869f368017c36b76362`. The
  owner was asked before it was installed. The first attempt ran the build
  before that commit (SHA-256 `13abbd1e…7857`).
- **Character:** save slot 14, "Sorak" (White Mage), in `Town_01_rm`, later
  the Outskirts of Inoya for the zone check. No flight charm worn,
  `jumpscenery` off, the saves backed up first and restored at the end.
- **Control:** as Mod live procedure 1 (`ping`, bare `mark1probe`).
- **Pieces:** `mark1probe spawn set` at the player, one piece per class, each
  picked up by a left click and worn by a right click, then Save & Exit and a
  reload from character select.
- **Bar:** `hs_skills_status` and `tgprobe talents` for every learned skill,
  recording each one's `abilityCooldown`. A no-cooldown skill is a learned
  skill that is not an aura and whose `abilityCooldown` is at most 0.25. Q
  takes Shadow Bolt, R the next such skill (ties: Heavenly Fire first), E
  keeps Healing Zone as the one cooldown skill, and Y stays empty as the
  empty-slot outlier. Soul Spurn left the bar for the session, so there is no
  toggle case. The bind goes through the game's skill popup.
- **Walks:** Mod live procedure 1's modal-step rule (`pos 240` from one spot,
  `ms=` within 10%). A walk with the bonus on also runs `watch effect` over
  the same window, and its mode is taken over the frames whose
  `playerEffect[199]` reads 1 (casts left out); fewer than 60 such frames is
  not-observed for that check.
- **Debugger:** none: every check is an outcome, and a per-frame logpoint
  lengthens the frame and moves the modal step.

**Checks**, in the order they run:

- `dll-hash`, `control`, `marker` (validity).
- `fresh-off`: before any `mark1set` command, the stat line reads the switch
  off and nothing installed.
- `art-default`: before anything arms the art, a spawned body armour shows
  its base item's sprite on the ground and in the bag, never our art and
  never the error sprite.
- `arm-at-select`, `art-load`: `mark1set bonus 1` at character select waits
  for the character and installs after the load; the art loads every
  embedded PNG and the armour from `art-default` takes our bag icon.
- `art-ground`, `art-fallback`: with every PNG loaded the spawned pieces lie
  on the ground as our ground copies; with none, as their base items'
  sprites. Only one of the pair can be seen in one build.
- `spawn-control` (validity for what follows), `art-bag`, `art-tooltip`
  (research): the four pieces carry the table's names, Heroic rarity and the
  gold `Mark 1` row; the bag draws our icon; what the tooltip draws as its
  icon is recorded.
- `equip-detect`, `art-equipped` (research): with all four worn the set reads
  full with no unreadable read; what the worn body slot draws is recorded.
- `walk-control` (validity for the walks): two walks with the switch off,
  modes within 3%.
- `off-vanilla`: with the switch off, no press, only the key cast counted,
  and a jump of about 97 px for a cursor 100 px away.
- `on-automation`: with the switch on and no key, each skill on Q, E and R
  cast at least once, column 6 never pressed, and every failure count 0.
- `gate-kept`: Healing Zone's column casts fewer than half its presses.
- `cooldown-recast`: Healing Zone cast at least twice with no key, its
  presses refused in between, the frames between casts recorded.
- `on-border`: the deep-red border on each bound column, none on an empty one.
- `on-speed`: the bonus-on mode over its effect-1 frames is 0.47 to 0.53 of
  `walk-control`'s mean.
- `on-penalty`, `penalty-stack` (research): the run of frames below 0.9 of
  the bonus-on mode around an automated cast; and, for a keyed Healing Zone
  cast, the cast frames' mode against the same walk's pre-cast mode (about
  0.65 if the slowdown stacks on the half walk, about 1.3 if it replaces it).
- `jump-learn`, `jump-near`, `jump-far`: the jump's maximum M as learned, and
  jumps with the cursor 300, 100, about 500 and about 900 px away each landing
  about 3M (738 px) away.
- `jump-backoff`, `jump-prop`, `jump-edge`, `zone-jump-edge` (research): a
  take-off whose 3M point is blocked backs off to a walkable point past M; a
  jump across the Bounty Board lands past it; a jump at the town's south edge
  and one at the Outskirts' edge are refused, the player staying inside, and
  the refusal comes from read answers, not failed reads.
- `unequip-stops`, `bonus-off-restores`: the gloves off stop the presses and
  give back the walk; the switch off stops them too, and the jump returns to
  about M.

**Amendment 11**, after the first attempt's `equip-detect` failed: the worn
read finds each piece wherever the worn gear holds it among gear slots 0 to 9
rather than at the table's slot for its class, and the stat line names the
slot each piece was found in (`worn=`); the build became that commit's; from
character select the operator clicks Back before loading the slot; and
`lootshow` follows each spawn, because the owner's loot filter hides the
pieces, with no piece spawned twice. The steps and the checks were otherwise
unchanged.

### Mod live 2 results

The built set on the research build (Mod live procedure 2, 2026-10-19, two
attempts: 18:18 to 18:27 UTC and, after Amendment 11, 19:15 to 19:33 UTC).
`tools/live_checks.py` reads 32 checks from the second attempt's block of
`forgepact-166-mark1-set-mod-live-2.md` (its last `## Checks` block): 26
pass, 1 fail (`on-automation`), 5 not-observed (`art-fallback`, `on-speed`,
`on-penalty`, `penalty-stack`, `jump-far`), none not-run. The validity checks
passed, and so did `spawn-control`, `equip-detect` and `walk-control`, which
the later checks stand on.

**The first attempt** ran the earlier build and stopped at `equip-detect`.
Everything before it passed as in the second attempt below, but with all four
pieces worn the stat line read `set=` `partial(3)`, `unreadable=0`, on seven reads,
a reload included, and taking the gloves off and putting them back left it at
`partial(3)`: the read did not see the gloves. That build read each piece
only at its class's row of the toolkit's equipment-slot table, whose gloves
row (slot 3) had never been measured. The walk, automation and jump checks
were not run, and Amendment 11 followed.

The second attempt ran at about 6.94 ms a frame (6.93 to 6.96 in every
window), as Mod live 1 did. With the four pieces worn in place of Sorak's own
gear, the unlevered modal step was 2.385 px, against Mod live 1's 2.989 px in
that gear.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, `rows=20 hooked=0 held=0`. |
| `fresh-off` | pass | At the main menu, before the first load: `bonus=off`, `install=not-armed`, `set=none`, `reads=0`. |
| `art-default` | pass | `icons=0/8 art=off ground=0`; the spawned body armour lay as a small grey plate and showed a grey chest-plate icon in the bag, its base item's art; no error sprite. |
| `arm-at-select` | pass | `bonus=on install=waiting-for-character` at select, then `installed` after the load with `set=none`, presses 0 and `art=armed`. |
| `art-load` | pass | `icons=8/8`, `ground-route=both`; the armour from `art-default` now drew our detailed 2x3 icon. |
| `art-ground` | pass | Four spawn lines `dressed=yes`, `ground=4`; the body armour on the ground drew our armour sprite. |
| `art-fallback` | not-observed | The build embeds all eight PNGs (`icons=8/8`), so the no-art case could not occur. |
| `spawn-control` | pass | `Mark 1 Body Armor` (`Heroic Body Armor`) with the gold `Mark 1` row and two `Full set:` rows; the other three labelled `Mark 1 Helmet`, `Mark 1 Boots`, `Mark 1 Gloves`. |
| `art-bag` | pass | Both body armours in the bag drew our 2x3 icon. |
| `art-tooltip` | pass | The armour's tooltip draws no item icon at all, only its text rows (research, recorded). |
| `equip-detect` | pass | `set=full unreadable=0 worn=0/1/2/4`: helmet at gear slot 0, body armour 1, boots 2, gloves 4; the same after a reload. |
| `art-equipped` | pass | The worn body slot drew our armour art, the same as the bag (research, recorded). |
| `walk-control` | pass | Two walks with the switch off, modal step 2.385 px both, 6.93 and 6.94 ms a frame. |
| `off-vanilla` | pass | Switch off: presses 0, the probe's `TalentUse` row counted one call and `casts=252:1` (the key cast of Healing Zone); a jump with the cursor 101 px west covered 96.9 px. |
| `on-automation` | fail | The switch on and no key, for 1350 gate calls (one a frame): Q, E and R pressed 1350 times each and column 6 never; Shadow Bolt (Q) cast 20 times, Healing Zone (E) once, Heavenly Fire (R) not at all; the gate was called 1350 times and every failure count read 0, `threw=0`. |
| `gate-kept` | pass | From the same window: E's 1350 presses gave 1 cast, Q's 1350 gave 20 and R's 1350 gave 0. |
| `cooldown-recast` | pass | With no key for the 3600-frame count, `White_Mage_Healing_Zone_obj`'s count rose at five frames, 661, 910, 1071 and 501 frames apart, and the probe read `casts=252:4`; E's presses rose from 5250 to 9750 while its casts stayed put. The setup read Healing Zone's `abilityCooldown` as 14. |
| `on-border` | pass | The deep-red border framed Q, E and R; Y is empty and the bar does not draw it. |
| `on-speed` | not-observed | No frame of the window read `playerEffect[199]` at 1: it read 0.65, rising to 0.670833 for one frame at each automated Shadow Bolt cast, about 60 frames apart. The whole window's mode was 0.775 px at 6.93 ms. |
| `on-penalty` | not-observed | Under the same reading no frame ran below 0.9 of the bonus-on mode (0.6975 px) although casts rose (Shadow Bolt by 26, Healing Zone by 1, over the walk and the seconds around it): with the slowdown held on by the automation, a cast's dip cannot be told from the bonus-on mode. |
| `penalty-stack` | not-observed | No frame read `playerEffect[199]` at 1, so there was no pre-cast mode, and the keyed E cast fell after the window, because the walk's calls ran one after another. |
| `jump-learn` | pass | `max=` read `250.3(cap)`, jumpscenery's cap; with the cursor 300 px west, jump 1 covered 525.0 px and jump 2 734.9 px. |
| `jump-near` | pass | Cursor 110 px west: 740.9 px, to the full distance and not the cursor; `granted` 2 to 3. |
| `jump-far` | not-observed | The 500 px case (cursor 510 px away) landed 750.9 px away; the 900 px case was not run, because no point on screen is 900 px from the player. |
| `jump-backoff` | pass | From (912, 1560), cursor due south: 250.0 px to y 1810, short of the blocked ground, `backed-off` 1 to 2, the landing walkable. |
| `jump-prop` | pass | From (1300, 822) west across the Bounty Board: 718.9 px, landing past it on free ground, `granted` 5 to 6. |
| `jump-edge` | pass | At the town's south edge: `refused-landing` 0 to 1 and a vanilla jump of 40.3 px; `refused-unreadable` 0 and every `unread=` count 0; the family's `rejected=` count rose; `grid-reads` rose only in its free count through the town jumps (18/0/0), never nogrid. |
| `zone-jump-edge` | pass | At the Outskirts' edge: `refused-landing` 1 to 2, a vanilla jump of 125.1 px that stopped inside the wall (research). |
| `unequip-stops` | pass | Gloves off: `set=` `partial(3)` and `worn=0/1/2/-`, presses held at 78873 across two reads about a second apart (the first about 140 frames after the gloves came off, not within 60), and the walk's mode back at 2.385 px. |
| `bonus-off-restores` | pass | Switch off with the set worn: presses unchanged across about 360 frames, the mode 2.385 px, and a 300 px jump covered 250.3 px with the mod's jump count unchanged. |

**The set is read where the pieces really are.** The worn read found the
helmet at gear slot 0, the body armour at 1, the boots at 2 and the gloves at
4 of the local player's worn gear, and lost only the gloves when they came
off (`worn=0/1/2/-`). So slot 3 is not where this character's gloves are
worn, against the toolkit's equipment-slot table, whose rows 1 to 8 had no
measurement behind them.

**The automation pressed every column, and one skill took every turn
that mattered.** Pressing Q, E and R inside every gate call cast Shadow Bolt
about once in 60 frames and Healing Zone whenever it was ready, but
Heavenly Fire, whose `abilityCooldown` (0.25) is Shadow Bolt's, was never
cast in those 1350 frames. Plausibly the gate serves one asking slot a call
in a fixed order and Shadow Bolt, ready again within about 60 frames, keeps
it from reaching R; in which order the gate serves the slots, and whether
it serves more than one a call, was not read. The owner's answer after the
session (take turns: when a column's cast lands, the next press goes to
the column that has waited longest) is this workorder's turn rule.

**The game's cooldown holds a forced press.** Healing Zone's column was
pressed every gate call and cast once in `on-automation`'s 1350 presses, and in the
3600-frame count it was recast 501 to 1071 frames apart. Its
`abilityCooldown` reads 14; at 144 frames a second 14 seconds would be 2016
frames, longer than every measured interval, so what unit it is in, and
whether something shortens it, is **not established**. Five rises of the
zone object's count stand against four casts the probe counted in the same
window; the two were not reconciled.

**The half walk under the automation: not observed.** With Shadow Bolt
recast about every 60 frames, `playerEffect[199]` never returned to 1 in a
bonus-on window, so no window had frames free of the cast's slowdown, and
`on-speed`, `on-penalty` and `penalty-stack` are not observed. One number
is still worth keeping, as a consistency observation and not a verdict: the
bonus-on windows' mode, 0.775 px, is 0.325 of `walk-control`'s 2.385 px, and
0.325 is exactly 0.5 × 0.65, the half walk times the slowdown's flat factor
from Mod live 1. That is what stacking would give, but by arithmetic only:
the frames behind it are all under the slowdown, nothing measured them
against an unslowed bonus-on step, and the check that would tell (`on-speed`
with no automated cast) did not run. The `watch effect` reply also hit its
400-line cap after about 126 of the 240 frames in each window, so the rest
of each window was not watched.

**The jump.** M read 250.3 px, jumpscenery's cap; landings of 734.9, 740.9
(cursor 110 px away) and 750.9 px (cursor 510 px away) are 2.94 to 3.00 times
it, a back-off stopped at 250.0 px, and a jump across the Bounty Board covered
718.9 px. Both edges refused: the town's south edge, and the Outskirts' edge,
which loaded in a different layout from Mod live 1 and was found again by the
same signature (an `Invisible_Wall_obj` met by the family, then the grid alone
blocked over the cliff tiles). The view is 1280 × 720 room pixels centred on
the player, so an on-screen cursor reaches about 645 px across and 741 px at a
corner: `jump-far`'s 900 px case cannot be made with the mouse, and no
on-screen cursor lies past 3M (751 px) either. It is recorded not observed and
not re-run; the harness scenario for a cursor past 3M pins that rule.

**The art.** The worn body slot draws the piece's `"11"` sprite, our art, as
the bag does; the tooltip draws no item icon at all; and the fallback to base
art was not observed in this build, which embeds all eight PNGs.

**The toggle guard was not observed live**: Soul Spurn left the bar's cases
for the session (owner, 2026-10-19).

### Mod live procedure 3

The turn rule that replaced Mod live 2's press-every-column automation, and
the half walk re-measured with Healing Zone alone on the bar (the owner's
answers after Mod live 2, 2026-10-19), on the research build with the switch
on. The capture is the turns workorder's live capture 1,
`forgepact-166-mark1-set-turns-live-1.md`, kept with the hub's workorder
files and not tracked.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  ForgePact commit that completes the landing detour and the turn rule's
  adapter (`506038b`), DLL SHA-256
  `96c6d215ac87ab5e614b908abc937a404664b3aec60214a24a44c3401781483e`. The
  owner was asked before it was installed.
- **Character:** save slot 14, "Sorak" (White Mage), in `Town_01_rm`, from
  the save before Mod live 2 (no Mark 1 pieces yet), with Mana Orb (253) on
  Q, Healing Zone (252) on E, Soul Spurn (240) on R and Y empty. No flight
  charm, `jumpscenery` off, the saves backed up first and restored at the
  end.
- **Control:** as Mod live procedure 1 (`ping`, bare `mark1probe`).
- **Pieces:** `mark1probe spawn set` once, then `lootshow`; each piece picked
  up by a left click and worn by a right click from the bag (C).
- **Install:** `mark1set bonus 1`, then `mark1set bonus 0` (the install stays
  in), and only then `mark1probe hook`, so that the probe's `TalentUse` row
  goes in and the rows the set holds are named as held by `mark1set`.
- **Part A, Healing Zone alone:** Q and R emptied through the game's skill
  popup, so Healing Zone is the one automated column and Y the empty one.
- **Part B, Mod live 2's own bar:** Shadow Bolt (239) on Q, Healing Zone on
  E, Heavenly Fire (245) on R and Y empty, bound through the popup.
- **Walks:** from `playerwarp 912 822`, D held, `mark1probe pos <n>`; with the
  bonus on, `mark1probe watch effect <n>` in the same command file, and a
  window's mode taken over its watched frames whose `playerEffect[199]` reads
  1. Fewer than 60 such frames leaves that window not observed.
- **Debugger:** none. Every check is an outcome counted by the probe's
  `TalentUse` row and `mark1probe count` beside the set's own counters, and
  a per-frame logpoint lengthens the frame and moves the modal step.

**Checks**, in the order they run:

- `dll-hash`, `control`, `marker` (validity).
- `equip-detect` (validity): with the four pieces worn, `set=full`,
  `unreadable=0` and four slots in `worn=`.
- `install` (validity): `install=installed` with no refusal, `landed=0/0/0/0`
  and `cast-calls=` on the stat line, and the probe's `hook` naming
  `mark1set` for the rows it holds.
- `walk-control` (validity for the walks): two walks with the switch off,
  modes within 3%.
- `cast-control` (validity for Part A): one key cast of E, counted by the
  probe as `casts=252:1`.
- `landed-control`: Part A, the switch on and no key for a 2400-frame count of
  `White_Mage_Healing_Zone_obj`: Healing Zone cast at least once, column 4's
  `landed=` equal to the probe's casts within 1, the count showing as many
  new zones, presses on column 4 only and `yielded=0/0/0/0` (a lone column
  never rests).
- `on-speed`: Part A, the bonus-on mode over at least 60 watched frames
  reading 1 is 0.47 to 0.53 of `walk-control`'s mean.
- `on-penalty`, `penalty-stack` (research): Part A, a bonus-on walk holding
  an automated Healing Zone cast after at least 60 watched frames reading 1:
  the run of frames below 0.9 of the pre-cast mode, its length and lowest
  ratio; and the cast frames' modal step against the pre-cast mode, about
  0.65 if the slowdown stacks on the half walk, about 1.3 if it replaces it.
- Part B's positive control (no check of its own): with the switch off, one
  key cast of Q and one of R, counted as 239 and 245. No session had yet
  recorded a cast of Heavenly Fire, so without it the two checks below would
  be recorded not observed, never failed.
- `on-automation`: Part B, the switch on and no key for a 3600-frame count:
  239, 245 and 252 each cast at least once, 239's and 245's casts each at
  least half the other's, column 6 never pressed, the four failure counts
  `0/0/0/0` and `threw=0`.
- `turn-landed`: from the same reads, each column's `landed=` rise equals the
  probe's casts for its talent within 1, and `cast-calls=` rose at least as
  much as their sum.
- `cooldown-recast`: from the count, at least two new Healing Zone instances
  with no key, and the frames between them.

`landed-control` is off the session's required passes by the owner's
decision of 2026-10-20, after it ran: one Healing Zone cast can leave two zone
instances, so the zone count is no exact control for landings (below).

### Mod live 3 results

The turn rule and the half walk on the research build (Mod live procedure 3,
2026-10-20, 09:37 to 09:50 UTC, one attempt). `tools/live_checks.py` reads 14
checks from `forgepact-166-mark1-set-turns-live-1.md`: 13 pass, 1 fail
(`landed-control`), none not observed, none not run. The validity checks
passed, and so did `equip-detect`, `install`, `walk-control` and
`cast-control`, which the later checks stand on. The session ran at 6.93 to
6.94 ms a frame, as Mod live 1 and 2 did, and the unlevered modal step in the
set was 2.385 px, Mod live 2's.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `marker` | pass | `mark1probe: research build`, `rows=20 hooked=0 held=0`. |
| `equip-detect` | pass | `set=full unreadable=0 worn=0/1/2/4`, the slots Mod live 2 measured. |
| `install` | pass | `install=installed`, no refusal; `landed=0/0/0/0` and `cast-calls=0`; `mark1probe hook` detoured 15 rows, its `TalentUse` row among them, and named `CheckTalentUse`, `TalentUseSetSpeed` and `ReturnSpecificStat` as held by `mark1set`. |
| `walk-control` | pass | Two walks with the switch off, modal step 2.385 px both, 6.93 and 6.94 ms a frame. |
| `cast-control` | pass | One key cast of E: the probe's `TalentUse` row counted one call and `casts=252:1`. |
| `landed-control` | fail | Every clause but one held: over the first window column 4's `landed=` read 4 and the probe `casts=252:4`; presses only on column 4 (`presses=0/5760/0/0`, `skip-empty=5760/0/5760/5760`); `yielded=0/0/0/0`. The clause that missed is the zone count's: in each of two windows one rise of `White_Mage_Healing_Zone_obj`'s count went from 0 to 2, so the second window's two casts left three new zones. |
| `on-speed` | pass | 103 watched frames read `playerEffect[199]` at 1, every one at a step of 1.192 px: 1.192 / 2.385 = 0.4998 of `walk-control`. |
| `on-penalty` | pass | An automated Healing Zone cast (no key) after 86 watched frames at 1 dropped the step below 0.9 of the pre-cast 1.192 px for 86 of the next 87 frames (one double step broke the run, 28 and 58 frames); lowest step 0.775 px, 0.650 of the pre-cast mode (research, recorded). |
| `penalty-stack` | pass | The cast frames' modal step, from the fall to the step's return to 1.192 px 92 frames later, was 0.775 px on 75 of them: 0.650 of the pre-cast mode. The slowdown stacks on the half walk. |
| `on-automation` | pass | The switch on and no key, over 4590 gate calls (one a frame): `casts=239:31,245:30,252:3`; column 6 never pressed; `col-unreadable`, `orig-unreadable`, `write-failed` and `restore-failed` `0/0/0/0`, `threw=0`. Part B's positive control had cast both by key first (`casts=239:1,245:1`). |
| `turn-landed` | pass | `landed=` rose 31, 3 and 30 on columns 3, 4 and 5 against the probe's 239:31, 252:3 and 245:30, and `cast-calls=` by 64, their sum. |
| `cooldown-recast` | pass | With no key, `White_Mage_Healing_Zone_obj`'s count rose at four frames, 1064, 587 and 1627 frames apart, against three casts the probe counted; the extra zone lasted 88 frames. |

**The skills take turns.** On Mod live 2's own bar, where pressing every
column had given Shadow Bolt 20 casts and Heavenly Fire none, one column a
call gave Shadow Bolt 31, Heavenly Fire 30 and Healing Zone 3 in 4590 frames
(about 32 seconds). Over the same calls the set pressed Q 1772 times, E 993
and R 1825, never Y, and counted `skip-turn=` rises of 2818, 3597 and 2765.
Healing Zone, the column on cooldown most of the time, gave up its turn 11
times (`yielded=0/11/0/0`), and the two no-cooldown skills never did. Those
are the casts per column the provisional patience (90 frames) and rest (288
frames) give on this bar. Heavenly Fire cast by key in the positive control
and 30 times through the press write here, so Mod live 2's zero came from
pressing every column at once, not from the skill or the route; which of
several pressed slots the gate serves is still not read.

**The landing instrument agrees with the probe.** Every landing the set
counted matched a cast the probe's `TalentUse` row counted, column by column,
alone (4 against 4) and beside two other skills (31, 3 and 30 against the
same), and `cast-calls=`, the detour's own control, rose by exactly their
sum. A lone eligible column was pressed on every call and never rested
(5760 presses, `yielded=0`).

**One Healing Zone cast can leave two zones.** The only clause that failed,
in `landed-control`, was the zone count's: one rise of
`White_Mage_Healing_Zone_obj`'s count went from 0 to 2 in each of two
windows, and in Part B four rises stood against three casts, the extra zone
lasting 88 frames. So the zone count is at least the casts and not equal to
them; it was the wrong control for landings, and what the second instance is
was not read (§ Not established). Alone on the bar, Healing Zone was recast
1575 frames apart in both windows; beside the other two, 587 to 1627 frames
apart.

**The half walk measures half, and a cast's slowdown stacks on it.** With
no cast in its window, the bonus-on step was 1.192 px against 2.385 px
switched off, 0.4998: the shipped lever halves Sorak's walk in the set, as
`speed-half` found in his own gear. An automated Healing Zone cast then held
the step at 0.775 px, 0.650 of the bonus-on step, for 76 frames (one double
step among them) and brought it back over a 16-frame ramp. So the slowdown
multiplies the halved walk rather than replacing it: under a cast the set
walks at 0.325 of the unlevered step, which Mod live 2's 0.325 only matched
by arithmetic and this session measured. The flat part ran 76 frames here
against Mod live 1's 57 for a key cast without the set (§ Not established).
The `watch effect` reply again hit its 400-line cap, after 132 of the 480
frames, so the step's return was read from `pos`.

**Emptying a bar slot.** The operator emptied Q and R without the owner:
a left click on the bar slot opens the skill popup, and a left click on the
skill already bound to that slot (the one labelled with the slot's key)
unbinds it. `hs_skills_status` then read the two slots at talent 0.

**What ran differently.** In Part A's first window the operator idled about
5760 frames, longer than the 2400-frame count, so that window's probe and
stat reads cover more than its count; the second window took the reads right
after the count, but the set's counters were cumulative there. Neither
changes a verdict: the first window's landings and casts were counted over
the same span from zero.

### Phase 2 decision

Recorded on 2026-10-19 from Mod live 1 and the owner's answers after it;
`penalty-stack` on 2026-10-20 from Mod live 3. Each key takes exactly one
value, on a line of its own, and the check that decides it is on the line
below.

speed-route: stat-model
Decided by `speed-sweep` and `speed-half` (both pass, control `walk-control` passed): one affine shape fits every swept point within 0.02%, and the interpolated answer gave 0.500; `speed-builtin` found no builtin that moves the walk and `speed-factor` no effect from the factor members. `stat-trace` placed the id-64 call outside every row Mark 1 wraps, so the halved answer goes to every local id-64 call, and the character sheet and the damage scripts that read id 64 see it too.

speed-shape: affine
Decided by `speed-sweep` (pass): the modal step is proportional to id 64's answer plus about 200, every point within 0.02%, with no cap reached up to an answer of 226.5.

penalty-route: none
Decided by `penalty-stat` (pass: the answer never fell), `penalty-watch` and `penalty-lever` (both fail: `playerEffect[199]` held at rest removed only the ramp, leaving 57 frames at 0.650), so neither the stat route nor an element route qualifies. The owner chose to ship with the slowdown unchanged and to measure in Mod live 2 whether it stacks on the half speed or replaces it.

cast-mana: not-observed
Decided by `cast-mana` (not-observed): no `pSt` element changed on a key cast or a forced one, and none changed in any `pst` watch of the session, so the watch has no positive control within `pSt` and where mana is held stays open.

toggle-settle: 19
Decided by `toggle-settle` (fail on its second pair; the value taken by the owner): one forced press switched Soul Spurn on 19 frames later. Measured finding: the second forced press switched it off 20 frames after the press and on again 57 frames after it, with no further press from the probe; what switched it back on was not read.

piece-bases: 0:7,1:0,2:0,4:0
Decided by `base-control` and `base-check` (both pass): the helmet builds at base 7 and body armour, boots and gloves each at base 0.

zone-edge: both
Decided by `zone-edge` (pass, control `zone-control` passed): at the Outskirts' upper edge `family` met an `Invisible_Wall_obj` one step before the path grid turned blocked, and past the wall only the grid still read blocked, to the room's edge, as in town; the tile layers read did not mark it. One edge of one zone was measured.

penalty-stack: stacks
Decided by `penalty-stack` (Mod live 3, research; `on-speed` passed in the same walk form, control `walk-control` passed): an automated Healing Zone cast in a bonus-on walk held the step at 0.775 px against the walk's pre-cast 1.192 px, a ratio of 0.650, the slowdown's flat factor, and not about 1.3, so the cast's slowdown multiplies the half walk. Mod live 2 had left it not observed.

The values each key may take:

- `speed-route`, the lever that halves the walk: `stat-model`, `builtin`,
  `factor` or `none`.
- `speed-shape`, the step against the id-64 answer: `affine`, `capped`,
  `affine-capped`, `other` or `not-run`.
- `penalty-route`, the lever that removes the cast's slowdown: `stat-dip`,
  `entry` or `none`.
- `cast-mana`, whether a forced cast still costs mana: `charged`,
  `not-charged` or `not-observed`.
- `toggle-settle`, frames from a forced press to the toggle's on-state: a
  whole number, or `not-observed`.
- `piece-bases`, the base each piece's class builds at: `<class>:<b>` four
  times, comma-separated.
- `zone-edge`, which fields mark a generated zone's edge: `grid`, `family`,
  `both`, `neither` or `not-observed`.
- `penalty-stack`, how a cast's slowdown combines with the half walk:
  `stacks` (the cast frames' step about 0.65 of the bonus-on step),
  `replaces` (about 1.3) or `not-observed`.

## Phase 3: the drops

Phase 3 lets a player find the four pieces. Behind a second switch, **Mark 1
drops** (`mark1set drops`, off by default), each piece joins the game's own
Heroic roll, `DropItemHeroic`, with one entry's share, through the stand-in,
attribution and rewrite machinery the flight charms (#74) use for the Angelic
roll. The game builds the piece's base item and the Mark 1 dressing turns it
into the piece. Only that roll is built; the ordinary unique roll on kills,
the Angelic roll, vendors and crafting stay out of scope, because how the
pieces are obtained is still the owner's call and the Heroic roll is phase 1's
default. The toolkit's ForgePact guide documents the switch, its stat line
and the probe forms.

### Phase 3 static reading

Read on 2026-10-20 through the hub's ghidra MCP, and, for the two callers the
MCP could not decompile (`VaultUse` timed out at 240 s and at 500 s), a
headless decompile on the same build. The shared facts are in the hub's
`docs/RUNTIME_DATA_MODELS.md` § 23.4; this is the part phase 3 builds on, in
short. No decompiled text is quoted here.

- **The roll's arguments.** `DropItemHeroic` (1374) takes an x, a y and a
  sub-list index. An undefined x or y is read from `self`, and an undefined
  index means sub-list 5. **Static reading.**
- **No empty pick.** It draws from `Controller_obj.lootListUnique` at that
  index and draws again until an entry passes both tests: the definition's
  flag 40 clear and a rarity (field 27) that is neither 7 nor 10. Every draw
  makes one `GetUniqueRepoStruct` lookup. So an entry listed r times among
  the T_H entries that pass is placed r / T_H of the time, whether or not the
  random index can reach the list's length, and one pushed stand-in per piece
  gives each piece 1 / (T_H + 4) of the calls with all four pushed. **Static
  reading**; the share is arithmetic on it, not measured.
- **One build per call.** After the loop it calls `CreateDefaultParams`
  once, from the kept entry's sub-type and base, and then the placement with
  the entry's own type. So the placed class comes from the entry, not the
  parameters: a stand-in has to share its piece's class, and
  `CreateDefaultParams` is the point where the hit is attributed and the
  record rewritten, as in the Angelic roll. **Static reading.**
- **Who calls it.** A byte scan for direct calls found two sites, one in
  `VaultUse` (4597) and one in the Chaos Tower NPC's closure, with
  `CreateDefaultParams`' 177 direct sites as the positive control in the same
  run. The MCP's callers, callees and disassembly for `DropItemHeroic` and
  `VaultUse` are empty: **not observed**, never "none", and a call through the
  script table is invisible to both. As far as is read, the roll runs when a
  player uses a vault or that NPC, and not on a kill.
- **What a vault passes.** `VaultUse` runs from a vault item's right-click in
  the bag. It sorts the vault into one of seven tiers and starts the roll at
  sub-list 4, moving it to 5 on a random check whose threshold rises with the
  tier; a second random check, also rising with the tier, sends the use to an
  unnamed draw of the Angelic kind instead (by its body and its place most
  likely `DropItemAngelic`, an inference). When it does call `DropItemHeroic`
  it passes its own `self`, the x and y of an instance resolved earlier in the
  body, and that index. So a vault's Heroic roll reads sub-list 4 or 5. The
  random bound, and so the chances, were **not read**. **Static reading.**
- **What the NPC passes.** The closure calls `DropItemHeroic` in a loop a
  number of times worked out from `floor`, with all three arguments
  undefined: its own position and sub-list 5. **Static reading.**
- **Measured before.** Ten by-name calls with the player as `self` placed
  seven Heroic (9) and three Satanic (6) uniques (flight charms Live 1,
  2026-10-12, `heroic-route`).
- **Not established.** Which Heroic uniques of the four piece classes
  `lootListUnique[5]` (380 entries) and `[4]` hold, and so T_H and which class
  has a stand-in. The Item Editor's catalog lists 13, 15, 17 and 17 Heroic
  uniques of classes 0, 1, 2 and 4, but that is not a reading of the game.
  Mod live procedure 4's `pool` reads both: `mark1probe pool` for sub-list 5
  and `mark1probe pool 4` for the vault's starting list. How often a vault
  reads [4] rather than [5] is still not read, and the procedure does not
  measure it.

### Mod live procedure 4

The drops on the research build: that the switch starts off and changes
nothing, that the roll's pool has a stand-in for each piece class, that each
piece is reached through the roll, built as the piece and shown with its art,
the share against one entry's, the switch's teardown, and one use of the
game's own vault as the outlier that takes the direct-call path. The capture
is the drops workorder's live capture 1,
`forgepact-166-mark1-set-drops-live-1.md`, kept with the hub's workorder files
and not tracked.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  ForgePact commit that completes the drop adapter; its SHA-256 is recorded in
  the capture. The owner is asked before it is installed.
- **Character:** save slot 14, "Sorak", in `Town_01_rm`. No flight charm
  worn; `mark1set bonus`, `jumpscenery`, Headhunter, Tyrant's Crown and the
  flight charms' drop switches stay off. The saves are backed up first and
  this session's backup is restored at the end.
- **Control:** `ping` answers `pong (YYTK 4.0.1)`, and a bare `mark1probe`
  answers `mark1probe: research build` with its rows line.
- **Cases:** `mark1probe heroic`, the measured by-name call shape, for every
  count; the game's own vault use once, as the outlier.
- **Debugger:** the hub's x64dbg MCP, attached under the hs-drive lease, only
  if the decompile index's build hash is this build's (else the session is
  aborted). Two non-breaking logpoints: the control on `CheckTalentUse`,
  which logs about once a frame, armed first; then the candidate on
  `DropItemHeroic`'s own entry. The locations and expected entry bytes are in
  the capture and the hub's x64dbg guide, never in this doc. Both are armed
  before the switch goes on, so they see the game's bytes, not the detour's
  jump; once the detour is in, a call through the script table enters the
  hook rather than the entry, so from then on the candidate counts direct
  calls only. The debugger detaches before the game is stopped.

**Steps and checks**, in the order they run:

- `dll-hash` (validity): the installed DLL's SHA-256 equals the build's.
- `control`, `marker` (validity): `ping` and `mark1probe` answer as above.
- `fresh-off` (acceptance): `mark1set stat`'s first line reads `bonus=off`
  and `art=off`, and its drops line is the guide's fresh-session line
  (`drops=off`, `install=not-armed`, `heroic-calls=0`, every piece `0/0`).
- `dbg-control` (validity): attached, the control logpoint and then the
  candidate armed; the control logs about once a frame.
- `heroic-control` (validity): `mark1probe heroic 10` places at least 8
  items, every rarity 6 or 9, `pieces=` all 0, and the candidate's hits are
  recorded.
- `pool` (research): `mark1probe pool`, then `mark1probe pool 4`, each
  recorded whole: the length, `droppable=` above 0, and per class a stand-in
  or `none`. The driver records the stand-ins per list (for example
  `[5]: all`, `[4]: partial`) and sets the route from [5] alone, the list
  `mark1probe heroic` reads: stand-ins for all four classes, for some, or for
  none. `reach` and `rate`'s T_H are for [5] only. A vault starts its roll at
  [4] and moves to [5] only on a random check whose bound was not read, so
  which share of a vault's rolls read [4] is not measured, and a route of
  `all` taken from [5] does not mean every piece can drop from a vault.
- `drops-on` (acceptance): `mark1set drops 1` replies `drops=on
  install=installed`; `mark1set stat`'s drops line shows `gate=on`,
  `hook=detoured` and each class's stand-in as `pool` named it, and its first
  line `art=armed` and `ground-route=both`.
- `reach` (acceptance): `mark1probe copies 200`, then `mark1probe heroic 20`
  twice and `mark1set stat`. Every piece `pool` gave a stand-in is placed at
  least once in the 40 calls; each piece's `ours` equals its `built`;
  `refused=0 anomalies=0`; `heroic-calls=` up by 40; the candidate's count
  recorded.
- `drop-built`, `drop-art` (acceptance): a placed piece screenshotted on the
  ground, then picked up by a left click and the bag (C) screenshotted. The
  probe's placed line names it `Mark 1 <piece>` at rarity 9; on the ground it
  shows the owner's ground art while `art=armed` (the base item's sprite while
  `art=off`), never `Error_spr`; the bag shot is recorded (a full bag is
  recorded not observed).
- `rate` (research): `mark1probe copies 1`, then `mark1probe heroic 20` five
  times and `mark1set stat`. The pieces' sum over the 100 calls lies inside
  `observation_within`'s band for 4 / (T_H + 4), T_H from `pool`'s [5], run
  from the hub checkout's root; `refused=0 anomalies=0`.
- `drop-off` (acceptance): `mark1set drops 0`, `mark1probe copies 200`,
  `mark1probe heroic 20` and `mark1set stat`: at least one item placed,
  `pieces=` all 0, and `injected=` unchanged since before the step.
- `heroic-natural`, `dbg-heroic` (research): `mark1set drops 1`; if Sorak's
  bag holds a vault item, the owner right-clicks it once and confirms; then
  `mark1set stat`. `heroic-calls=` and the candidate's count each rise by 1
  and an item is placed. With no vault in the bag, not observed. A vault may
  send the use to the Angelic-kind draw instead (Phase 3 static reading), so
  a rise in neither count with an Angelic or Unholy item placed is that
  branch, not a fault.
- Last: detach, stop the game, inspect the saves, restore this session's
  backup and inspect again.

The session passes on `dll-hash`, `control`, `marker`, `fresh-off`,
`dbg-control`, `heroic-control`, `drops-on`, `reach`, `drop-built`,
`drop-art` and `drop-off`. `pool`, `rate`, `heroic-natural` and `dbg-heroic`
are research and never required. A research check whose control failed
(`heroic-control`, `dbg-control`) is recorded `not-run (control)`.
`heroic-natural` with `heroic-calls=` unmoved while the candidate's count
rose is a blind hook, recorded as a fail, never as "the vault does not
roll".

### Phase 3 kill route static reading

The owner's decision, 2026-10-20, after Mod live procedure 4 ran: "Nooo, it
should drop normally from kills like white mage set the holy one (sacred) or
zealots prophecy (zealot). Both are ss rarity sets with drops available
anywhere". Asked about the two defaults that followed, the owner chose
"Kills and vaults (default)", so the Heroic route stays beside the kill
route, and for the pieces' rarity "Satanic, like real sets", so the pieces
come out Satanic (6) at SS tier instead of Heroic (9).

**Why the Heroic roll was not enough.** As far as is read, `DropItemHeroic`
runs when a player uses a vault or the Chaos Tower NPC, never on a kill
([Phase 3 static reading](#phase-3-static-reading)). A piece that only that
roll can place is a vault reward, not a drop "available anywhere". The
ordinary unique roll is the one a kill runs, and it keeps only rarities below
7, so the Heroic route's rarity-9 stand-ins are refused there; the kill route
needs Satanic stand-ins of the pieces' classes.

Read on 2026-10-20 through the hub's ghidra MCP on the same build. The shared
facts are in the hub's `docs/RUNTIME_DATA_MODELS.md` § 23.4; this is the part
the kill route builds on, in short. No decompiled text is quoted here.

- **A kill reaches the ordinary roll.** A dying monster's Destroy event calls
  `DropItem`, `DropItem` calls `LoadDrops`, and `LoadDrops` calls
  `DropUniqueItems` (1424). Drop type 45 is the one the shared references
  measured as "a unique item" by opening every gate on one monster
  (2026-08-27). How often type 45's gate passes on a kill was **not read** and
  not measured. Chests, goblins, destructibles and piles reach `DropItem`
  too, so the route covers them as well (an inference). **Static reading.**
- **One draw, no set branch.** `DropUniqueItems` picks a sub-list k from 0 to
  4 by a zone value with random step-ups, draws one random index of
  `Controller_obj.lootListUnique[k]`, keeps the entry only when its rarity is
  below 7 and it is not hidden, then builds and places it with the entry's
  own type. A refused draw places nothing. Nothing in it treats set items
  differently. **Static reading**, agreeing with the 2026-10-11 reading.
- **Set items are Satanic uniques.** Mod live procedure 4's `heroic-control`
  placed "Sacred Pendant" from the unique repository at rarity 6, and its
  `pool` found `lootListUnique[4]`'s 218 droppable entries all rarity 6
  (**measured**, 2026-10-20). The ordinary roll keeps every rarity below 7, so
  a set piece on lists 0 to 4 drops through it like any other Satanic unique
  (an inference). Which lists hold the Sacred or Zealot's Prophecy pieces is
  **not established**.
- **How the game names a set.** `GetSetInformation` maps a set's name to one
  of 70 indices. `GetSetName`, `GetSetNames` and `DefineSetPieceRequirements`
  exist by name and were not read, and how an item names its set was not
  read. No drop script names a set: **not observed** by name, never "there is
  none". The pieces are not made a game set item; only their rarity follows
  the game's set pieces.
- **Not readings of the game.** HS Offline Tracker's item table and the Item
  Editor's catalog label the Sacred and Zealot pieces "Set"; neither says how
  the game drops them.
- **Not observed.** The MCP's callers and disassembly of `DropUniqueItems`
  are empty, as for `DropItemHeroic`.

**What the kill route does with it.** For each piece class (helmet 0, body
1, boots 2, gloves 4) the stand-in is a distinct triple of that class on the
unique lists that the ordinary roll keeps and that is listed the same number
of times n on each of lists 0 to 4, preferring n = 0 and then the smallest n.
The roll cannot say which list it drew, so the coin c / (n + c) that
attributes a hit to the piece is exact only when n is equal on all five, the
rule Pocket Elf's charm uses. A class with no such triple has no stand-in on
the kill route; the other pieces still drop and the Heroic route is
untouched. Each call pushes one entry per resolved piece onto each of lists
0 to 4 and removes them on every way out, so each piece gets c / (L_k + p) of
the calls that read list k, p being every entry pushed. With the measured
lengths (50, 61, 79, 152 and 221) at c = 1 and p = 4 that is between 1 call
in 225 and 1 in 54, the share of any single unique on that list; per kill it
is that times type 45's chance, which is not measured. Pocket Elf already
holds `DropUniqueItems`' one inline detour, and a second install would be
table-only and blind, so both features share one detour and one install,
called by whichever switch comes first; with Mark 1's switch off every flight
charm drop behaves as before.

### Mod live procedure 5

The kill route, the pieces' tier and the owner's loot filter on the research
build, with a re-check of Mod live procedure 4's `drops-on`, `drop-built` and
`drop-art`. The capture is the drops workorder's live capture 2,
`forgepact-166-mark1-set-drops-live-2.md`, kept with the hub's workorder files
and not tracked.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  ForgePact commit that completes the kill route's adapter, with the tier
  change in it; its SHA-256 and `DropUniqueItems`' expected entry bytes are
  recorded in the capture. The owner is asked before it is installed.
- **Character:** save slot 14, "Sorak", in `Town_01_rm`, with no Mark 1 piece
  in the bag. No flight charm worn; `mark1set bonus`, `jumpscenery`,
  Headhunter, Tyrant's Crown and every flight charm drop switch, Pocket Elf's
  included, stay off. The saves are backed up first and this session's
  backup is restored at the end.
- **Control:** `ping` answers `pong (YYTK 4.0.1)`, and a bare `mark1probe`
  answers `mark1probe: research build` with its rows line.
- **Cases:** the four pieces through the ordinary roll called by name
  (`mark1probe unique`, the measured call shape of Pocket Elf's `rate-elf`);
  one piece through the Heroic roll, the route the owner kept; `mark1probe
  spawn set`, the loader route, as an outlier; and real kills in
  `Act_01_01`, the game's own direct call from `LoadDrops`, as the other
  outlier. A piece loaded from a save is not run (not observed live).
- **Debugger:** the hub's x64dbg MCP, attached under the hs-drive lease, only
  if the decompile index's build hash is this build's (else the session is
  aborted). Two non-breaking logpoints: the control on `CheckTalentUse`,
  which logs about once a frame, armed first; then the candidate on
  `DropUniqueItems`' own entry. The locations and expected entry bytes are in
  the capture and the hub's x64dbg guide, never in this doc. Both are armed
  before the switch goes on, so they see the game's bytes. Before the shared
  detour is in, a by-name call reaches the entry, which proves the logpoint;
  once it is in, a by-name call enters the hook through the script table, so
  from then on the candidate counts direct calls only, which a kill's is. The
  debugger detaches before the game is stopped.

**Steps and checks**, in the order they run:

- `dll-hash` (validity): the installed DLL's SHA-256 equals the build's.
- `control`, `marker` (validity): `ping` and `mark1probe` answer as above.
- `dbg-control` (validity): attached, the control logpoint and then the
  candidate armed; the control logs about once a frame.
- `kill-control` (validity): with the drops switch off, `mark1probe unique
  10` places at least 8 items, every rarity below 7, `pieces=` all 0, and the
  candidate's count rises by 10.
- `filter-control` (control for `filter-shown`), still with the drops switch
  off, so it runs on the game's own items before any tier of ours exists:
  one item `kill-control` placed whose line read `tier=5`; if none did,
  `mark1probe unique 10` again, switch still off, up to four more times
  until one does. The ground around it screenshotted without `lootshow`,
  then the item hovered and its tooltip screenshotted, its affix lines
  recorded. It passes when that item's ground label shows past the owner's
  loot filter and its tooltip lists no affix lines, the shape of a Mark 1
  piece, which carries none. The match matters because the filter's one
  test read so far, `LootFilterAffixTierVisible`, reads the affix slots'
  tiers, not the item's own tier (RDM § 18.5, static reading), so a tier-5
  item shown for its affixes does not control for a piece without them. A
  tier-5 item with affix lines is recorded with them and is not the
  control; no tier-5 item in the 50 calls is recorded not observed.
- `drops-on` (acceptance): `sigdrop status` and `flightcharm stat` recorded
  whole; `mark1set drops 1` replies `drops=on install=installed gate=pending
  kill-gate=pending`; `mark1set stat` then shows `kill-hook=detoured`,
  `gate=pending`, `kill-gate=pending`, every `standin-<key>=` and
  `kill-standin-<key>=` `pending`, and its first line `art=armed`. After one
  `mark1probe unique 1`, `mark1set stat` shows `kill-gate=on` or
  `not-resolved`, each `kill-standin-<key>=` a stand-in or `none`,
  `gate=pending` still, and `kill-calls=` up by 1.
- `pool-kill` (research): `mark1probe pool kill`, recorded whole: the five
  list lengths and, per class, a stand-in whose `counts=` are all equal, or
  `none`. The driver records the route from it: stand-ins for all four
  classes, for some, or for none.
- `reach-kill`, `tier-read`, `sig-untouched` (acceptance): `mark1probe
  copies 200`, then `mark1probe unique 20` twice, `mark1set stat`, `sigdrop
  status` and `flightcharm stat`. Every class `pool-kill` gave a stand-in is
  placed at least once in the 40 calls; each piece's `kill-<key>` ours equals
  its built; `refused=0 anomalies=0`; `kill-calls=` up by 40 (`reach-kill`).
  Every Mark 1 placed line reads `rarity=6 tier=5`, and the other lines'
  `tier=` are recorded (`tier-read`). `sigdrop status`' `injected=`,
  `ourHits=` and `built=`, and `flightcharm stat`'s drops line, equal those
  recorded at `drops-on` (`sig-untouched`).
- `filter-shown` (acceptance): the ground around the player screenshotted
  without `lootshow`, under the same filter `filter-control` ran under. A
  `Mark 1 <piece>` ground label shows past the owner's loot filter.
- `drop-built`, `drop-art`, `tier-ss` (acceptance): if the filter showed no
  piece, `lootshow` first. One piece hovered on the ground and
  screenshotted, then picked up by a left click, the bag opened (C), the
  piece hovered and screenshotted. Its placed line named it `Mark 1 <piece>`
  at rarity 6; the ground sprite is the owner's art, never `Error_spr`; the
  tooltip, on the ground or in the bag, reads the piece's name, Satanic and
  "Tier SS"; the bag icon is the owner's art (a full bag is recorded not
  observed).
- `heroic-still` (acceptance): `mark1probe heroic 20` and `mark1set stat`:
  at least one Mark 1 placed line reads `rarity=6 tier=5`, `gate=on`, and
  `heroic-calls=` is up by 20.
- `spawn-tier` (acceptance): `mark1probe spawn set`, `lootshow`, and one
  spawned piece hovered and screenshotted: four lines `dressed=yes tier=5`,
  and the tooltip reads "Tier SS".
- `kill-natural` (acceptance), `dbg-unique` (research): `mark1probe copies
  400`, `mark1set stat` and the candidate's count recorded; the waypoint to
  `Act_01_01`, then monsters killed (by hs-drive input; by the owner only if
  that fails, and recorded so), with every chest met opened and every pile
  broken on the way, since those reach `DropItem` too (RDM § 13.1), until
  the candidate's count has risen by at least 1, or for 30 minutes; then
  `mark1set stat`. No by-name call runs in this step, so only the game's own
  direct call moves the candidate. `dbg-unique` is the candidate's rise,
  recorded whole. `kill-natural` passes when the candidate rose by N, at
  least 1, and `kill-calls=` rose by the same N: the direct call from
  `LoadDrops`, the one a kill makes, reached the hook the player build
  ships. That is the only check of this session the script table cannot
  satisfy; `drops-on` and `reach-kill` call the roll by name and so enter
  the hook through the table. A Mark 1 piece on the ground from a kill,
  with its `kill-<key>` ours and built each up by 1, is recorded when seen
  and is not required, because whether a kill's one draw lands on a
  stand-in is chance.
- `drop-off` (acceptance): `mark1set drops 0`, `mark1probe unique 20` and
  `mark1set stat`: at least one item placed, `pieces=` all 0, and
  `injected=` and `kill-pushed=` unchanged since before the step.
- Last: detach, stop the game, inspect the saves, restore this session's
  backup and inspect again.

The session passes on `dll-hash`, `control`, `marker`, `dbg-control`,
`kill-control`, `drops-on`, `reach-kill`, `tier-read`, `sig-untouched`,
`filter-shown`, `drop-built`, `drop-art`, `tier-ss`, `heroic-still`,
`spawn-tier`, `kill-natural` and `drop-off`. `pool-kill` and `dbg-unique`
are research and never required; `filter-control` is `filter-shown`'s
control. A check whose control failed (`kill-control`, `dbg-control`) is
recorded `not-run (control)`.

`kill-natural` is read one of three ways, and `### Phase 3 decision`'s
`kill-natural:` line takes the matching value:

- `seen`: it passed. The shipped route sees a kill.
- `blind`: the candidate's count rose and `kill-calls=` did not rise by the
  same amount. The session fails: the kill route's hook does not see the
  game's own call, whatever the by-name checks showed. It is never read as
  "kills do not roll".
- `not-observed`: the candidate's count did not rise in the 30 minutes.
  This is not a pass. The session is inconclusive for the kill route,
  nothing is written saying the pieces drop from kills, and the feature's
  pull request is not opened on the by-name checks alone. The next sitting
  needs a kill whose call is made reliably, and the owner chooses how: a
  longer kill window, or, in a research build and only with the owner's
  acceptance, the lever behind § 13.1's measurement of type 45 (every gate
  opened on one monster), which this build does not carry. It is never read
  as "kills do not run the roll", because type 45's chance per kill is not
  read.

`filter-shown` is read against `filter-control`. A pass needs no control. A
fail while `filter-control` passed is a defect of ours: the tier we write is
not what the filter reads. A fail while `filter-control`'s item was hidden
too says the owner's filter hides SS items without affixes, and is recorded
so, with both screenshots. A fail while `filter-control` was not observed,
or found only tier-5 items with affix lines, cannot tell those two apart and
is recorded `not-run (control)`, not a fail; the next sitting brings a
matching control before the switch goes on.

### Mod live procedure 6

A short session on the research build that carries `mark1probe killgate`,
the lever the owner chose on 2026-10-20 (route (a)) after Mod live procedure
5's `kill-natural` saw no natural call in 30 minutes. It reruns
`kill-natural` with the lever on, reruns `filter-shown` with
`filter-control` in the same session, and checks that the lever, once off,
leaves the gate vanilla. The capture is the drops workorder's live capture
3, `forgepact-166-mark1-set-drops-live-3.md`, kept with the hub's workorder
files and not tracked. Each check reads as Mod live procedure 5 defines it,
except where this procedure says otherwise.

**The lever** (research build only; the player build carries none of it).
`mark1probe killgate 1` turns it on, `killgate 0` off, and `killgate` alone
prints `mark1probe killgate: lever=<on|off>
hook=<detoured|table-only|refused|none> loaddrops=<n> arrays=<n>
c45-nonzero=<n> c45-max=<v> game45=<n> raised=<n> reentered=<n>
unique-during=<n> threw=<n>`. It works only inside the game's own
`LoadDrops` call, around the trampoline of the `LoadDrops` detour, at most
once per `chances` array (one `DropItem` passes one array to all its
`LoadDrops` calls, RDM § 13.1):

- **raise:** when the game itself calls `LoadDrops` with type 45 ("a unique
  item") on an array the lever has not handled, `chances[45]` is opened for
  that call and put back after it;
- **re-enter:** on the first game call of any other type on such an array,
  the game's call runs untouched, then the trampoline runs once more at
  type 45 with `chances[45]` opened, and the value is put back. This is the
  mechanism of `dungeonkey typemap`, which is § 13.1's measurement of
  type 45.

What calls `DropUniqueItems` is `LoadDrops`' own code; the draw, the filter
and the placement are the game's. The lever never calls `DropUniqueItems`
and rolls no die. The opened value goes back on every way out, a throw
included (`threw=`), and a call nested inside an opened one passes
untouched. The first `killgate 0` or `1` installs the `LoadDrops` detour
inline by its SDK name; a hook already there table-only (the typemap,
`fullprobe` and `probe` forms install it that way) would miss `DropItem`'s
direct call, so the lever refuses it with one line and nothing changed. With
the lever off every call passes through with nothing written and nothing
re-entered, and the counters still move: `loaddrops=` counts every call the
detour sees, `arrays=` the distinct arrays, `c45-nonzero=` and `c45-max=`
type 45's vanilla chance on them, read before anything else, and `game45=`
the game's own type-45 calls. `unique-during=` is how far the shared
`DropUniqueItems` detour's `kill-calls=` rose inside opened calls. The
harness (`tests/mark1_probe_harness.cpp`, labels `killgate/*`) pins each of
these and the exact text.

**Setup:**

- **Build:** the research build from `plugin_build\build.bat dev` at the
  ForgePact commit that adds the lever; its SHA-256 and `LoadDrops`'
  expected entry bytes are recorded in the capture. The owner is asked
  before it is installed. The Mod live procedure 5 DLL still installed is
  not this build.
- **Character:** save slot 14, "Sorak", in `Town_01_rm`. Every session
  restores its own backup at the end, so this one starts as Mod live
  procedure 5 started, with no Mark 1 piece in the bag. No flight charm
  worn; `mark1set bonus`, `jumpscenery`, Headhunter, Tyrant's Crown, every
  flight charm drop switch and `dungeonkey` stay off. The owner's loot
  filter stays as it is.
- **Control:** `ping` answers `pong (YYTK 4.0.1)`, and a bare `mark1probe`
  answers `mark1probe: research build` with its rows line.
- **Cases:** one ordinary kill route (the lever on, monsters in
  `Act_01_01`), the lever off (natural kills, the baseline), one piece
  placed by name for the filter, and one tier-5 item without affixes as the
  filter's control. Bosses and other acts are not run (not observed live).
- **Why the filter can be read this time.** Mod live procedure 5 ran
  `lootshow` and then judged labels drawn after it. This session runs no
  `lootshow` at all, and reads the filter's verdict from the game's own
  counter as well as from a picture: `lootcensus` before and after one
  `mark1probe unique 1`. When that call placed exactly one item, `ground=`
  rises by 1 and `hidden=` by that item's verdict (0 shown, 1 hidden). The
  player stands still in town for the filter steps, so no other ground item
  appears, and waits about 1 s before the second census, since the game
  applies the verdict within 0.3 s.
- **Debugger:** the hub's x64dbg MCP, attached under the hs-drive lease, only
  if the decompile index's build hash is this build's (else the session is
  aborted). Three non-breaking logpoints: the control on `CheckTalentUse`,
  armed first, which logs about once a frame; then `dbg-unique` on
  `DropUniqueItems`' own entry and `dbg-loaddrops` on `LoadDrops`' own
  entry. The locations and expected entry bytes are in the capture and the
  hub's x64dbg guide, never in this doc. Both candidates are armed before
  the switch and the lever install their detours, so they see the game's
  bytes; a candidate refused for a byte mismatch is recorded `not-run` and
  is not re-armed without bytes. Once the detours are in, a by-name call
  enters through the script table and skips the entry, so the candidates
  count direct calls only. The lever's re-entry calls the trampoline, so
  `dbg-loaddrops` does not count it, while its inner direct call to
  `DropUniqueItems` does reach `dbg-unique`. The debugger detaches before
  the game is stopped.

**Steps and checks**, in the order they run:

- `dll-hash` (validity): the installed DLL's SHA-256 equals the build's.
- `control`, `marker` (validity): `ping` and `mark1probe` answer as above.
- `dbg-control` (validity): attached; the control, then `dbg-unique` and
  `dbg-loaddrops` armed; the control logs about once a frame.
- `kill-control` (validity): with the drops switch off, `mark1probe unique
  10` places at least 8 items, and `dbg-unique` rises by 10.
- `filter-control` (control for `filter-shown`): still with the drops
  switch off and no `lootshow` at any point of the session, `lootcensus`,
  `mark1probe unique 1`, about 1 s, `lootcensus`, repeated up to 30 calls
  until a call placed one item whose line reads `tier=5`; that item hovered
  and its tooltip screenshotted. It passes when the tooltip lists no affix
  lines and that call's census rises are `ground=` +1 and `hidden=` +0.
  Rises of +1 and +1 mean the item is hidden, recorded `fail`. A tier-5
  item with affix lines does not count; none in 30 calls is recorded not
  observed.
- `drops-on` (acceptance): `mark1set drops 1` replies `drops=on
  install=installed`, and `mark1set stat` shows `kill-hook=detoured`.
- `filter-shown` (acceptance): `mark1probe copies 400`, then `lootcensus`,
  `mark1probe unique 1`, about 1 s, `lootcensus`, repeated up to 10 calls
  until a call placed exactly one item whose line names `piece=<key>` with
  `rarity=6 tier=5`; the ground around it screenshotted. It passes when that
  call's census rises are `ground=` +1 and `hidden=` +0 and the screenshot
  shows a `Mark 1 <piece>` label.
- `lever-sees` (validity), `dbg-loaddrops`, `natural-45` (research):
  `mark1probe killgate 0`; `mark1probe killgate`, `mark1set stat` and both
  candidates' counts recorded; the waypoint to `Act_01_01`, then monsters
  killed by teleporting to them and right-click casting (hs-drive input; the
  owner only if that fails, and recorded so), every chest met opened, for 10
  kills or 5 minutes; then the same three readings again. Expected
  `lever=off hook=detoured` and `raised=0 reentered=0`. `lever-sees` passes
  when `loaddrops=` rose by N, at least 1, and `dbg-loaddrops` rose by the
  same N: the lever's hook sees the game's own direct calls. `natural-45`
  is `arrays=`, `c45-nonzero=`, `c45-max=` and `game45=`, recorded whole;
  `dbg-unique`'s and `kill-calls=`' rises are recorded beside them.
- `kill-natural` (acceptance), `dbg-unique`, `lever-opened` (research):
  `mark1probe killgate 1`, `mark1probe killgate` recorded; monsters killed
  the same way, chests opened, until `dbg-unique` has risen by at least 3
  since the step began, or 15 minutes; then `mark1probe killgate` and
  `mark1set stat`. `lever-opened` is `lever=on` with `reentered=` or
  `raised=` above 0 and `threw=0`, with `unique-during=` recorded.
  `kill-natural` passes when, across this step and the previous one
  together, `dbg-unique` rose by N, at least 1, and `kill-calls=` rose by
  the same N. A Mark 1 piece on the ground from a kill, with its
  `kill-<key>` ours and built each up by 1, is recorded when seen and is not
  required.
- `lever-off` (acceptance): `mark1probe killgate 0`; 5 monsters killed, or
  3 minutes; `mark1probe killgate`. It passes on `lever=off` with `raised=`
  and `reentered=` unchanged since the step began while `loaddrops=` rose:
  the hook still sees the game's calls, so the unchanged counts are not
  blindness.
- Last: `mark1set drops 0`, detach, stop the game, inspect the saves,
  restore this session's backup and inspect again.

The session passes on `dll-hash`, `control`, `marker`, `dbg-control`,
`kill-control`, `lever-sees`, `drops-on`, `filter-shown`, `kill-natural`
and `lever-off`. `filter-control` is `filter-shown`'s control.
`dbg-loaddrops`, `natural-45`, `dbg-unique` and `lever-opened` are research
and never required. A check whose control failed is recorded `not-run
(control)`: `kill-natural` when `kill-control` or `lever-sees` failed, and
`filter-shown` as Mod live procedure 5 says.

`kill-natural` takes `seen`, `blind` or `not-observed` exactly as Mod live
procedure 5 defines them; the roll stays the game's own call, so `seen` is
still its only accepted value. Under the lever, `not-observed` with
`reentered=` above 0 says the type-45 path did not call `DropUniqueItems`;
that is recorded as a finding and is not a reading that kills do not roll.
If `natural-45` reads zero on every array, ordinary kills of those monsters
never run the roll the kill route hooks: that is a product finding for the
owner and a Known Limitations entry, never a reason to ship the lever.
`filter-shown` is read against `filter-control` as Mod live procedure 5
says.
