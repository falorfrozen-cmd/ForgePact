# Angelic / Unholy düşüş araştırması (2026-09-07)

Durum: 1.3.14'te panele "Angelic / Unholy Drops" kaydırıcısı kondu (ForgePact'in kendi zarı).
Vanilla mekanizma bu notta; "vanilla-sadık" mod (seçenek 2) henüz yapılmadı.

Bu not ForgePact'in diğer araştırma belgeleriyle aynı kuralla yazıldı: ölçülen
davranış, script/nesne adları ve indeksleri, bizim kendi kodumuz ve
komutlarımız — no decompiled script text. Oyunun kodu yerelde okundu; ne
yaptığı burada kendi sözlerimizle, kapı kapı anlatılıyor, çağrı dizisi olarak
değil (hub `AGENTS.md` › "Legal: Decompiled Output Never Reaches Any Origin").
`tests/test_research_docs_no_decompiler_output.py` bu belgeyi bu standartta tutar.

## Oyunun kendi mekanizması (statik okuma; canlı ölçülen yalnızca buff yokken zarın hiç atılmaması)

- `gml_Script_DropItem` her öldürmede oyuncuda **buff 332** var mı diye bakıyor;
  bu kontrol DropItem'in içinde ve sonucu tek bir evet/hayır kapısı. Buff yoksa Angelic
  zarı hiç atılmıyor. Canlı: 984 öldürme, 0 zar (oyuncuda buff yoktu).
- Buff 332'nin adı: `buff_angelic_chance` = "Angelic Item drop chances increased by X".
  Kaynakları: Blood Pact modifier'ı `blood_pact_angelic_rate` ("Angelic/Unholy drop rate")
  ve zindan modifier'ı `dungeon_angelic_rate` ("Angelic item drop rates").
- Buff varsa şans değeri X, buff 332'nin oyuncudaki değerlerinin toplamı (buff
  `ADD` türünde, yani birden fazla kaynak üst üste biner). DropItem bu X'i ve
  öldürmenin konumunu `DropItemAngelicChance`'e verir; ne düşeceğine o karar verir.
- `DropItemAngelicChance` adayı `lootListUnique`'ten seçer (Loot_Manager_obj üstünde,
  her giriş bir tip ve bir indeks çifti) ve eşyanın tanımını unique deposundan
  (`GetUniqueRepoStruct`) okur. Temel eşya bilgisinde **40 numaralı bayrağı açık olan
  adayları atlar** (40 = gizli/dev eşya: Dev Charm, DEVELOPRE BOOT, Elemelon... 9 tane).
  Kalan aday için zar, sıfırdan tanımın `droprate.base` değerine kadar düzgün bir
  tamsayı; zar X'in altında kalırsa eşya varsayılan parametrelerle
  (`CreateDefaultParams`) kurulur ve `LootGroundCreate` ile yere düşer. Yani bir
  eşyanın Angelic şansı kabaca X / `droprate.base`.
- Tanım bayrağı: `c = 1` unique repo demek (angelic'e özel bayrak yok; Angelic/Unholy
  eşyalar unique deposundaki ayrı girişler). Kayıt biçimi `{w,j,b,a,c}`; `j` silah alt türü.
- Okunan `droprate.base` değerleri: Marcher's of Hatred 4.266.000, Annihilator 4.158.450,
  Tayrel's Chestplate 25.000.000 (dropPlaces "The Circle of Hatred", chaseDropRate 0.25),
  Lucifer's Crown 111.111.111. Görülen X: 2.148-3.586 (bugün), 204.944-216.469 (5 Eylül).
- `DropItemAngelic` (garantili üretici) bölge listesinde aday yoksa sonsuz döngüye giriyor;
  oyuncu bağlamında çağırınca oyunu dondurdu. **Asla çağırma.**
- `@anon@` metot rutinleri (GetItemDef, AddStat, GenerateItemHash) o denemede YYTK ile
  isimden çözülmedi (status 14 = `AURIE_OBJECT_NOT_FOUND`) - bu **not observed** demek,
  "asla çözülmez" demek değil: denenen tam isim o zaman kayda geçmemişti. Eklentinin o
  anki GenerateItemHash kopyası `@anon@4638@` yazımıydı; bu isim o zamanki hs-game-sdk
  tablosunda yoktu (tabloda o zamanki (yama öncesi) oyun sürümü için sadece `@anon@4645@`
  vardı). Oyun yaması sonrası hs-game-sdk yeniden üretildi; güncel sürümün adı
  `@anon@4791@`. Yanlış bir isim zaten status 14 üretir - tek başına yeterli bir
  açıklama. `hashprobe direct` (research build) bunu yeniden test ediyor. Global
  scriptleri kullan.

