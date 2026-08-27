# Blood Pact modifiyerlerini offline bulma — çalışma notu

Tarih: 2026-08-27
İstemci: `Hero_Siege.exe` 303.708.672 B (7.0.30)
Rutin tablosu: `generated/s10_routines_7030.json` (20.852 rutin, 15/15 hedef bulundu)

Amaç: oyunun **Edit Blood Pact** ekranındaki 30 modifiyerden ForgePact'te
karşılığı olmayan 22 tanesini offline olarak bulmak.

---

## 1. Kapatılan yanlış yol: `blood_pact_*` isimleri

`s10_varmap_current.json` içinde tam da aradığımız isimler duruyor:
`blood_pact_magic_find`, `blood_pact_rune_rate`, `blood_pact_monster_life`, …
82 tane. İlk bakışta doğrudan hedef gibi görünüyorlar.

**Hepsi çeviri anahtarı, hiçbiri değişken değil.**

Ayırt etme yöntemi (YYC): bir GML değişken adı için ikili hücre bulunur —
`[ isim_işaretçisi (8 B) ][ slot_id = 0xFFFFFFFF (4 B) ]`. Slot çalışma anında
doldurulur, dosyada `-1` durur. Böyle bir hücre yoksa isim yalnızca bir metin
sabitidir. (Aynı kural `plugin/ModuleMain.cpp`'nin isim çözümlemesinin
dayandığı `rip_target − 8` kuralının kaynağı.)

192 aday isim tarandı, yalnızca 4'ü gerçek değişken çıktı:
`magic_find`, `movement_speed`, `showExpGain`, `m_RemoveSubSkillPoint`.

`blood_pact_*` isimlerinin tamamı `translationsBloodPact.csv`'nin anahtarları,
yani Edit ekranındaki **etiket metinleri**.

Aynı şekilde `GPV` / `SPV` (Get/Set Pact Value) çağrılarında da anahtar dizesi
yok — modifiyerler **sayısal kimlikle** saklanıyor, isimle değil. Blood Pact
verisi zaten sunucudan `&pact_data=` ile geliyor (`httpBloodPactJoin`,
`httpBloodPactRefresh`, …), yani çevrimdışı bir istemcide "Blood Pact'i aç"
diye bir yol yok. Her modifiyeri **etkisinden** yakalamak gerekiyor.

## 2. Çalışan yol: global isim listesi

Eklentiye eklenen `gnames [filtre]` komutu (`variable_instance_get_names(-5)`)
çalışan oyundan **3477 global** döküyor. Blood Pact satırlarının birebir
karşılıkları burada duruyor:

| Blood Pact satırı | oyunun kendi adı |
| --- | --- |
| Magic Find | `StatMagicFind` |
| Attack Speed / Cast Rate | `StatAttackSpeed`, `StatFasterCastRate` |
| Experience gain | `StatExperienceGain` |
| Total Movement Speed | `StatMovementSpeed` |
| Total Damage dealt | `StatTotalDamage` |
| Extra Gold per Monster kill | `StatExtraGold` |
| Rune drop rate | `DropRunes`, `DropSpecialRunes` |
| Orb drop rate | `DropOrbs` |
| Boss Gem drop rate | `DropBossGems` |
| Extra ore drops | `DropOres`, `DropOreMaterials` |
| Scroll of Ra drop rate | `DropScrollOfRa` |
| Satanic drop rate | `DropSatanicCrystal`, `DropSatanicFragment`, `DropSatanicDice` |
| SS Satanic/Heroic drop rate | `DropItemHeroic` |
| Angelic/Unholy drop rate | `DropItemAngelic`, `DropItemAngelicChance` |
| Prime Evil part drop rate | `DropBossParts`, `DropUberParts` |
| Dimensional Shards | `DropDimensionalShard` |
| Battle Fragments | `DropBattleFragments` |
| Colosseum Fragments | `DropColossusFragment` |
| Disable unique drops | `DropUniqueItems` |
| Extra monster life/damage | `SetAllModifiersNew`, `LoadMonsterDropModifiers` |

## 3. Eşya kategorisi haritası (yeni)

`GetNormalRepoStruct(kategori, 0, indeks)` ile kategoriler tarandı. Bugüne
kadar yalnızca 12 (Keys) ve 16 (Relic) biliniyordu; tamamı:

| kat | içerik | adet |
| --- | --- | --- |
| 0 | helmet | 15 |
| 1 | armors | 20 |
| 2 | boots | 15 |
| 4 | gloves | 20 |
| 5 | amulets | 25 |
| 6 | shields | 18 |
| 7 | rings | 30 |
| 8 | belts | 15 |
| 10 | charms | 60 |
| 11 | consumable | 27 |
| **12** | **keys** | 44 |
| **13** | **collectible** | 65 |
| **14** | **material** | 74 |
| **15** | **socketable** | 200 |
| **16** | **relic** | 156 |
| 18 | flask | 16 |
| 19 | vault | 7 |

