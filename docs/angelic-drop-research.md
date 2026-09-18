# Angelic / Unholy düşüş araştırması (2026-09-07)

Durum: 1.3.14'te panele "Angelic / Unholy Drops" kaydırıcısı kondu (ForgePact'in kendi zarı).
Vanilla mekanizma bu notta; "vanilla-sadık" mod (seçenek 2) henüz yapılmadı.

## Oyunun kendi mekanizması (statik + canlı doğrulandı)

- `gml_Script_DropItem` her öldürmede oyuncuda **buff 332** var mı diye bakıyor
  (`GetBuff(332)` inline; kapı `test al,al / je`, DropItem+0x1ADEA). Buff yoksa Angelic
  zarı hiç atılmıyor. Canlı: 984 öldürme, 0 zar (oyuncuda buff yoktu).
- Buff 332'nin adı: `buff_angelic_chance` = "Angelic Item drop chances increased by X".
  Kaynakları: Blood Pact modifier'ı `blood_pact_angelic_rate` ("Angelic/Unholy drop rate")
  ve zindan modifier'ı `dungeon_angelic_rate` ("Angelic item drop rates").
- Buff varsa: DropItem `X = Σ m_GetBuffValue(332)` (oyuncu başına toplanıyor, `ADD`)
  hesaplayıp `DropItemAngelicChance(x, y, X, undefined)` çağırıyor.
- `DropItemAngelicChance`: `lootListUnique` (Loot_Manager_obj üstünde, [tip, indeks]
  dizileri) içinden aday seçer, `GetUniqueRepoStruct(tip, alt, indeks)` ile tanımı alır,
  `GetBaseItemInfo(40)` **açık olanları atlar** (40 = gizli/dev eşya: Dev Charm, DEVELOPRE
  BOOT, Elemelon... 9 tane), sonra `irandom(tanım.droprate.base) < X` ise
  `CreateDefaultParams(j, b, true)` + `LootGroundCreate` ile düşürür.
- Tanım bayrağı: `c = 1` unique repo demek (angelic'e özel bayrak yok; Angelic/Unholy
  eşyalar unique deposundaki ayrı girişler). Kayıt biçimi `{w,j,b,a,c}`; `j` silah alt türü.
- Okunan `droprate.base` değerleri: Marcher's of Hatred 4.266.000, Annihilator 4.158.450,
  Tayrel's Chestplate 25.000.000 (dropPlaces "The Circle of Hatred", chaseDropRate 0.25),
  Lucifer's Crown 111.111.111. Görülen X: 2.148-3.586 (bugün), 204.944-216.469 (5 Eylül).
- `DropItemAngelic` (garantili üretici) bölge listesinde aday yoksa sonsuz döngüye giriyor;
  oyuncu bağlamında çağırınca oyunu dondurdu. **Asla çağırma.**
- `@anon@` metot rutinleri (GetItemDef, AddStat, GenerateItemHash) YYTK ile isimden
  çözülmüyor (status 14). Global scriptleri kullan.

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
  suspected hazard; it is not proven to be the cause, and in-game confirmation is pending.
  `tests/test_headhunter_dispatch.py` (drop scenarios) and `tests/test_kill_drop_contract.py`
  pin the order.

## Sıradaki adım (seçenek 2, vanilla-sadık)

Kaydırıcı oyuncuya buff 332 versin (plugin'de `BuffAdd` yolu var, Headhunter kullanıyor);
oyun kendi zarını, kendi bölge listesini ve ağırlıklarını kullanır. x2 = Blood Pact
modifier'ının en düşük kademesinin X değeri (oyundan okunacak; `GetBloodPactInfo` kancası
var). Kısa canlı test gerekir: buff süresi, coop'ta toplanma, X'in ölçeği.