## Plugin'de olanlar

- `angelicdrop <1/N> | off | status` (player build): öldürme başına kendi zar, tutunca
  `kAngelicBases` (editör kataloğundan 58 satır, ilk kullanımda doğrulanır: isim anahtarı
  eşleşmeli, bayrak 40 kapalı, iksir değil, deneme üretimi rarity 7/10 vermeli -> 47/49 aday)
  içinden rastgele eşya, imza düşüşü yoluyla (`{"w":1,"a":seed,"j":sub,"b":b,"c":1}` ->
  InitItemFromJson -> LootGroundCreateFromItem). Canlı: 122 öldürme = 122 düşüş.
- `angeliclist` (araştırma): havuz doğrulamasını satır satır yazar.
- `angelicwatch on|status` (araştırma): kapıya dokunmadan oyunun kendi zarını sayar.
- `raredrop angelic <x>`: eski kapı yaması duruyor; şansı çarpmak yerine zar sayısını
  çarpıyor (x99'da sıfır düşüş sorunu böyle çözüldü). Panelde yok.
- Panel: `angelic_items` (1 = kapalı, 2 = 7500'de 1, her kademe bir zar daha).
- Note (1.4.3, in English): the 1.3.14 "122 kills = 122 drops" run used the old order - the
  kill hook called the game's own kill proc first, then read the enemy and spawned the drop
  with that enemy as self. Players then reported crashes at x100 on 1.4.1. 1.4.3 moves both
  kill drops (angelic and signature) ahead of the original, so they read and spawn while the
  enemy is still live, and nothing touches the enemy after the original. This removes a
  suspected hazard; it is not proven to be the cause. Live, 2026-09-18, current game build:
  the research build with this order gave `angelicdrop 1` 30 rolls = 30 drops, `fails=0`,
  items on the ground and pickable, and `sigdrop 100 100` 17 = 17; the unmodified 1.4.1
  plugin (old order) gave `angelicdrop 1` 30 = 30, `fails=0`, and neither crashed. The x100
  crash was not reproduced, so the change stands as a precaution, not a fix.
  `tests/test_headhunter_dispatch.py` (drop scenarios) and `tests/test_kill_drop_contract.py`
  pin the order.

## Sıradaki adım (seçenek 2, vanilla-sadık)

Kaydırıcı oyuncuya buff 332 versin (plugin'de `BuffAdd` yolu var, Headhunter kullanıyor);
oyun kendi zarını, kendi bölge listesini ve ağırlıklarını kullanır. x2 = Blood Pact
modifier'ının en düşük kademesinin X değeri (oyundan okunacak; `GetBloodPactInfo` kancası
var). Kısa canlı test gerekir: buff süresi, coop'ta toplanma, X'in ölçeği.

Güncelleme (#64, 2026-09-23): oyunun kendi Angelic zarı `angelicprobe` ile canlı ölçüldü -
hangi adımın kancalanacağı, `buffme 332` ile verilen değerin şansa yansımayışı ve ölçülemeyenler
`angelic-roll-hook-research.md` dosyasının `## Decision` bölümünde.

## Gold Inlaid Mysterious Potion (#229)

This section is in English. It keeps the posture of the rest of this note:
the game's behaviour in our own words, each claim labelled static reading,
measured or not established, no addresses and no decompiled script text.

The report (#229, milestone v2.4.0, related #13): the Gold Inlaid Mysterious
Potion, the game's one Angelic potion, never drops, even when every other
Angelic item has dropped several times over. Offline the game's own Angelic
roll practically never fires (a hit at the natural chance has never been
observed, hub `docs/RUNTIME_DATA_MODELS.md` § 13.4), so a player who sees
every other Angelic drop again and again is on the Loot tab's
**Angelic / Unholy Drops** slider (`angelic_items`, which sends
`angelicdrop <1/N>`): ForgePact's own die. For the slider the answer is in our
own source (below). For vanilla, Live 1 (2026-10-23, `### Results` below)
measured that the game's own Angelic list carries the potion once and that the
Angelic roll's two filters keep it, so vanilla's roll can pick it; a vanilla
drop itself was not observed.

### Static reading

Read locally through the Ghidra project, 2026-10-23; paraphrased.

- **The game's Angelic roll has no item-type rule** (static reading).
  `DropItemAngelicChance` takes an entry of `Controller_obj.lootListUnique[5]`,
  reads that entry's definition, and picks again until the hidden flag (base
  info `"40"`) is clear and the rarity is 7 or 10 (hub RDM § 13.4). Nothing in
  it tests the entry's item type; the type is handed from the entry to the
  placement unchanged. A type-18 entry that passes the two filters would drop
  exactly like any other Angelic.
- **The default parameters carry no type** (static reading, consistent with a
  measurement). `CreateDefaultParams` takes three values, the sub-type (with
  a fallback when it is absent), the base index and a unique flag, and
  returns a `{j, b, c}` record. #74 Live 2 measured
  `{"b":51.0,"j":0.0,"c":1.0}` for Liquor Holster's `{8, 0, 51}`. The roll
  passes the entry's type to the placement separately.
- **Where potions are defined** (static reading, partial).
  `DefineItemUniqueConsumable` fills repository type 11 (consumables) only.
  The type-18 entries come from another loader (`DefineItemUniqueFlask`, by
  name), whose body did not decompile within the tool's time limit. So the
  potion's definition in the game (its key, hidden flag and rarity) has
  **not been read** statically: the key and the `{18, 0, 8}` triple come from
  `kAngelicBases`, which was taken from the Item Editor's catalog. Live 1
  measured them from the running game instead (`### Results`): key
  `consumable_gold_inlaid_mysterious_potion`, hidden flag unset, built rarity 7.
- **Where the six unique lists are built** (static reading, partial). A
  read-only scan for references to the `lootListUnique` variable, with the two
  known readers (`DropItemAngelicChance` and `DropUniqueItems`) as positive
  controls, found 59 references in 17 functions: the two rolls,
  `DropItemHeroic`, `DropItemDebug`, `CreateShrineEffect`, `DoCraftResult`,
  the two merchant grids, `ReturnRandomSatanic`, the gamba machine's event and
  six unnamed functions. One unnamed function, placed just after
  `Controller_obj`'s Create-event closures (that it is the Create event is an
  inference), creates the eleven loot lists (five normal, six unique) and then
  fills them in a loop that reads unique definitions through
  `GetUniqueRepoStruct` and normal ones through `GetNormalRepoStruct`. Another
  destroys all eleven. The builder is too large to decompile here (the
  decompiler ran out of time at 30 minutes), and the rule for which entry goes
  into which list sits inline in it, so list membership is **not
  established** statically; Live 1 measured it for the two potions instead
  (`### Results`).
- **A hint, not a reading.** #74 measured 48 droppable entries in
  `lootListUnique[5]` (2026-10-13, counted with repeats) against ForgePact's
  47 validated candidates: one more droppable entry than ForgePact knows. It
  could be the potion, a repeat, or an Angelic the table lacks. Live 1 found
  the potion in `lootListUnique[5]` once, kept by the Angelic filters, and the
  validated pool at 48 with it, which is consistent with the potion being that
  48th entry; the list was not compared with the table entry by entry.

### What the plugin did

- Since `v1.3.14` (2026-09-07, the commit that added the slider),
  `BuildAngelicPool` rejected every `kAngelicBases` row whose item type is 18
  with the reason `potion`, before asking the game anything. The rule is the
  "iksir değil" item in this note's validation list above. Neither that commit
  nor this note gives a reason for it.
- The table has two type-18 rows: the Gold Inlaid Mysterious Potion
  `{18, 0, 8}` (flagged Angelic) and the Elixir of Unworldly Cognition
  `{18, 0, 5}` (flagged Unholy). HS Offline Tracker's item table, which
  ForgePact carries as `src/forgepact_tracker/item_table.py`, lists the elixir
  as Heroic and the potion as Angelic, SS grade, one in 3,850,000.
- Measured pool (#74 Live 1, 2026-10-02): 47 candidates, 11 rejected; by the
  rule, both potions were among the rejected. So with the slider on, the potion
  could never drop: the die picks uniformly from the validated pool, and the
  potion was never in it.
- Nothing else in ForgePact filters it: the die, `SpawnAngelicItem` and
  `AngelicDropOnKill` have no type rule, and the table's `angelic` flag is
  stored and read by nothing. The README row promises "one of the game's real
  Angelic / Unholy uniques", which the type rule made untrue for the potion.

### The fix

- The type rule is gone. A `kAngelicBases` row enters the pool exactly when
  the game's own answers pass the three existing tests: the repository key
  (`GetUniqueRepoStruct`'s `itemBaseInfoStruct["28"]` equals the row's key),
  the hidden flag (`"40"` unset), and the rarity the game builds
  (`InitItemFromJson` of `{"w":1,"a":<seed>,"j":<sub>,"b":<b>,"c":1}` must give
  `itemInfoStruct["27"]` 7 or 10). No row is rejected, or kept, for its type.
- The table, the uniform die, `SpawnAngelicItem`'s build and placement, the
  retry rule and the `angeliclist` line format are unchanged.
- Measured on the real game (Live 1, `### Results`): the potion validates
  (`-> ok`) and the pool reads 48 candidates, 10 rejected; the elixir is
  rejected by its built rarity (`game makes it rarity 9`), as the Tracker's
  table predicted. Whatever the game answers decides; neither row is
  special-cased.
- If the live session shows the potion refused by one of the three tests, or
  built but not usable as an item (no ground label, no pickup, gone after a
  save), the slider route is refused and the fix is redesigned from that
  evidence, not patched with a type exception.

### Instrument: angelicprobe pool

Two `angelicprobe` subcommands in the research build only (the existing
`angelicprobe` block, which the player build does not compile). Both refuse,
as `angelicprobe`'s other list verbs do, while a roll's stand-in entries are
on a list.

- `angelicprobe pool find <t> <s> <b>` is read-only. It prints one line with
  the definition's key, rarity and hidden flag (base info `"28"`, `"27"`,
  `"40"`, the values the rolls read), whether the Angelic roll's two filters
  keep it (`angelic-keeps`) and whether the ordinary unique roll's would
  (`ordinary-keeps`: rarity below 7 and not hidden, hub RDM § 23.4), then how
  many entries of each of the six `lootListUnique` sub-lists equal the whole
  triple, and the six list sizes. The lists are read by name, never through an
  address.
- `angelicprobe pool drop <t> <s> <b>` builds the pool if needed, finds the
  triple among the validated candidates, resolves the local player as an
  instance handle, and calls the production `SpawnAngelicItem` with the player
  as context, 48 px to the player's right. It prints `SpawnAngelicItem`'s own
  `angelicdrop:` line, then the ground-item count before and after (counted as
  `lootannprobe`'s census counts `Loot_Ground_obj`). A triple that is not a
  validated candidate is refused with the pool size. It never touches
  `angelicdrop`'s counters.

### Live procedure 1

Run 2026-10-23 by the live operator; capture
`.claude/workorders/forgepact-229-gold-inlaid-potion-live-1.md` (hub, local;
not tracked). Research build of the plugin at the branch head with the fix and
`angelicprobe pool` (the installed DLL's sha256 matched the build's), character
slot 14, Hell, Outskirts of Inoya (zone level 243), no debugger. The save
folder was backed up before launch and restored after, and the restore checked
clean.

- Controls first: `ping` answered, and `angelicdrop status` read off with
  zero counters before anything ran.
- `angeliclist` away from the waypoint, every verdict recorded.
- `angelicprobe pool find` for Liquor Holster `{8, 0, 51}` (the positive
  control: hub RDM § 13.4 has it once in `lootListUnique[5]` only), the potion
  `{18, 0, 8}` and the elixir `{18, 0, 5}`.
- `angelicprobe pool drop` for Liquor Holster, then the potion, each followed by
  a screenshot of its ground label.
- Pickup by hovering the potion's label and pressing F, then the bag (C).
- Drinking it, then Save & Exit, reloading the character and opening the bag.
- The slider at `angelicdrop 1` (one roll per kill) over a little more than 20
  kills, then `angelicdrop off`.

No step needed the owner: the operator reached the zone and drank the potion
through the game's own input. The procedure expected the `angelicdrop:` line to
end in the item's repository key; it prints the display name twice
(`<name> -> <name> rarity 7 at <x>,<y>`), as `SpawnAngelicItem` always has.
That was a wrong expectation in the procedure, not a fault in the drop: the
placement, the ground count and the label all matched (the owner read the
session as passing on that basis, 2026-10-23).

### Results

Measured in Live 1 (2026-10-23) unless a line says otherwise.

- **The fix works on the slider's pool.** `angeliclist` reads
  `Gold Inlaid Mysterious Potion (18/0/8) -> ok`, and the pool is
  `48 candidates, 10 rejected`: the 47 measured before the fix plus the potion.
  The ten rejected are nine hidden or developer items (info flag `"40"` set)
  and the elixir.
- **The elixir is Heroic in the game.** `angeliclist` reads
  `Elixir of Unworldly Cognition (18/0/5) -> game makes it rarity 9`, so the
  rarity test keeps it out, as the Tracker's table predicted. Its `angelic`
  flag in the table is still read by nothing.
- **The potion's definition**, read from the game by `pool find`: key
  `consumable_gold_inlaid_mysterious_potion`, built rarity 7, hidden flag
  unset. The elixir's key is `consumable_elixir_of_unworldly_cognition`, rarity
  9, hidden flag unset.
- **Vanilla's Angelic list carries the potion.** With the control passing
  (Liquor Holster: `[5]=1`, `[0]` to `[4]` all 0, `angelic-keeps=yes`), the
  potion read `[5]=1` and `[0]` to `[4]` all 0, with `angelic-keeps=yes`. The
  elixir is on `lootListUnique[5]` once as well, but `angelic-keeps=no` (rarity
  9). The six sub-list sizes were 50, 61, 79, 152, 221 and 380.
  `angelic-keeps` is the plugin applying the Angelic roll's two filters (hidden
  flag clear, rarity 7 or 10) to the game's own answers, and that the roll uses
  only those two is a static reading (above). So the game's own Angelic roll can
  pick the potion, one entry among the list's others; a vanilla drop of it was
  **not observed**, and offline that roll practically never fires (hub RDM
  § 13.4).
- **A forced drop lands like any other.** `angelicprobe pool drop 8 0 51` and
  `pool drop 18 0 8` each printed `SpawnAngelicItem`'s line at rarity 7 and the
  ground count went up by one (7 to 8, then 8 to 9); the screenshots show the
  labels `Liquor Holster | SS (1)` and `Gold Inlaid Mysterious Potion | SS`.
  The owner's loot filter was not changed.
- **It can be picked up.** Hovering its label and pressing F gave
  `[Gold Inlaid Mysterious Potion] Picked up`; it went to the bag grid, not the
  potion slots. Its tooltip read `Potion`, `Currently Has 0 Charges out of 1`,
  a 45 second flask cooldown, a 15 second effect, Tier SS, level 1.
- **It survives a save.** After Save & Exit and reloading the character the
  potion was still there (in potion slot 4, where the operator had moved it),
  now with `1 Charges out of 1`.
- **It works when drunk, once it has a charge.** With 0 charges, neither a
  right-click in the bag nor the potion slot's key drank it. After the reload,
  with its charge back, the slot's key gave a 15 second buff, `Endless Wealth`:
  Magic Find increased by 275% and extra gold from kills 26%. The potion stayed
  in its slot afterwards: it is a recharging flask, not a single use. In both
  tries the key drank every charged potion slot at once, recorded as seen.
- **The slider drops it.** At `angelicdrop 1`, 24 kills gave 24 rolls, 24 drops
  and no failures from the 48-candidate pool; two of those drops were the
  potion.

### Not established

- What refills the potion's charge: it was picked up empty and had its charge
  after a save and reload. Whether time, kills or the reload refilled it was
  not measured.
- A vanilla drop of the potion: not observed (above). The list entry and the
  filters say the roll can pick it; the roll was not seen doing so.
- Whether `lootListUnique[5]` holds an Angelic or Unholy entry that
  `kAngelicBases` lacks: the 48 candidates match #74's 48 droppable entries in
  count only.
- Not run in Live 1, so not observed live: the other Angelic and Unholy rows
  through `pool drop` (they take the same path and dropped from the slider
  before the fix), another difficulty or zone tier, and co-op.