3, 9, 17, 20–25 boş (silahlar başka bir alt-tiple geliyor olmalı).

### Blood Pact satırlarının kategori karşılığı

**kategori 13 — collectible**

| indeks | eşya | vanilya base |
| --- | --- | --- |
| 0 / 41 | `collectible_battle_fragment` (+infernal) | 105 / 155 |
| 1 / 42 | `collectible_dimensional_shard` (+infernal) | 124 / 184 |
| 2–7 | Prime Evil parçaları (`gurags_soul`, `deaths_sigil`, `damiens_eye`, `anubis_ankh`, `karp_kings_bellybutton`, `satans_horn`) | 27 |
| 43–48 | aynılarının infernal hâli | 35 |
| 17 / 52 | `collectible_scroll_of_ra` (+infernal) | 34 / 48 |
| 18 / 53 | `collectible_colosseum_fragment` (+infernal) | 54 / 84 |

**kategori 15 — socketable**

| grup | adet | vanilya base |
| --- | --- | --- |
| runeler (`el`…`zod`, 33 tane) | 33 | 350 → 334.800 |
| `socketable_orb_*` | 18 | 11.000 – 14.400 |
| `socketable_gem_*` | 14 | 9.000 / 11.000 |
| chipped/flawed/flawless taşlar | 21 | 50 / 100 / 200 |
| `socketable_jewel_*` | 30 | 410 |

**kategori 14 — material**

`material_mining_*` (copper, iron, gold, jade, ruby, tarethium) — 6 eşya,
hepsi base 50.000.000, yani doğal düşüşü **yok**; madencilikle geliyor.

---

## 4. Bilinen boşluk

Kategori düzeyinde oran ayarlamak tek başına yetmeyebilir. Anahtarlarda
öğrenildiği gibi sistem iki kademeli: `LoadDrops` önce bir **damla tipi** zarı
atıyor (`chances[tip]`), tip geçmezse eşyanın kendi `droprate.base` oranı hiç
denenmiyor. Ev bölgesi dışında bazı ailelerin tipi 0 şansla atılıyor.

Bunu ölçmek için eklentiye `dungeonkey fullprobe <N>` eklendi: sonraki N
`LoadDrops` çağrısında `chances` dizisinin **tamamını** ve tüm argümanları
`bp_ipc\chances.txt` dosyasına döküyor. Sıfırdan büyük her indeks o canavarın
attığı bir zar — rune/orb/gem/ore tiplerinin numaraları buradan çıkacak.

**Bu ölçüm yapıldı — sonuçlar aşağıda 6. bölümde.** Dizi 70 slot çıktı ve
normal bir canavarda yalnızca 9'u sıfırdan büyüktü, yani boşluk gerçekti.

## 5. Eklenen araştırma komutları (yalnızca dev derleme)

| komut | ne yapar |
| --- | --- |
| `gnames [filtre]` | tüm global değişken adlarını döker |
| `ijson <değişken>` | oyuncunun bir örnek değişkenini json'a döker |
| `ojson <nesne> <değişken>` | herhangi bir nesnenin değişkenini json'a döker |
| `dungeonkey fullprobe <N>` | `LoadDrops` `chances` dizisinin tamamını döker |

