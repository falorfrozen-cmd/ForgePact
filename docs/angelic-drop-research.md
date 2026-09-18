# Angelic / Unholy düşüş araştırması (2026-09-07)

Durum: 1.3.14'te panele "Angelic / Unholy Drops" kaydırıcısı kondu (ForgePact'in kendi zarı).
Vanilla mekanizma bu notta; "vanilla-sadık" mod (seçenek 2) henüz yapılmadı.

Bu not ForgePact'in diğer araştırma belgeleriyle aynı kuralla yazıldı: ölçülen
davranış, script/nesne adları ve indeksleri, bizim kendi kodumuz ve
komutlarımız — no decompiled script text. Oyunun kodu yerelde okundu; ne
yaptığı burada kendi sözlerimizle, kapı kapı anlatılıyor, çağrı dizisi olarak
değil (hub `AGENTS.md` › "Legal: Decompiled Output Never Reaches Any Origin").
`tests/test_research_docs_no_decompiler_output.py` bu belgeyi bu standartta tutar.

## Oyunun kendi mekanizması (statik + canlı doğrulandı)

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

## Sıradaki adım (seçenek 2, vanilla-sadık)

Kaydırıcı oyuncuya buff 332 versin (plugin'de `BuffAdd` yolu var, Headhunter kullanıyor);
oyun kendi zarını, kendi bölge listesini ve ağırlıklarını kullanır. x2 = Blood Pact
modifier'ının en düşük kademesinin X değeri (oyundan okunacak; `GetBloodPactInfo` kancası
var). Kısa canlı test gerekir: buff süresi, coop'ta toplanma, X'in ölçeği.