Yardımcı çözümleyiciler `scratchpad/` altında: `varsolve.py` (isim gerçek
değişken mi), `strxref.py` (dizeyi/RVA'yı kullanan rutinler), `routinestr.py`
(bir rutinin dokunduğu tüm dizeler).

---

## 6. ÖLÇÜLDÜ — damla tipi haritası (2026-08-27 akşam)

`LoadDrops`'un 9. argümanı **70 slotluk bir şans dizisi**: her indeks bir damla
tipi, değeri o canavarın o tip için attığı zarın şansı. Normal bir canavarda
70 tipin yalnızca **9'u sıfırdan büyük** — kalan 61'i o canavar için hiç
denenmiyor. Anahtarlarda çarptığımız duvarın kaynağı bu.

Tipleri aileye bağlamak için eklentiye `dungeonkey typemap <şans> all` eklendi.
Tek yaratık ölümünde, tek karede:

1. 20 kategorideki **814 eşyanın** `droprate.base` değerini 1'e çeker
   (vanilya `g_DropRateVanilya`'da saklanır)
2. 70 tipin her biri için sırayla dış kapıyı açıp `LoadDrops`'u çağırır
3. Yaratım kancalarını (`CreateItemNew`, `GenerateItemRandomStats`) o anki
   tiple etiketler — tekilleştirme kapatılır, yoksa oturumda daha önce görülen
   eşya elenip tip "üretmedi" görünür
4. 814 eşyanın hepsini vanilyaya geri koyar

İç zarı da açmak şart: `chances[t]=100000` yalnızca dış kapıyı açıyor, eşyanın
kendi base'i (rune 350–334.800, orb 11.000+) geçmezse tip geçerli olsa bile
hiçbir şey görünmüyor. İlk denemede tam da bu yüzden 70 tipten yalnızca 12'si
ürün verdi; iç zar açıkken **34'ü** verdi.

### Sonuç — üretken 34 tip

| tip | vanilya şans | ürün |
| --- | --- | --- |
| 4 | **14 (açık)** | rune (`Tul`) |
| 6 | **45 (açık)** | taş (`Topaz`) |
| 10 | **3 (açık)** | flask |
| 12 | 0 | Treasure Key (zindan anahtarı) |
| 15 | 0 | Ninja Hook |
| 16 | 0 | Angelic Key |
| 17 | 0 | Satanic Dice |
| 18 | 0 | Ruby Key |
| 25 | 0 | **Battle Fragment** |
| 26 | 0 | Codex Page |
| 31 | **15 (açık)** | Bifröst Key |
| 33 | 0 | `Flo` (mücevher?) |
| 34 | 0 | **Scroll of Ra** |
| 35 / 36 | 0 | Eternity Codex / Infernal Codex |
| **37** | 0 | **ORB'LAR — 18 tane** (Ancient, Fatality, Doom, Earth, Gladiator, Midas, Treasure, Relic, Wisdom, Brute, Magister, Agility, Swiftness, Angel, Heroism, Kobold, Runeforge, Goblin) |
| 38 | 0 | **Colosseum Fragment** |
| 39 | 0 | **Satanic Crystal** |
| 41 | 0 | **Dislocated Eye** (Prime Evil parçası) |
| 43 | 0 | **Dimensional Shard** |
| 45 | 0 | benzersiz eşya (`helmet_zeus_circlet`) |
| 46 / 51 | 0 / **1 (açık)** | Destiny Shard |
| 47 / 52 | 0 | Gypsy's Prophecy / Prophet's Wisdom |
| 48 | 0 | Prophet's Wisdom |
| 49 | 0 | Satanic Dice |
| 50 | 0 | Blacksmith's Mallet |
| 56 / 60 | 0 | tarot kartları |
| 59 | 0 | Essence Vault |
| 61 / 63 | 0 | Tarot Deck |
| 64 | 0 | `Liquate` |

Tip 2 bu canavar için geçersiz (istisna atıyor).

### Blood Pact satırlarının karşılığı

| Blood Pact satırı | tip | dış kapı | ne gerekiyor |
| --- | --- | --- | --- |
| Rune drop rate | 4 | **açık** | yalnızca `droprate` çarpanı |
| Boss Gem drop rate | 6 (normal taş) | **açık** | yalnızca çarpan; boss taşları ayrı, henüz bulunmadı |
| Orb drop rate | **37** | kapalı | kapı + çarpan |
| Scroll of Ra | **34** | kapalı | kapı + çarpan |
| Satanic drop rate | **39** (+17/49) | kapalı | kapı + çarpan |
| Prime Evil parts | **41** | kapalı | kapı + çarpan |
| Dimensional Shards | **43** | kapalı | kapı + çarpan |
| Battle Fragments | **25** | kapalı | kapı + çarpan |
| Colosseum Fragments | **38** | kapalı | kapı + çarpan |
| Disable unique drops | **45** | kapalı | tersine — kapatma |

Yani mekanizma anahtarlarla **birebir aynı**: `dungeonkey` motorunun tip
listesine bu numaraları eklemek + `droprate group` için kategori/indeks
kümelerini tanımlamak yetiyor. Yeni tersine mühendislik gerekmiyor.

### Hâlâ bulunamayanlar

- **Extra ore** — iç zar açıkken bile hiçbir tip madencilik malzemesi
  üretmedi. Muhtemelen damla sisteminden değil, madencilikten geliyor.
- **Angelic/Unholy** ve **SS Satanic/Heroic** — bunlar eşya *tier*'ı,
  ayrı bir tip değil; `DropItemAngelic` / `DropItemHeroic` üzerinden gidiyor.
- **Oyuncu istatistikleri** (Magic Find, Attack Speed, Exp, Movement Speed,
  Total Damage) ve **canavar istatistikleri** (life/damage, pack size) —
  bunlar damla sistemi değil, `Stat*` fonksiyonları. Ayrı bir tur gerekiyor.

### Uyarı

`typemap ... all` taraması tek karede 814 eşyayı iki kez dolaşıyor ve oyunu
birkaç saniye donduruyor. Yalnızca araştırma derlemesinde var; yayın
derlemesinde bu kod hiç derlenmiyor.
