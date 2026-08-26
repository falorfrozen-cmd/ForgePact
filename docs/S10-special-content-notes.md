# ForgePact — Sezon 10 Special Content çalışma notu

Tarih: 2026-08-25
Doğrulanmış istemci: `Hero_Siege.exe` 303.302.144 B
SHA256 `0766aa8bfc6eb5679df46f78546644e34fae333adc22474f96903c1d68f251f5`
Profil: **S10-2026.08.24**

Test kurulumu (asıl oyuna dokunulmuyor):
`C:\Users\falor\Downloads\testhero siege\Hero-Siege-AnkerGames (1)\HeroSiege\bin`

S9 çalışan kaynak yedeği:
`source\ForgePact_S9_CALISAN_YEDEK\`

---

## 1. ÇÖZÜLDÜ — Sezon 9 mekanizması ve neden S10'da çalışmadığı

S9 Python arayüzü (`src/forgepact.py`) beş spawner için şunu gönderiyordu:

```
multobj <nesne_index> <carpan>
```

S9 indeksleri: rift 3516, battlefield 4990, cursedorb 5664, summonportal 3565,
chaospillars 4624.

Eklenti (`plugin/ModuleMain.cpp` → `DoMultiCreate`) `instance_create_layer` /
`instance_create_depth` çağrısını yakalayıp nesneyi N kez, konumu kaydırarak
yaratıyordu:

```cpp
a[0] = Args[0] + ((i % 5) - 2) * 28;   // x
a[1] = Args[1] + ((i / 5) - 2) * 28;   // y
orig(tmp, S, O, argc, a.data());
```

**S10'da neden çalışmıyor (ÖLÇÜLDÜ):**
`Spawn_*_obj` nesneleri artık `instance_create_layer`'dan **sıfır kez** geçiyor —
odaya gömülü geliyorlar. `creator_objects.log`'da hiç `Spawn_*` yok.
`Enemy_Creator_*` ise eskisi gibi `instance_create` ile yaratılmaya devam ediyor,
bu yüzden **density hâlâ çalışıyor** (`extra_creators=868` ölçüldü).

---

## 2. ÇÖZÜM — S9'un asıl tekniği: SpawnAtPlayer

S9'da `spawnname <NesneAdi>` komutu vardı:

```cpp
pid = instance_find(asset_get_index("Player_obj"), 0)
px  = variable_instance_get(pid, "x")
py  = variable_instance_get(pid, "y")
instance_create_depth(px, py, 0, hedefNesneIndex)
```

Oran/kapı/ZoneState'e hiç dokunmuyor. Bu teknik S10 runtime'ına taşındı
(`SpawnObjectAtPlayer`), pipe komutu: `set spawn <NesneAdi> [adet]`.

**KRİTİK:** GML builtin'leri yalnızca **oyun thread'inden** çağrılabilir.
İstek pipe'tan kuyruğa alınır, `HookMinimapStep` (oyun thread'i) içinde
`DrainSpawnQueue` ile işlenir. Önceki çökmelerin bir kısmı yanlış thread'den
çağrı yüzündendi.

---

## 3. S10 içerik nesne adları (routine tablosundan çıkarıldı)

| Özellik | Nesne | Durum |
|---|---|---|
| Chaos Pillars | `Chaos_Pillar_obj` | ✅ çalışıyor |
| Cursed Orb | `Cursed_Orb_obj` | ✅ çalışıyor (malevolent spirit çıkıyor) |
| Summon Portal | `Summoning_Portal_obj` | ✅ çalışıyor (mavi portal) |
| Rift | `Rift_Portal_obj` | ❌ yaratılıyor, görünmüyor |
| Battlefield | `Portal_Battlefield_obj` | ❌ yaratılıyor, görünmüyor |

Çalışmayan ikisi **başka bölgeye götüren geçit**; hedef bölge verisini normal
akışta spawner/zone state'ten alıyor. `Rift_Portal_obj` Create olayı 13.024 bayt,
GPV okuyor + RNG çalıştırıyor → kurulum verisi olmadan kendini yok ediyor.
Çalışan üçü bulunduğu haritada olay başlatıyor, kendine yetiyor.

---

## 4. Kapı analizi (Special Content neden doğal yoldan çıkmıyor)

Callback'lerin kapısı (Cursed Orb örneği, rva 0xAFDD320):

```
+213  xorps xmm6, xmm6        ; xmm6 = 0.0  SABIT SIFIR
+298  call  sub_c5b4ca0       ; eSt[<dinamik>] oku  -> 35
+419  call  sub_c5d5620       ; RValue karsilastirma(35, 0)
+436  setle sil               ; sil = (okunan <= 0)
      test sil,sil / je       ; degilse ERKEN CIKIS
```

Canlı ölçüm: beş callback de **tek** karşılaştırma yapıyor, `sol=35 sag=0`,
sonuç 1 → atla. RNG'ye **hiç ulaşılmıyor**. Bu yüzden RNG hook'lama,
callback tekrarı gibi yaklaşımlar baştan anlamsızdı.

`global.eSt` = `[35, 50, 3, 4, 12, 25, 15, 0, 14, 18, 28]` — her odada **aynı**,
`gml_Object_Controller_obj_Other_5` (Room Start) kuruyor. Azalan sayaç değil,
sabit yapılandırma tablosu.

### Denenip ÇÖKEN yaklaşımlar (tekrar deneme)
| # | Yöntem | Sonuç |
|---|---|---|
| 1 | Callback içi eSt oranını büyütme | çökme (mantık ters, kapıyı daha da kapatıyor) |
| 2 | İç RNG (`sub_c605880`) hook'u | donma |
| 3 | Callback içi eSt sayacını sıfırlama | çökme |
| 4 | Room Start'ta eSt override | çökme |
| 5 | `Spawn_*_obj` instance'ını çoğaltma | çökme — spawner'lar ZoneState'e kayıtlı **tekil**; kopya anahtarı çakıştırıyor |

Sonuç: kapı gerçek bir **ön koşul**, ayarlanabilir oran değil. Zorlamak =
brief'te yasaklanan "placement validation bypass". Kod `kEstOverrideEnabled=false`,
`kSpawnerDuplicationEnabled=false` ile kilitli.

---

## 5. Faydalı adresler / bulgular (S10-2026.08.24)

```
sub_c605880   ic irandom (double doner; builtin tablosunu ATLAR -> builtin
              random/irandom hook'lari asla atesleme)
sub_c5d5620   RValue karsilastirma (rcx=sol, rdx=sag, xmm2=eps, r9b=flag)
sub_c5b4ca0   degisken oku (src, varId, arrayIdx, dst, bool, bool)
sub_1826a0    FreeRValue     sub_1827d0  CopyRValue
gDataProtected[N] = N  -> anti-tamper dolaylama; sabitler buradan okunuyor
```

Global değişken ID'leri (varmap **yanlış** nesneleri haritalıyor — C++ statik
string nesneleri; `variable_get_hash` ile doğru ID alınır):
```
0x11BE1DF0 -> id 104359 -> gDataProtected
0x11C00C20 -> id 103819 -> eSt
0x11C27CC0 -> id 104325 -> gameLayer
```

Yerleştirme fonksiyonları (henüz kullanılmadı):
```
gml_Script_CreateInFreePos      rva 0x71a310   argc=5 (sira BILINMIYOR)
gml_Script_IsObtainablePlace    rva 0x3f16160
gml_Script_CreateLootInFreePos  rva 0x718db0
```

---

## 6. Pipe kontrol komutları

Boru adı: `\\.\pipe\ForgePactNative_<pid>` (pid `startup.log`'un 2. satırında)

```
set density <1-5>
set rift|battlefield|cursedorb|summonportal|chaospillars <n>
set relic|keys|gold <n>
set reveal <0|1>
set relicgate <0|1>
set spawn <NesneAdi> [adet]         # oyuncunun yaninda yarat
set spawnspread <NesneAdi> [adet]   # haritaya yay  (yeni)
set estset <indeks> <deger>         # eSt override (KILITLI, kullanma)
set estclear
```

---

## 7. Harita geneline yayma (uygulanan yaklaşım)

`CreateInFreePos`'un argüman sırası bilinmediği için **körlemesine çağrılmıyor**.
Onun yerine: her `Enemy_Creator_*` yaratımında konum kaydediliyor
(`gCreatorPositions`, üst sınır 4096). Bunlar oyunun kendi bölge üreticisinin
seçtiği, haritaya yayılmış, **geçerliliği kanıtlanmış** noktalar. `spawnspread`
bunların arasından eşit aralıklarla seçip içeriği oralara koyuyor.

Böylece yerleşim doğrulaması bypass edilmiyor — konumları oyun zaten onaylamış.

---

## 8. Çalışan özellikler (S10)

| Özellik | Durum |
|---|---|
| Monster Density 1–5× | ✅ doğrulandı (`extra_creators=868`) |
| Special Content (3/5 tür) | ✅ on-demand spawn, 10 yaratım, çökme yok |
| Drop Rates (relic/keys/gold) | hook kurulu, S9 mantığı aynen taşındı |
| Map Reveal | hook kurulu |
| Relic Gate | imza hazır |

---

## 9. ÇALIŞAN DURUM (canlı doğrulandı, 2026-08-25)

Ölçüm: `auto_spread=6`, `spawned=44`, `extra_creators=602`, çökme yok.

- **Otomatik dağıtım**: her oda başlangıcında `gCreatorPositions` temizlenir
  (eski haritanın koordinatları sızmasın), harita dolarken üretici konumları
  toplanır, 40 konum birikince aktif içerikler bir kez dağıtılır.
  Her yeni haritada tekrarlanır — kullanıcı komut göndermez.
- Sayılar pipe'tan ayarlanır: `set chaospillars 10`, `set cursedorb 6`, vb.
  Çarpan değeri = o haritaya kaç adet dağıtılacağı.
- Bir haritada gözlenen konum sayısı ~100-190; oyuncu dolaştıkça artar.

## 10. Sıradaki iş

1. `Rift_Portal_obj` / `Portal_Battlefield_obj` kurulum verisi — Create olayında
   hangi instance değişkenini bekliyor, onu set edip yaratmayı dene
2. ForgePact arayüzüne bağlama (şu an pipe komutu gerekiyor)
3. Ayarların kalıcılaştırılması (oyun açılışında otomatik yüklensin)

---

## 11. Rift / Battlefield — nerede takildik (2026-08-25)

### Nesne adlari DOGRU
`data.win` OBJT chunk'i ayristirildi: **6014 nesne**, index->isim tablosu
`objnames.json`. Dogrulama: 4659=Spawn_Battlefield_obj, 4662=Spawn_Chaos_Pillars_obj,
4665=Spawn_Cursed_Orb_obj, 4672=Spawn_Rift_obj, 4676=Spawn_Summon_Portal_obj.

Tum oyunda battlefield icin 3, rift icin 3 nesne var:
`Portal_Battlefield_obj`, `Spawn_Battlefield_obj`, `Fall_Battlefield_Tent_01_obj`
`Rift_Portal_obj`, `Spawn_Rift_obj`, `Architect_Rift_Collapse_obj`
=> `Portal_Battlefield_obj` / `Rift_Portal_obj` DOGRU hedefler, baska nesne yok.
Kullanicinin tarif ettigi "el" portalin acilmadan onceki hali.

### Nesne TAM KURULU yaratiliyor
`Portal_Battlefield_obj` yaratildiginda 59 degiskeni dolu:
```
portalName   = "Eternal Battlefield"    enemySpawnCount = 149
voidRadiusMax= 1472                     fragmentAmount  = 50
portalDelay  = 30                       discoveryRange  = 750
m_BattlefieldPortalEnter (method)
setVisible=0 portalActive=0 portalUsable=0 isDiscovered=0 portalState=0
```

### AMA kendini yok ediyor  <-- ASIL SORUN
Yaratildiginda `variable_count=59`, ~4 sn sonra `variable_count=0`.
Yani nesne olmus. Denenen ve ISE YARAMAYAN mudahaleler (hepsi ayni karede,
`set lastvar` ile):
`isEligible=1  portalActive=1  portalUsable=1  setVisible=1
 isDiscovered=1 portalState=1 blendPortal=1 drawOutline=1 circleImage=1 visible=1`

Karar Create olayinin ICINDE veriliyor; bayrak yazmak kurtarmiyor.

### HIPOTEZ (dogrulanmadi)
Rift ve Battlefield **bolge gecisi** ozellikleri. `gml_Script_sc_rift` (1600 B,
`StringStartsWith` ile oda adi karsilastirmasi) bir YUKLEM: "bu oda rift bolgesi mi".
Portal, gidecegi bolge yoksa kendini kapatiyor olabilir. Bu dogruysa bu iki ozellik
yalnizca **uygun zone tiplerinde** calisir ve test edilen haritada zaten mumkun degil.

### Sonraki adim onerisi
1. Farkli act/zone tiplerinde ayni testi tekrarla (ucuz, sadece spawn+dumplast)
2. `Portal_Battlefield_obj_Create_0` icinde instance_destroy kosulunu bul
3. Alternatif: mevcut `Spawn_Battlefield_obj` instance'inin `m_activateMechanic`
   metodunu dogrudan cagir (oyunun kendi aktivasyon yolu) - ama kapi yine
   eSt<=0'da takilabilir

## 12. Yeni pipe komutlari (tani icin)
```
set spawn <NesneAdi> [adet]        # oyuncunun yaninda yarat + degisken dokumu
set spawnspread <NesneAdi> [adet]  # haritaya yay
set lastvar <isim> <deger>         # son yaratilan instance'a degisken yaz
set dumplast 1                     # son yaratilan instance'i yeniden oku
                                   #   variable_count=0 -> nesne olmus
```
Log: `spawned_instance.log`

---

## 13. Rift / Battlefield — DUZELTME ve gercek durum (2026-08-25, 2. tur)

### Onceki teshis YANLISTI
"Portal kendini yok ediyor" dedim, degil. Olcum:
```
Portal_Battlefield_obj  exists=1  x=13524.7 y=2133.63   <- YASIYOR, oyuncunun yaninda
Chaos_Pillar_obj        exists=1  x=13534.4 y=2133.63
instance_destroy hook'u HIC tetiklenmedi
```
Onceki `variable_count=0` sonucu benim okuma hatamdi (dumplast'ta instance
kimligini duz sayi olarak geciriyordum). Kontrol testi: Chaos_Pillar ayni
yontemle 94 degisken donduruyor -> yontem calisiyor, portallar gercekten
farkli davraniyor ama OLMUYORLAR.

### Gercek durum: portal UYKUDA
Nesne tam kurulu ve canli, sadece aktive edilmiyor:
```
setVisible=0 portalActive=0 portalUsable=0 isDiscovered=0 portalState=0
voidRadius=0 (voidRadiusMax=1472'ye buyumesi gerekiyor)
```
Bu bayraklari elle yazmak (10 farkli kombinasyon, ayni karede) ISE YARAMADI.

### Olay yapisi — asil is Alarm_0'da
```
Portal_Battlefield_obj : Alarm_0=8528B  Alarm_1=11072B  Alarm_8=1712B
                         Create_0=19600B  Step_0=49248B  Draw_0=11776B
Rift_Portal_obj        : Alarm_0=48048B  Alarm_8=1440B
                         Create_0=13024B  Step_0=736B
```
Rift'in Step'i sadece 736 B: bir degisken oku -> karsilastir -> baska
degiskene 1 yaz. Yani durum makinesi; motor Alarm_0.
Spawner normalde portali yaratip ALARMINI kuruyor. Biz alarmi kurmuyoruz,
portal hic uyanmiyor.

### Alarm'i dogrudan cagirma denemesi - TIKANDI
`set fireevent <RoutineAdi>` komutu eklendi (rutini YYObjectEvent olarak
cagirir). Ama instance_create_depth **kind=15** donduruyor ve
`value.pointer` = 0x40000010004471f -> bu bir CInstance* DEGIL, paketlenmis
instance referansi (dusuk 32 bit = 0x4471f = 280351 = instance id).
Olay cagrisi istisna firlatti (SEH yakaladi, oyun cokmedi).

### SONRAKI ADIM (statik, kullanici testi gerektirmez)
Oyunun "instance id -> CInstance*" cozumleyicisini bulmak gerekiyor.
YYToolkit'te bu `GetInstanceObject`. Oyunun kendi `with()` uygulamasi bunu
kullanir; runner icinde bulunabilir. Bulununca:
   id -> CInstance* -> Alarm_0(self, self)  cagrilabilir.

Alternatif: `event_perform_object` benzeri bir builtin varsa onunla.

---

# COZULDU - S10 Ozel Icerik Sistemi (2026-08-25, tamamen statik analiz)

## Ozet: yanlis nesneyi yaratiyorduk

`Portal_Battlefield_obj` ve `Rift_Portal_obj` mekanigin **sonunda** beliren
odul portallaridir. Haritadaki mekanik nesnesi degildirler. Bu yuzden
yaratildiklarinda tam kurulu (59 degisken), canli (`exists=1`) ama hareketsiz
duruyorlardi: onlari surecek hicbir olay yoktu.

Haritadaki gercek mekanik nesnesi **`Spawn_Battlefield_obj`** /
**`Spawn_Rift_obj`** - hepsi `Spawn_Mechanic_Parent_obj` cocugudur.

## Statik analiz araclari (scratchpad)

- `an.py`  - degisken adlarini cozen disassembler.
  `.data` icinde 16 hizali `{int32 id; int32 pad; const char* name;}` yuvalari
  var. YYC kodu `mov edx, dword ptr [rip+X]` ile id alanini okur.
  **Ad, `X-16` adresindeki yuvadan okunur (delta=-16).**
  Dogrulama: `Portal_Battlefield_obj_Create_0` -> `portalName`, `enterPortal`,
  `portalZone`, `isDiscovered` cikti.
- `xref.py` - bayt taramasiyla rip-goreli erisim ve `e8` cagri capraz
  referansi. Tam tarama ~3 saniye (232 MB .text icin).
- `fp.py`   - PE okuyucu + capstone + rutin tablosu (20841 rutin).

## Mekanik listesi (9 adet, hepsi Spawn_Mechanic_Parent_obj cocugu)

| Marker nesnesi                 | discoverable | not                       |
|--------------------------------|--------------|---------------------------|
| `Spawn_Abyss_obj`              | 1            |                           |
| `Spawn_Battlefield_obj`        | 1            | ARANAN                    |
| `Spawn_Rift_obj`               | 1            | ARANAN                    |
| `Spawn_Cursed_Orb_obj`         | 1            |                           |
| `Spawn_Summon_Portal_obj`      | 1            |                           |
| `Spawn_Shadow_Realm_obj`       | 1            |                           |
| `Spawn_Traveling_Merchant_obj` | 1            |                           |
| `Spawn_Chaos_Pillars_obj`      | -            | randomizePosition/skipPosition kendi kurar |
| `Spawn_Chaos_Tower_obj`        | -            | discoverable yok          |

Cocuk Create_0 (352 bayt, hepsi ayni):
    event_inherited();              // sub_c6231c0
    discoverable       = 1;
    m_activateMechanic = <anon@119@...>;   // asil mekanigi kuran fonksiyon

`sub_c6231c0` = **event_inherited**. Kanit: `Spawn_Mechanic_Parent_obj`
cagirmiyor, cocuklar cagiriyor; govdesi nesne hash tablosundan ebeveyni bulup
`[rcx+0x8c]` ile olay gonderiyor. 7345 cagri yeri, 5004'u `_obj_Create_0`.

## Yasam dongusu (patch gerektirmiyor - nesne kendi kendini surer)

`gml_Object_Spawn_Mechanic_Parent_obj_Create_0` @ 0xafe3c40
    isActive       = <bos>
    skipPosition   = <bos>
    collisionRadius, discoverable=0, discoverRange
    discoverTimer  = game_get_speed() * 0.5     // ~30 kare
    activateTimer  = game_get_speed() * 0.05    // ~3 kare
    m_activateMechanic = <varsayilan anon@259>

`gml_Object_Spawn_Mechanic_Parent_obj_Step_0` @ 0xafe7ff0
    discoverable ise: discoverTimer sayar, distance_to_object(Player) <=
    discoverRange olunca PlayerUpdateMinimap (minimapta gorunur olur)
    activateTimer bitince:   **alarm[0] = 1**       (+4039, sub_c5b4800)

`gml_Object_Spawn_Mechanic_Parent_obj_Alarm_0` @ 0xafe4c90  (13152 bayt)
    1. StringStartsWith(room_get_name(room), ...)   // oda adi on eki
    2. ZoneStateExists(room)                        // bolge durumu olmali
    3. isActive guard
    4. skipPosition degilse: **999 deneme** ile rastgele nokta sec
         x in [128, room_width-128], y in [128, room_height-128]
         collision_circle(...) x2  +  IsObtainablePlace(x, y)
       -> nesne kendini gecerli bir noktaya TASIR
    5. RunningHost()                                // tek kisilik = host
    6. **m_activateMechanic()**                     // mekanik burada baslar
    7. isActive = 1  (SPV)
    8. m_activateMechanic = undefined  (SetVariableToUndefined) + SetVariable

### Neden kisitlarimizi ihlal etmiyor
- **Tekrarlayan geri-cagri yok**: `isActive=1` ve `m_activateMechanic=undefined`
  aktivasyondan sonra kuruluyor; tek atimlik.
- **Yerlesim dogrulamasi atlanmiyor**: tersine, oyunun kendi 999-denemeli
  collision_circle + IsObtainablePlace dogrulamasi calisiyor.
- **ZoneState anahtari cogaltilmiyor**: kaydi oyunun kendisi yapiyor.
- **RNG'ye global kanca yok**: hicbir sey kancalanmiyor.

## Uygulama sonucu

Tek yapilmasi gereken: `instance_create_depth` ile **marker** nesnesini
yaratmak. Konum onemsiz - Alarm_0 nesneyi zaten kendi tasiyor. Yani
`gCreatorPositions` yayilimina da gerek yok; oyuncunun uzerinde yaratmak
yeterli, oyun haritaya kendisi dagitir.

## Yan bulgu: instance hash tablosu

`event_inherited` govdesindeki nesne aramasi ile `instance_exists` icindeki
instance aramasi ayni dugum duzenini kullaniyor: anahtar `+0x10`, sonraki
`+8`, deger `+0x18`. Ama tablo/maske **tek bir struct isaretcisi** uzerinden
okunuyor (`mov rcx,[rip+X]` sonra `[rcx]` = kovalar, `[rcx+8]` = maske),
iki ayri global degil. `ResolveInstanceById`'nin nullptr donmesinin sebebi
buydu. Marker cozumu bu yolu gereksiz kildigi icin duzeltilmedi.

---

# DUZELTME - marker yolu CALISMIYOR (2026-08-25 19:40)

Yukaridaki "COZULDU" bolumundeki su iddia **YANLIS**: "iki dal da
m_activateMechanic'e cikiyor". Cikmiyor.

`Spawn_Mechanic_Parent_obj_Alarm_0` @ 0xafe4c90 icinde:
    +3730  ZoneStateExists(room, ...)
    +3787  test bl,bl ; je 0xafe724a   ->  +9658'e sicrar
    +9609  m_activateMechanic okunur
    +9644  cagrilir (sub_c5b17e0)
    +9649  jmp 0xafe7258
    +9658  <-- SICRAMA HEDEFI: mekanik cagrisindan SONRA, isActive okumasindan once

Yani ZoneStateExists dali `m_activateMechanic()` cagrisini **atliyor**.
Uretilmis bir bolgeye sonradan isaretci enjekte edince hep bu dala giriliyor.

Canli olcum (pid 62256, marker derlemesi):
    cursedorb Create cagrisi = 29,  Alarm_0 cagrisi = 63,  gorunur nesne = 0

Dalin yonu `sub_c5d5620`'nin semantigine bagli; o fonksiyon bir RValue
karsilastirma jump-table'i (0xc5d5620, r8d = kind_a*16 + kind_b ile dagitim,
r9d = operator kodu). Yon kesinlestirilmeden yeni deneme yapilmamali.

## Karar
Otomatik dagitim **dogrudan nesne yaratmaya geri alindi**
(`Chaos_Pillar_obj` / `Cursed_Orb_obj` / `Summoning_Portal_obj`).
Kaynak, marker denemesinden onceki yedekle davranis olarak ayni
(`_backups/marker_fix_20260825/`), derleme boyutu yine 463872 bayt.

Marker yaratma hala `set spawn <ObjectName>` boru komutuyla elle
denenebilir; otomatik yolda degil.

---

# DENSITY REGRESYONU VE ONARIMI (2026-08-25 19:45)

## Belirti
Kullanici "density calismiyor" dedi.  Olcum onu dogruladi (pid 46628):
    density=4
    density_route=stat588
    density_stat_calls=140804   density_stat_matches=5   density_stat_overrides=5
    extra_creators=0            extra_enemies=0

## Kok neden
Iki ayri sey vardi:

1. `extra_creators` / `extra_enemies` sayaclari kodda **hicbir yerde
   artirilmiyordu** - olu sayaclar. "0" olmalari bir kanit degildi.

2. Asil density mekanizmasi olan **S9 DoMultiCreate blogu**
   (`CallCreateWithDensity` icinde spawner'i N kez yaratma) dosya bozulup
   `_backups/special_gate_pre_callsite_...` yedeginden geri alinirken
   kaybolmus; `reapply.py` o blogu geri koymuyordu.  Sadece
   `HookReturnSpecificStat` (stat 588) yolu kalmis.

   Stat 588 yolu etkisiz: `ReturnSpecificStat` bir oturumda 140804 kez
   cagriliyor ama **yalnizca 5'inde** 588 argumani geciyor.  Ustelik
   `SetReal(output, max(natural, multiplier))` carpma degil, en fazla 5'e
   sabitleme yapiyor.

## Onarim
S9 blogu `_backups/BOZUK_20260825_1805.cpp.bak` icinden aynen kurtarildi ve
`CallCreateWithDensity`'ye geri konuldu.  Derleme 464384 bayt.
Basari gostergesi artik `extra_creators` (yeniden canli sayac); calisirken
haritada birkac yuz olmasi beklenir (eski olcum: 602).

## Ders
`reapply.py` tam bir geri-uygulama degildi.  Bir daha kaynak geri alinirsa
oncesinde `grep -c "gExtraCreators.fetch_add"` gibi davranis kontrolleri
yapilmali; boyut/derleme basarisi regresyonu yakalamiyor.

---

# ASIL CEVAP: gml_Script_sCP  (2026-08-25 20:10, statik)

## Ayirt edici gozlem
Mekanik govdelerinin degisken listesi karsilastirildiginda:

  KENDI YARATAN (instance_create_layer govdede):
    Abyss, Chaos_Pillars, Chaos_Tower, Cursed_Orb, Summon_Portal,
    Traveling_Merchant
  sCP'YE DEVREDEN (govdede instance_create_layer YOK):
    Battlefield, Rift, Shadow_Realm

Calismayanlar tam olarak sCP'ye devredenler.

## sCP imzasi
`gml_Script_sCP` @ rva 0x3d81c0 (4256 bayt), 3 argumanli:

    sCP(nesneAssetRef, x, y)

Govdesi: `instance_create_layer(x, y, gameLayer, nesne)` + `pSpwd`
kaydi (SPV / SetVariable).  **Icinde erken donus kapisi yok** - cagrilirsa
yaratir.

Arguman 0 paketlenmis RValue, kind 15, ust dword `0x01000000` (asset
etiketi), alt dword nesne indeksi.  Kodda `movabs rax, 0x1000000XXXXXXXX`
olarak gorunur.

## Dogrulanan nesne indeksleri
    Battlefield    -> 3581  Portal_Battlefield_obj
    Rift           -> 4126  Rift_Portal_obj
    Shadow Realm   -> 3598  Portal_Shadow_Realm_obj
    (957 Collision_Parent_obj ve 4669 Spawn_Mechanic_Parent_obj
     yalnizca collision_circle / place_meeting maskeleri)

## Neden onceki denemeler bosa gitti
Biz de ayni `Portal_Battlefield_obj`'i yaratmistik, ama:
  1. `instance_create_depth` ile, `gameLayer` yerine derinlik katmanina,
  2. sCP'nin yaptigi `pSpwd` kaydi olmadan.
Nesne tam kurulu ve canli oluyordu ama hicbir sey onu surmuyordu.

## Yapilacak
1. `gml_Script_sCP` profile zorunlu rutin olarak eklenecek (rva 0x3d81c0).
2. Runtime'da asset-ref RValue kurulup oyun ipliginde `sCP(obj, x, y)`
   cagrilacak.
3. Konum: zaten topladigimiz oyun-dogrulamali Enemy_Creator konumlari.
4. rift / battlefield kaydiricilari buna baglanacak.

Kisitlara uyumu: yeni kanca yok, RNG'ye dokunulmuyor, ZoneState anahtari
cogaltilmiyor, yerlesim dogrulamasi atlanmiyor (oyunun kendi kaydettigi
gecerli noktalar kullaniliyor), tek atimlik.

## Chaos Tower AYRI
sCP kullanmiyor.  `chaosTowerStarted`, `chaosTowerSpawnZone`, `buffZone`,
`buffArray`, `buffValueArray`, `zonesVisited`, `RunningHost`,
`NetworkSendChaosTowerZone` ve kat yonetimi icin `Spawn_Next_obj`
(56880 bayt) gerekiyor.  Bu bir bolge modu; portal yaratmakla olmaz.
Ayri is olarak ele alinmali.

---

# SEZON 9'UN GERCEK MEKANIGI: multobj  (2026-08-25 20:35)

## S9 kaynagindan (ForgePact_S9_CALISAN_YEDEK)
`src/forgepact.py`:
    SPAWNERS = [("rift", 3516, ...), ("battlefield", 4990, ...), ...]
    out.append(f"multobj {idx} {adet}")

`plugin/ModuleMain.cpp` -> `DoMultiCreate`:
    instance_create_depth ve instance_create_layer builtin'leri kancalanir.
    Yaratilan nesnenin indeksi g_ObjMult tablosundaysa, ayni cagri konum
    kaydirmasiyla (i%5-2)*28 / (i/5-2)*28  N kez daha yapilir.

**S9 hicbir zaman portal yaratmadi.**  Oyunun kendi yarattigini cogaltti.
Bu yuzden hic cokmedi: oyuna yeni bir cagri girmiyor.

## S10 karsiligi
`sCP` zaten `instance_create_layer(x, y, gameLayer, Portal_...)` cagiriyor,
yani oyun bir battlefield/rift actiginda **bizim kancamizdan geciyor**.
Tek eksik, katalogda portal nesnelerinin ozellige baglanmamis olmasiydi.

`SpawnFeatureForName` genisletildi:
    Rift_Portal_obj        -> "rift"
    Portal_Battlefield_obj -> "battlefield"
    Portal_Shadow_Realm_obj-> "shadowrealm"

Cogaltma zaten var olan `CallCreateWithDensity` blogunda yapiliyor;
`densityEligible` yalnizca Enemy_Creator'lar icin true oldugundan portallar
yogunluk carpanindan etkilenmez, sadece kendi kaydiricilarindan.

Dogrulama gostergesi: `spawner_catalog` 5 -> **8** olmali.

## sCP'yi DOGRUDAN cagirmak COKERTIYOR
2026-08-25 20:19: `content_placer=READY` (rutin cozuldu) ama
`content_placed=0` ve oyun coktu.  Cagri bicimi ya da baglam yanlis.
`gContentPlacerEnabled` varsayilan **false**; `set contentplacer 1` ile
bilerek acilabilir.  Bu yol dogrulanmadan otomatik dagitimda kullanilmaz.

## Sinir
Bu mekanizma yalnizca oyun kendisi bir tane actiginda devreye girer.
S9'da da boyleydi.  Dogal oran dusukse haritada hicbir sey gorunmeyebilir;
o zaman sirada oranin nereden geldigini bulmak var (eSt / mekanik ici kapi),
portal yaratmak degil.

---

# PORTAL COGALTMASI = SONSUZ LOADING  (2026-08-25 21:55, geri alindi)

`SpawnFeatureForName` icine portal nesneleri eklenmisti:
    Rift_Portal_obj -> "rift",  Portal_Battlefield_obj -> "battlefield"
Boylece sCP'nin instance_create_layer cagrisi 10 kez cogaltiliyordu.
Sonuc: bolge yuklenmesi sonsuz donguye girdi.

## Dogrulanan sebep (statik)
    Rift_Portal_obj_Create_0        -> instance_create_layer
    Portal_Battlefield_obj_Step_0   -> instance_create_layer   (HER ADIMDA)
    Portal_Battlefield_obj_Alarm_8  -> instance_create_layer
    Rift_Portal_obj_Alarm_0         -> instance_create_layer

Portal, kendi olaylarinda nesne yaratiyor.  Portali cogaltmak, o
yaratimlarin da cogalmasi demek -> ussel buyume / adim dongusunun bogulmasi.

**DERS: bir nesneyi cogaltmadan once, o nesnenin olaylarinin kendisi
instance_create* cagirip cagirmadigi kontrol edilmeli.**
Enemy_Creator'lar guvenli cunku tek atimlik yaratici nesneler.

## Su anki durum
- Portal cogaltmasi geri alindi, derlendi, kuruldu.
- Density calisiyor: canli olcum `extra_creators=975`.
- Rift/Battlefield yine yok.

## Kalan yol
Portali degil, **isaretciyi** (Spawn_Rift_obj / Spawn_Battlefield_obj)
cogaltmak gerekiyor - S9'da da cogaltilan buydu.  Ama S10'da isaretciler
instance_create_layer'dan gecmiyor; once onlari kimin yarattigini bulmak
lazim.  Ayrica mekanik govdesindeki eSt kapisi hala cozulmedi:
canli olcumde `special_calls=rift:10,battlefield:10` yani mekanik 10'ar kez
CAGRILDI ama portal uretmedi -> karar mekanigin icinde veriliyor.

---

# KAPI BULUNDU: global.eSt[0] <= 0   (2026-08-25 22:20)

## Once bir uyari
Steam kopyasi **25.08.2026 19:58'de guncellendi**
(303426048 B, sha 0B7B9231) ve artik profille UYUSMUYOR.
Test kopyasi degismedi (303302144 B, sha 0766AA8B).
`fp.py` Steam kopyasini okuyordu; 19:58 sonrasi yapilan statik okumalarin
hepsi gecersizdi (Rift'in yerinde Chaos Tower kodu goruluyordu).
`fp.py` artik test kopyasini okuyor ve boyut kontrolu yapiyor.

## Mekanik adresleri (KODDAN cikarildi, tablodan degil)
Spawn_<X>_obj_Create_0 icindeki `lea rdx, [rip+X]` + `sub_c5b2670`
(MakeMethod) ciftinden.  Rutin tablosu script isimleri icin guvenilmez.

    Abyss              0xafce6d0     Rift            0xb00f660
    Battlefield        0xafd0750     ShadowRealm     0xb017460
    ChaosPillars       0xafd3c70     SummonPortal    0xb018cc0
    ChaosTower         0xafd80e0     TravelingMerch  0xb01ccb0
    CursedOrb          0xafdd320

## Kapi
Rift govdesi (0xb00f660):
    +325  xorps xmm7, xmm7          ; karsilastirma operandi = 0.0
    +399  eSt[0] okunur
    +540  call sub_c5d5620          ; compare(eSt[0], 0.0)
    +557  setle dil                 ; dil = (eSt[0] <= 0)
    +588  test dil,dil / je +7669   ; degilse sCP'yi ATLA

Yani:  **if (global.eSt[0] <= 0) { ... sCP ... }**

Ayni `setle` kalibi Abyss, Battlefield, CursedOrb, SummonPortal,
TravelingMerchant govdelerinde de var.  eSt[0] hepsi icin ORTAK kapi.

## Olculen deger
`est_roomstart.log`, onlarca oda basinda degismeden:
    [0=35, 1=50, 2=3, 3=4, 4=12, 5=25, 6=15, 7=0, 8=14, 9=18, 10=28]

**eSt[0] = 35 > 0  ->  hicbir ozel icerik mekanigi uretim yapmiyor.**

Bu, kullanicinin gordugu Cursed Orb / Chaos Pillar'larin tamami bizim
dogrudan yarattiklarimiz oldugunu da aciklar; oyun kendisi hicbirini
uretmiyor.

## Sonuc
Rift ve Battlefield'in gorunmemesinin sebebi ne kancalarimiz, ne konum,
ne sCP.  Tek sebep bu deger.  Geriye tek kaldirac kaliyor:
mekanik calisirken eSt[0] (ve icerigin kendi slotu) gecici olarak 0
yapilip hemen geri konmasi.  Runtime'da bu icin `estset`/`estclear`
komutlari ve `ApplyEstOverridesUnsafe` zaten yazili ama
`kEstOverrideEnabled = false` ile kapali - daha onceki bir denemede
cokme yasanmisti.  Denenecekse kapsam mekanik kancasinin icine
daraltilmali (once/sonra), Room Start'ta global yazma yapilmamali.

---

# eSt YAZMA DENEMESI DE COKERTIYOR  (2026-08-25 22:30, kapatildi)

Kapsam mekanik cagrisina daraltilmis, SEH korumali, cagri biter bitmez eski
degeri geri yazan bir surumdu (`EstGateScope`).  Yine de haritaya girerken
oyun cikti.  Telemetri tazelenmedigi icin `est_opens` / `est_fail`
okunamadi - hangi asamada oldugu belirsiz.

`gEstGateEnabled` varsayilan **false**; panel kutusu da kapali geliyor.
Bilerek denemek icin `set estgate 1`.

## Simdiye kadar eSt/portal icin denenen ve COKEN yollar
1. Portali `instance_create_depth` ile elle yaratmak -> portal olu kalir
2. `sCP`'yi dogrudan cagirmak                        -> cokme
3. Portal nesnesini cogaltmak                        -> sonsuz loading
   (sebep dogrulandi: portalin kendi Step/Create'i instance_create_layer
    cagiriyor, ussel buyume)
4. Room Start'ta global eSt override                 -> cokme (onceki oturum)
5. Mekanik cagrisi kapsaminda eSt override           -> cokme (bu)

## Degismeyen gercek
`if (global.eSt[0] <= 0)` kapisi ve olculen `eSt[0] = 35`.
Teshis dogru; sorun teshiste degil, o degeri guvenle degistirmede.

## Denenmemis kalan yol
eSt'i **yazmak** yerine, oyunun o diziyi nasil doldurdugunu bulup girdiyi
kaynaginda degistirmek.  eSt bloodPactObject uzerinde duruyor ve oda
baslangicinda hep ayni degerlerle geliyor -> bir yerden yukleniyor
(kayit dosyasi / sezon yapilandirmasi / sunucu yaniti).  O kaynak
bulunursa calisma zamaninda hicbir seye yazmadan kapi acilabilir.

---

# eSt KAYNAGI BULUNDU + STAT KAPISI DENEMESI  (2026-08-25 22:50)

## eSt'i oyun kendisi dolduruyor
`gml_Object_Controller_obj_Other_5` (Room Start) icinde 11 atama:

    global.eSt[i] = ReturnSpecificStat(..., <istatistik>, ...)     (argc = 5)

| eSt | stat | icerik            | olculen |
|-----|------|-------------------|---------|
|  0  | 756  | ORTAK KAPI        |   35    |
|  1  | 675  | Chaos Pillars     |   50    |
|  2  | 682  | Chaos Pillars     |    3    |
|  3  | 676  | Chaos Pillars     |    4    |
|  4  | 705  | Battlefield       |   12    |
|  5  | 721  | Battlefield       |   25    |
|  6  | 666  | Chaos Tower       |   15    |
|  7  | 813  | Cursed Orb        |    0    |  <- tek acik olan
|  8  | 770  | Rift              |   14    |
|  9  | 723  | Shadow Realm      |   18    |
| 10  | 823  | Summon Portal     |   28    |

Cikarim betikleri: `est_ids.py`, `est_map.py` (scratchpad).

## Deneme: ReturnSpecificStat donusunu 0 yapmak -> COKTU
Ilk surum her cagrida butun argumanlara bakip 600-900 arasi sayi ariyordu.
`ReturnSpecificStat` bir oturumda ~140.000 kez cagriliyor ve 756/705 gibi
degerler koordinat/hasar/id olarak her yerde geciyor -> alakasiz yuzlerce
cagrinin sonucu sifirlandi, oyun coktu.

Ikinci surumde mudahale `gInRoomStart` bayragi + `argc == 5` ile Room Start
icine hapsedildi.  **Yine haritaya girerken cokme oldu.**
`gStatGateEnabled` varsayilan false; komut `set statgate 1`.

## Simdiye kadar denenen ve COKEN yollarin tam listesi
1. Portali instance_create_depth ile elle yaratmak -> portal olu kalir
2. sCP'yi dogrudan cagirmak                        -> cokme
3. Portal nesnesini cogaltmak                      -> sonsuz loading
4. Room Start'ta global eSt override               -> cokme
5. Mekanik kapsaminda eSt override (tur kontrolsuz)-> cokme
6. Mekanik kapsaminda eSt override (tur kontrollu) -> cokme
7. ReturnSpecificStat donusunu sifirlamak          -> cokme

## GetBloodPactInfo yolu -> OLUMSUZ (ama temiz)
S9 eklentisi ozel icerik oranlarini `GetBloodPactInfo` uzerinden veriyordu
(`blood_pact_rift_rate` vb.).  S10'da kanca kuruldu ve calisiyor ama
`rate_info_hits = 0`: oyun bolge uretiminde bu anahtarlari hic sormuyor.
Mekanik govdeleri dogrudan `eSt` okuyor.  Bu yol S10'da olu.

# AURIE DURUMU (2026-08-25 23:00)

Ayri oyun kopyasi: `C:\Users\falor\Downloads\HeroSiege_Aurie`

- AuriePatcher ile exe yamalandi -> **oyun 9 saniyede kapaniyor**
- Yamasiz exe geri konuldu       -> **oyun normal calisiyor**
=> **AuriePatcher S10 exe'sinde CALISMIYOR.**  Mod dosyalari dogru
   (md5'leri `ForgePact-error-fix (1)\modfiles` ile birebir ayni).

Buna ragmen YYToolkit S10'da SAGLAM:
    m_IsUsingMidFunctionHook = true, runner interface created
    YYC::GmpFindFunctionsArray() => AURIE_SUCCESS, 0x00007FF64E244B80

Yani sorun YYTK'da degil, yalnizca exe-yamalama yontemindeydi.

## Alternatif: yamasiz enjeksiyon
`C:\Users\falor\Downloads\bp_aurie\inject.py` oyunu askiya alinmis baslatip
`AurieCore.dll`'i LoadLibraryW ile enjekte ediyor; **exe diskte hic
degismiyor**.  AurieCore sonra mods\aurie'den YYToolkit + eklentiyi yukluyor.
Bozuk olan yamalama adimi tamamen atlaniyor.

## YYToolkit kaynaktan yeniden derlendi -> YINE COKUYOR
`Downloads\yytk_src` icindeki kaynak zaten ForgePact'in degistirdigi surum
(`Generic-RunnerInterfaceNew.cpp` birebir ayni).  MSVC 14.51 ile derlendi:
903680 bayt (dagitilan surum 887808).

Test (HeroSiege_Aurie kopyasi, exe YAMASIZ, inject.py ile AurieCore):
    sadece AurieCore                 -> oyun AYAKTA kaldi (1.9 GB)
    AurieCore + YYToolkit (dagitilan)-> 21 sn sonra kapandi
    AurieCore + YYToolkit (yeniden)  ->  3 sn sonra kapandi, log bile yok

## Sonuc
- Aurie framework S10'da CALISIYOR (yamasiz enjeksiyonla).
- AuriePatcher (exe yamalama) S10'da CALISMIYOR.
- **YYToolkit S10'da oyunu oldururuyor** - hem dagitilan hem yeniden
  derlenen surum.  Dagitilan surum daha ileri gidiyor: kosucu arayuzunu
  basariyla kuruyor (`runner interface created`,
  `GmpFindFunctionsArray => AURIE_SUCCESS`) ve ondan SONRA oluyor.
  Yani S10'u okuyabiliyor, kancalarini kurarken cokuyor.

YYTK'yi S10'a uyarlamak ayri bir proje: GameMaker'in yeni surumu icin
runner/hook katmaninin duzeltilmesi gerekiyor.

## Onemli cerceve
Native runtime S10'da YYTK'nin verecegi seyi zaten yapiyor:
22 builtin + 42 rutin cozuluyor, kancalar kuruluyor, GML fonksiyonlari
cagriliyor, instance degiskenleri okunup yaziliyor.  Aurie'ye gecmek yeni
bir yetenek getirmiyor; yalnizca test edilmis bir API getirirdi - ama once
YYTK'nin S10 portu yapilmali.

Cozulmemis asil problem araçtan bagimsiz: `global.eSt[0] = 35` ve kapi
`eSt[0] <= 0`.  Bu degeri acmanin denenen 7 yolu da cokertti.

---

# YYTOOLKIT v5 (experimental dali)  (2026-08-25 23:15)

## Neden gerekliydi
Elimizdeki YYTK **v4, stable dali, Mart 2025** (`86c133d`) - bu upstream
stable'in EN YENI commit'i, yani stable dali S10 icin cok eski.
`experimental` dali ise **v5** ve Subat 2026'ya kadar guncel:

    2026-02-02  YYC altinda RValue dizi offset analizi duzeltmesi
    2025-12-15  RUNNER_INIT geri cagrisi
    2025-12-14  2024.14 duzeltmeleri
    2025-08-31  2024.14 - fonksiyon dizisi arama + YYObjectBase
    2025-08-01  v5 - buyuk yeniden yapilandirma

Kaynak: https://github.com/AurieFramework/YYToolkit (experimental)
Indirilen: `C:\Users\falor\Downloads\yytk_v5\YYToolkit-experimental`
Derlenen : `YYToolkit\YYToolkit_v5.dll` (879104 bayt, MSVC 14.51)

Derleme komutu vcxproj'den cikarildi:
    cl /std:c++latest /EHsc /MD /LD /O2
       /DYYTK_INCLUDE_PRIVATE /DYYTK_DEFINE_INTERNAL /DWIN32 /DNDEBUG /D_CONSOLE
       /I include  <vcxproj'daki 14 dosya>
       /link /DLL user32.lib gdi32.lib d3d11.lib dxgi.lib

## v4 vs v5 farki (olculdu)
    v4 (dagitilan / yeniden derlenen) -> oyunu OLDURUYOR (3-21 sn)
    v5                                -> oyun AYAKTA, YYTK duzgunce
                                         basarisiz olup kendini kaldiriyor

v5'in hatasi COKME degil, zamanlama:
    YkSetupLateInitialization() starts waiting...
    [critical] Failed to await runner interface creation!
    Module "YYToolkit.dll" failed ModuleInitialize ... will be purged

## Cozum: erken enjeksiyon
`inject.py` oyunu normal baslatip **8 saniye sonra** enjekte ediyor
(v4 icin ayarlanmis).  v5 kosucu arayuzunun olusturulmasini BEKLIYOR,
8 saniye sonra o an gecmis oluyor.

Yeni betik: `C:\Users\falor\Downloads\bp_aurie\inject_early.py`
    CREATE_SUSPENDED ile baslat -> AurieCore.dll enjekte et -> ResumeThread
Boylece AurieCore giris noktasindan ONCE yukleniyor, exe diskte hic
degismiyor (AuriePatcher S10'da bozuk oldugu icin bu sart).

## v5 neden hala yuklenemiyor: ZAMANLAMA DEGIL, YUKLEME ANI
`MI_Aurie.cpp: YkSetupLateInitialization()`
    WaitForSingleObject(m_RunnerInterfacePopulatedEvent, 10000)

Olayi `Zeus-x64.cpp` icindeki **fonksiyon-ici kanca** (MmCreateMidfunctionHook)
isaretliyor: oyunun kosucu arayuzunu kurdugu kod calisinca YYTK arayuzu
kopyalayip `SetEvent` yapiyor.

Kosucu arayuzu oyunun ILK anlarinda kuruluyor.  Enjeksiyon ne zaman
yapilirsa yapilsin o kod coktan gecmis oluyor:

    gecikme 0.3 sn -> basarisiz
    gecikme 1.0 sn -> basarisiz
    gecikme 2.5 sn -> basarisiz
    gecikme 8.0 sn -> basarisiz (inject.py varsayilani)
    askiya alinmis (0 sn, giris noktasindan once) -> basarisiz
        (ana iplik durdugu icin oyun hic baslamiyor, kanca da hic tetiklenmiyor)

## Kalan tek engel
AurieCore'un oyunun **giris noktasindan once** yuklenmesi ve ondan sonra
oyunun normal calismaya baslamasi gerekiyor.  AuriePatcher tam bunu yapiyor
ama S10 exe'sinde bozuk (yamali exe 9 saniyede kapaniyor).

Denenebilecek yollar:
1. **Proxy DLL** - oyunun zaten import ettigi bir DLL (version.dll,
   dinput8.dll vb.) adiyla bir kopru DLL koyup AurieCore'u oradan yuklemek.
   Exe'ye hic dokunmaz, giris noktasindan once calisir.  En temiz yol.
2. AuriePatcher'in S10'da neden bozuldugunu bulmak (303 MB exe'ye .aurie
   bolumu ekliyor; S10'da butunluk kontrolu olabilir).

## Bu asamada elde edilenler
- YYTK v5 derlendi ve S10'u OLDURMUYOR (v4 olduruyordu) - buyuk fark
- Basarisizlik temiz: modul kendini kaldiriyor, oyun calismaya devam ediyor
- Engel tam olarak yerini belli etti: yukleme ani, kod uyumlulugu degil

---

# PROXY DLL COZUMU (2026-08-25 23:20) - CALISIYOR

AuriePatcher S10'da bozuk oldugu icin AurieCore'u exe'ye dokunmadan erken
yuklemek gerekiyordu.  Cozum: **version.dll proxy**.

Oyun `VERSION.dll` import ediyor (22 import arasinda).  Oyun klasorune
"version.dll" adiyla konan bir kopru DLL, yukleyici tarafindan **giris
noktasindan once**, import cozumlemesi sirasinda yuklenir.

    C:\Users\falor\Downloads\aurie_proxy\proxy.cpp   (+ derlenmis version.dll)
    kurulum: version.dll -> oyun bin klasoru
             C:\Windows\System32\version.dll -> bin\version_orig.dll

17 export `#pragma comment(linker, "/export:Ad=version_orig.Ad")` ile
gercek DLL'e yonlendiriliyor.  (.def dosyasindaki `ad=modul.ad` bicimi
MSVC'de LNK2001 veriyor; pragma dogru yol.)

Dogrulandi:
    23:21:02  AurieCore yuklendi        <- pencere daha yok
    23:21:08  oyun penceresi olustu
Yani proxy giris noktasindan once calisiyor, exe hic degismiyor.

## Ama YYTK yine yuklenemiyor - sebep AurieCore'un kendi beklemesi
    [ElWaitForCurrentProcessWindow] Waiting for process window...  (6 saniye)
    ...sonra modulleri yukluyor -> YYTK icin cok gec

AurieCore modulleri yuklemeden once oyun penceresini bekliyor.  O sirada
kosucu arayuzu coktan kurulmus oluyor.

## Surum durumu (arastirildi)
- AurieCore: elimizdeki **en gunceli** (md5, son surumle birebir ayni)
- YYTK v5 (experimental): **bitmemis taslak PR** - issue #77 hala ACIK.
  Kosucu arayuzunu hic yakalayamiyor.
- YYTK v4 (stable): S10'da kosucu arayuzunu YAKALIYOR ve fonksiyon
  dizisini buluyor, ondan SONRA olduruyor:
      m_IsUsingMidFunctionHook = true, runner interface created
      YYC::GmpFindFunctionsArray() => AURIE_SUCCESS, 0x00007FF64E244B80

Yani gercek hedef v5 degil, v4'un o noktadan sonraki cokmesi.

---

# YYTOOLKIT COKME NOKTASI (2026-08-25 23:40)

Izli surum derlendi (`yytk_src`'ye YkCrashTrace/YkEarlyTrace eklendi,
her cagrida dosyayi acip yazip kapatarak ANINDA diske yazar).

Sonuc: **iz dosyasi HIC olusmadi.**  Yani ModulePreinitialize ve
ModuleInitialize hic cagrilmiyor.  aurie.log tam surada kesiliyor:

    - ModulePreinitialize offset: CB10
    - ModuleUnload offset: CE00
    Module 'YYToolkit.dll' compiler configuration could not be verified.
    <log biter, surec olur>

Aurie modulu haritaliyor, export'lari buluyor, sonra `__AurieFrameworkInit`
ile cerceve isaretcilerini veriyor -> **surec tam orada oluyor**.
Cokme YYTK'nin statik kuruculari / cerceve init'inde, GML tarafinda degil.
Upstream issue #76 ile ortusuyor (statik RValue kuruculari modul
yuklenirken kosucu arayuzune dokunuyor).

## Ek gozlem
Kendi derledigim v4, dagitilan v4'ten DAHA ERKEN oluyor (3 sn / hic log)
- yani MSVC 14.51 + /std:c++latest ile uretilen ikili, dagitilanla ayni
davranmiyor.  Bu yuzden "kendi v4 derlememi" ayiklamak dagitilan surum
hakkinda bilgi vermiyor.

## Nihai durum
    AuriePatcher (exe yamasi)        -> S10'da bozuk
    version.dll proxy                -> CALISIYOR (erken yukleme cozuldu)
    AurieCore                        -> en guncel surum, sorunsuz
    YYTK v4 (stable, Mart 2025)      -> S10'da olduruyor
    YYTK v5 (experimental)           -> bitmemis taslak PR (issue #77 ACIK)

**S10 icin calisan bir YYToolkit yok.**  Sorun bizim kurulumumuzda degil;
aracin kararli dali bu GameMaker surumunden eski, yeniden yazimi ise
upstream'de bitmemis.

## Onemli cerceve (tekrar)
YYTK calissaydi bile Rift/Battlefield'i acmazdi.  Kapi oyunun kendi
kodunda: `if (global.eSt[0] <= 0)`.  YYTK yalnizca ayni fonksiyonlari
cagirmanin test edilmis bir yolu olurdu; native runtime bunu S10'da zaten
basariyla yapiyor (22 builtin, 42 rutin, kancalar, GML cagrilari).

---

# YYTOOLKIT TABANLI TEMIZ BASLANGIC (2026-08-26 08:55)

Kullanicinin verdigi calisan kopya:
    C:\Users\falor\Downloads\testhero siege\Hero-Siege-ChatGPt\HeroSiege

Bizim basaramadigimizi orada basarmislar - **AuriePatcher ile yamali exe**
(303584768) + **YYTK 4.0.1** (1116672 baytlik kaynak derlemesi, stacktrace
destekli).  YYToolkit.log tam init zinciri gosteriyor:
    GmpFindFunctionsArray => AURIE_SUCCESS
    m_FunctionEntrySize = 24
    CallBuiltinEx("code_is_compiled") => AURIE_SUCCESS
    GmpFindRVArrayOffset() => AURIE_SUCCESS, 0x8
    Stage 2 init OK!

Not: RI cache satiri onemli -
    GmpCreateHookOnInterfaceCreation() => RI restored from cache
Yani ForgePact'in disk-cache degisikligi sayesinde kosucu arayuzu
yakalanabiliyor.

## Density neden calismiyordu (tespit edildi)
out.txt:
    PROOF: density=x4 native creators seen=489 boosted=0 skipped=489
           | creator clones=0 event replays=0

Eklenti 489 yaraticiyi GORUYOR ama hepsini ATLIYOR.  ChatGPT'nin
degistirdigi surumde bir eleme kosulu var.  S9'un orijinal
`DoMultiCreate` kodunda boyle bir kosul yok:
    if (g_CreatorMult > 1 && IsCreatorObject(objIdx) && g_CreatorMult > mult)
        mult = g_CreatorMult;
    if (mult > 1) { for (i=1..mult) { konum kaydir; orig(...) } }

`IsCreatorObject` -> object_get_name(idx) "Enemy_Creator" onekiyle
basliyor mu.  Isim tabanli, S10'da gecerli (native runtime da ayni
kontrolu kullaniyor, creator_catalog=7).

## Yapilan
- ChatGPT eklentisi yedeklendi: mods\aurie\BloodPactPlugin.dll.chatgpt_yedek
- S9 deposundaki DOKUNULMAMIS kaynak derlendi (433664 bayt) ve kuruldu:
      ForgePact-repo\plugin\ModuleMain.cpp  ->  BloodPactPlugin.dll
- Eski aurie.log / YYToolkit.log / bp_ipc silindi (temiz baslangic)
- Kontrol araci: Hero-Siege-ChatGPt\ForgePact_YYTK\
      1-Oyunu Baslat.bat / 2-Ayarlari Uygula.bat / hs.py
  Protokol: bp_ipc\cmd.txt'ye satir satir komut, cevap out.txt'ye eklenir.

## YYTK v4'un S10'da urettigi olumcul hata (kayda gecti)
    Unable to find any instance for object index '257087' name '<undefined>'
    ...gml_Script_timer_system_update+0x8E0
    ...Code_Execute <- YYToolkit!HkExecuteIt
257087/277023 nesne indeksi degil, INSTANCE ID (S10 id araligi ~250k-300k).
S10'da instance referansi kind 15 ile paketleniyor: (tag << 32) | id.
Ama log "Runner caught the exception" diyor - oyun yakalayip devam ediyor,
yani bu satirlar oyunu kapatan sey DEGIL.

---

# ############ CALISAN TARIF - YYTOOLKIT (2026-08-26) ############

Chaos Pillars **oyunun kendisi tarafindan** uretildi.  Dogrulama:
    861 : Chaos_Pillar_obj         : count=18
    862 : Chaos_Pillar_Tooltip_obj : count=18
Tooltip da geldigi icin nesne tam kurulu (rarity/affix oyunun sectigi).

## Kurulum
Oyun kopyasi : C:\Users\falor\Downloads\testhero siege\Hero-Siege-ChatGPt\HeroSiege
    Hero_Siege.exe        303584768   AuriePatcher ile yamali
    AurieCore.dll            967680
    mods\aurie\YYToolkit.dll 1116672  YYTK 4.0.1 (kaynaktan, stacktrace'li)
    mods\aurie\BloodPactPlugin.dll    S9 kaynagi + eklediklerimiz

Kontrol : ...\Hero-Siege-ChatGPt\ForgePact_YYTK\
    1-Oyunu Baslat.bat / 2-Ayarlari Uygula.bat / hs.py
Protokol: bp_ipc\cmd.txt'ye satir satir komut, cevap out.txt'ye eklenir.

## Eklentiye eklenen komutlar (ForgePact-repo\plugin\ModuleMain.cpp)
    gaget <ad> <idx>            global dizinin bir elemanini okur
    gaset <ad> <idx> <deger>    yazar (tur + sinir kontrollu)
    estforce <idx> <deger>      HER KAREDE zorlar
    estfree <idx> | all         birakir
    eststat                     zorlamalari + guncel eSt'i yazar
`EstForceApply()` FrameCallback'in basina baglandi.

## Uc bulgu, uc engel
1) Oyun marker'larin HEPSINI kendisi yaratiyor (harita basina 1 tane):
       4658 Spawn_Abyss   4659 Spawn_Battlefield   4662 Spawn_Chaos_Pillars
       4663 Spawn_Chaos_Tower  4665 Spawn_Cursed_Orb  4666 Spawn_Dungeon
       4672 Spawn_Rift    4674 Spawn_Shadow_Realm  4676 Spawn_Summon_Portal
       4678 Spawn_Traveling_Merchant
   (Native runtime "bu nesneler instance_create'den gecmiyor" demisti - YANLIS.)

2) `multobj <idx> <n>` marker'i cogaltiyor (S9 DoMultiCreate).
   Kanit: `extra enemies created` sayaci artiyor.  DIKKAT: createlog
   sayaci kopyalari SAYMAZ (kopyalar orig'i dogrudan cagirir), o yuzden
   count=1 gormek cogaltmanin calismadigi anlamina gelmez.

3) Kapi: `global.eSt[0] <= 0` (ORTAK) + icerigin kendi slotu.
   Kaynak dogrulamasi (CursedOrb mekanigi 0xafdd320):
       +213 xorps xmm6, xmm6      -> karsilastirilan deger 0.0
       +415 rcx = eSt[0]  +410 rdx = xmm6  +419 karsilastirma
       +436 setle                 -> eSt[0] <= 0 ise devam
   Olculen: eSt = [35,50,3,4,12,25,15,0,14,18,28] -> eSt[0]=35, hepsi kapali.
   eSt[7]=0 oldugu halde Cursed Orb cikmiyordu, cunku ORTAK kapi kesiyor.

## Kritik ayrinti: tek sefer yazmak ISE YARAMAZ
Oyun HER ODA BASLANGICINDA (Controller_obj_Other_5) 11 slotu
`ReturnSpecificStat`tan yeniden dolduruyor.  Yazdiktan sonra harita
degisince eSt[0] tekrar 35 oluyordu.  Bu yuzden `estforce` her karede
geri yaziyor.  `eststat` ciktisi: `yazma=4` -> oda basinda 4 slot geri
yazilmis, marker'lar ONDAN SONRA bakmis.

Neden guvenli: bolge ureticisi eSt'i oda baslangicinin ICINDE kullaniyor,
kare geri-cagrisi o bittikten SONRA calisiyor.  Native denemede kaynagi
(ReturnSpecificStat donusu) uretim SIRASINDA ezmistim - oyun donuyordu.

## eSt slot haritasi (stat -> slot -> icerik)
    slot 0  stat 756  ORTAK KAPI
    slot 1  stat 675  Chaos Pillars
    slot 2  stat 682  Chaos Pillars
    slot 3  stat 676  Chaos Pillars
    slot 4  stat 705  Battlefield
    slot 5  stat 721  Battlefield
    slot 6  stat 666  Chaos Tower
    slot 7  stat 813  Cursed Orb
    slot 8  stat 770  Rift
    slot 9  stat 723  Shadow Realm
    slot 10 stat 823  Summon Portal

## TARIF
    estforce 0 0                 (ortak kapi - her zaman gerekli)
    estforce <icerigin slotu> 0  (bir veya birden fazla)
    multobj <marker idx> <adet>
    -> YENI haritaya gir
Chaos Pillars ornegi:
    estforce 0 0 / 1 0 / 2 0 / 3 0 ; multobj 4662 10  -> 18 pillar

## Density
`density <n>` -> S9 DoMultiCreate, Enemy_Creator* nesnelerini cogaltir.
Calisiyor: `extra spawners created=1764`.
ChatGPT'nin eklenti surumu density'yi ATLIYORDU
(`creators seen=489 boosted=0 skipped=489`); S9'un dokunulmamis kaynagi
derlenince duzeldi.  Yedegi: mods\aurie\BloodPactPlugin.dll.chatgpt_yedek

---

# ##### RIFT VE BATTLEFIELD COZULDU (2026-08-26 10:10) #####

Ayni haritada olculdu:
    861  : Chaos_Pillar_obj        : count=31
    3581 : Portal_Battlefield_obj  : count=3
    4126 : Rift_Portal_obj         : count=1
    4755 : Summoning_Portal_obj    : count=2

Hepsini OYUN uretti.  Biz yalnizca marker sayisini artirip kapiyi acik
tuttuk; konum, rarity, bolge kaydi oyunun kendi isi.

## Tarif (uc satir)
    estforce 0 0                  ortak kapi - her zaman gerekli
    estforce <slot> 0             icerigin kendi slotu
    multobj  <marker idx> <adet>  marker'i cogalt
    -> YENI haritaya gir

## Slot ve marker tablosu
    icerik            marker  eSt slot(lari)
    Chaos Pillars      4662    1, 2, 3
    Battlefield        4659    4, 5
    Chaos Tower        4663    6
    Cursed Orb         4665    7
    Rift               4672    8
    Shadow Realm       4674    9
    Summon Portal      4676    10
    Abyss              4658    (yalnizca ortak kapi)
    Traveling Merchant 4678    (yalnizca ortak kapi)
    ORTAK KAPI                 0

## Neden `estforce` sart - en kritik bulgu
`gaset eSt 0 0` tek basina ISE YARAMIYOR.  Oyun her oda baslangicinda
(Controller_obj_Other_5) 11 slotu ReturnSpecificStat'tan yeniden
dolduruyor; yazdigimiz deger marker'lar bakmadan siliniyor.
Olculdu: yazdiktan sonra harita degisince eSt[0] tekrar 35.

`estforce` degeri HER KAREDE geri yaziyor.  `eststat` ciktisindaki
`yazma=N` sayaci kac kez mudahale edildigini gosteriyor (63'e kadar
gozlendi).

Guvenli olmasinin sebebi: bolge ureticisi eSt'i oda baslangicinin ICINDE
kullaniyor, kare geri-cagrisi o cagri bittikten SONRA calisiyor.  Yani
uretim bozulmuyor, yalnizca marker'larin baktigi an degisiyor.
(Native runtime'da kaynagi uretim SIRASINDA ezmistim - oyun donuyordu.)

## Iki davranis notu
1) GECIKME NORMAL.  Marker oda basindan ~3 kare sonra kapiya bakiyor,
   zorlama kare kare yaziyor.  Ilk deneme kacarsa sonraki tutuyor;
   icerik hemen degil, birkac saniye icinde beliriyor.
   Kullanici once "cikmadi" dedi, sonra "simdi cikti".

2) DOZ SINIRI VAR.  Marker sayisi 10 + 10 + 10 iken oyun ayni haritada
   coktu (YYToolkit yeni bir yigin izi yazamadi -> kaynak tukenmesi).
   2-3 saglam calisiyor.  Yukseltmek gerekirse kademeli.

## createlog sayacini yanlis okuma tuzagi
`multobj` kopyalari orig'i DOGRUDAN cagirir, kancadan gecmez.
Bu yuzden createlog'da marker sayisi dusuk gorunur (ornegin count=5)
ama cogaltma calisiyordur.  Gercek gosterge `proof` satirindaki
`extra enemies created`.

---

# ##### NIHAI CALISAN AYAR - BESI DE (2026-08-26 10:30) #####

Kullanici hepsini oyunda gordu: Chaos Pillars, Battlefield, Rift,
Cursed Orb, Summon Portal.  Hepsini OYUN uretti.

## Calisan komut seti
    estfree all
    estforce 0 0          <- TEK gercek engel: ortak kapi
    estforce 7 14         <- Cursed Orb (dogal degeri 0, kendi kapisi >0 ister)
    multobj 4662 3        Chaos Pillars
    multobj 4659 6        Battlefield
    multobj 4672 6        Rift
    multobj 4665 6        Cursed Orb
    multobj 4676 6        Summon Portal
    -> YENI haritaya gir

## Iki kapi TERS yonde  (kaynaktan dogrulandi)
    eSt[0]     <= 0  gerekli   (ortak)     -> setle
    eSt[slot]   > 0  gerekli   (icerige ozel) -> setg
Cursed Orb'un slotu (7) DOGAL OLARAK 0 -> oyun onu hic uretmiyor.
Digerlerinin slotlari dogal olarak pozitif, yani kapilari zaten acik.

Polarite tablosu (mekanik govdelerinden cikarildi):
    Battlefield  [0]<=0  [4]>0  [5]>0
    CursedOrb    [0]<=0  [7]>0
    Rift         [0]<=0  [8]>0
    SummonPortal [0]<=0  [10]>0
    ShadowRealm  [0]<=0  [9]>0
    Abyss        [0]<=0
    TravelMerch  [0]<=0
    ChaosPillars (farkli kalip - setle/setg yok)

## YANLIS OLDUGU KANITLANAN IKI VARSAYIM
1) "Tum slotlari 0 yapalim"  -> icerik kapilarini KAPATIR.
   Rift/Battlefield'in bazen cikip bazen cikmamasi bundandi: estforce
   yalnizca deger farkliysa yaziyor, marker bizden once bakarsa dogal
   (pozitif) degeri gorup geciyordu - yaris durumu.
2) "Deger sans bolenidir, kucultelim" -> YANLIS.  Battlefield 12/25 iken
   calisiyordu, 1 yapinca DURDU.  Rift 14'te cikti, 1'de cikmadi.
   Degerler anlamli parametreler (dalga/seviye vb.); 1 yapmak mekanigi
   gecersiz kiliyor.
   DOGRUSU: dogal degerlere DOKUNMA, sansi artirmak icin MARKER SAYISINI
   yukselt (her marker bagimsiz atis).  S9'un yaptigi da buydu.

## Olculen sonuc
    861  : Chaos_Pillar_obj        : count=111
    3581 : Portal_Battlefield_obj  : count=7
    4126 : Rift_Portal_obj         : count=1
    4755 : Summoning_Portal_obj    : count=4
    eSt: 0 50 3 4 12 25 15 14 14 18 28   (yalnizca [0] ve [7] zorlanmis)

## Doz
10+10+10 marker ayni haritada oyunu cokertti.  3-6 arasi saglam.

## YYToolkit'in "olumcul hatasi" BIZDEN DEGIL
    Unable to find any instance for object index '257087' ...
    gml_Script_timer_system_update <- Menu_Controller_obj_Create_0
YYTK'nin ExecuteIt kancasi KALDIRILDIGI halde hata aynen olustu
(yigin izinde HkExecuteIt satiri yok).  Yani YYTK bu hatayi URETMIYOR,
YYError'i kancaladigi icin KAYDEDIYOR.  Oyunun kendi hatasi, runner
yakalayip devam ediyor.  Bizim eSt/multobj islemlerimizle ilgisi yok.

Yine de ExecuteIt kancasi kaldirildi (gereksiz: yalnizca
EVENT_OBJECT_CALL'u besliyor, biz EVENT_FRAME kullaniyoruz):
    yytk_src\...\Hooks\Hooks.cpp  ->  YYToolkit_noexec.dll (904192)
    Eski calisan surum: mods\aurie\YYToolkit.dll.calisan_yedek

---

## ABYSS COZULDU - 26.08.2026

### Kok sebep
`Spawn_Abyss_obj`, `Spawn_Mechanic_Parent_obj` ailesinin **`discoverable = true`
ayarlayan tek uyesi**.  Create olayi (rva 0x0afd05f0, sadece 352 bayt) yalnizca
iki sey yapar:

    discoverable      = 1                       <- AILEDE TEK
    m_activateMechanic = anon@119@gml_Object_Spawn_Abyss_obj_Create_0
                                                   (rva 0x0afce6d0)

Diger butun mekanikler (Chaos_Pillars, Battlefield, Rift, Cursed_Orb,
Summon_Portal, Chaos_Tower, Shadow_Realm, Traveling_Merchant, Cabin)
`discoverable`'a dokunmaz, yani dogrudan aktive olur.  Abyss'in aylardir
gorunmemesinin sebebi bu tek satir.

### Iki asamali akis  (gml_Object_Spawn_Mechanic_Parent_obj_Step_0, rva 0x0afe7ff0)

    +184   OKU discoverable      -> false ise kesif blogunu atla (je +2571)
    +270   discoverTimer         -> menzil icindeyken geri sayar (baslangic 30)
    +2126  OKU discoverRange     -> mesafe kontrolu (varsayilan 1000)
    +2554  call gml_Script_PlayerUpdateMinimap   <- KESIF: minimapte simge cikar
           discoverable = false, discoverTimer 30'a resetlenir
    +2581  if (activateTimer)    -> -1 (kapali) ise sona atla (je 0xafe8fc7)
    +2667  activateTimer -= dt
    +4039  YAZ alarm             -> Alarm_0 (rva 0x0afe4c90, 13152 B)
                                    -> m_activateMechanic -> "ABYSS SPAWNED"

Yani oyuncunun normalde yapmasi gereken: isaretcinin 1000 birim yakininda
30 saniye durmak (kesif + minimap simgesi), sonra uzerine gitmek (aktivasyon).

### Olculen degerler  (Act_09_01, 3 isaretci)
    discoverable    true -> (kesfedilince) false
    discoverRange   1000
    discoverTimer   30 (menzil disindayken donuk)
    collisionRadius 200      -- buyutmek aktivasyonu TETIKLEMEZ, denendi
    activateTimer   -1       -- kapali
    isaretci konumu 2939,2565   oyuncu 11395,4074  -> arada 8590 birim

### Cozum
Her `Spawn_Abyss_obj` ornegi icin:

    niset Spawn_Abyss_obj <n> discoverable 0
    niset Spawn_Abyss_obj <n> activateTimer 1

Otomatik hali: `ForgePact_YYTK\abyss.py [kac_tane]` - haritaya girilmesini
bekler, isaretci cikinca kendisi uygular.

### Cope giden varsayimlar (bir daha denenmesin)
- "nesneler yaratiliyor ama yok ediliyor"  -> YANLIS.  createpos.txt bos,
  instance_create hic cagrilmiyor.  destroywatch kancasi gereksizdi.
- "uzaklik/deactivate yuzunden gorunmuyor" -> YANLIS.
  instance_activate_object hicbir sey geri getirmedi.
- "collisionRadius buyutulurse aktive olur" -> YANLIS.  100000'e cikarildi,
  activateTimer -1'de kaldi.
- pullnear ile nesneleri oyuncunun yanina cekmek yaratilmayi hic etkilemiyor;
  eski "exists=1 sonra yok" olcumu pullnear ACIKKEN alinmisti ve yaniltti.

### Kesfedilen ek icerik
Abyss tam bir alan: 201 rutin, `Abyss_Realm_*` ve `Abyss_Jungle_*` dekor
setleri, `Abyss_Chest_obj` (idx 3), `Abyss_Lever_obj` (idx 77),
`Abyss_Gunpowder_obj`, `Abyss_Explosion_obj`, `Abyss_Servant_obj`,
`Abyssal_Hatred_Cultist_obj`.  Dekor degil, mekanik.

Ayrica ailede daha once fark edilmemis bir uye var: `Spawn_Cabin_obj`.

### Abyss - IsObtainablePlace denendi, SUCLU DEGIL  (26.08.2026, ikinci olcum)

`abyssforce on` komutu `gml_Script_IsObtainablePlace`'i YALNIZCA Abyss
mekaniginin cagri yigini icindeyken (g_InAbyss) true'ya zorlar.  Act
haritasinda 3 isaretci tetiklendi:

    mekanik = 3     mekanik ucunde de calisti
    sorgu   = 5     mekanik icindeyken konum 5 kez soruldu
    olmaz   = 4     oyun 4'unde "gecerli degil" dedi
    zorlanan= 4     dordu de true'ya cevrildi
    sonuc   : Abyss_Chest_obj = 0, Abyss_Lever_obj = 0, createpos.txt bos

Yani konum dogrulamasi tamamen bypass edildigi halde hicbir sey yaratilmadi.
IsObtainablePlace ELENDI.

Ayrica onemli: 3 calisma icin toplam 5 sorgu var.  Alarm_0'daki 999 denemelik
arama dongusu HIC donmuyor.  Mekanik konum aramasina bile girmeden, daha
erken bir kosulda vazgeciyor.

### Kalan supheliler (buradan devam edilecek)
`gml_Script_anon@119@gml_Object_Spawn_Abyss_obj_Create_0`, rva 0x0afce6d0,
uzunluk 7968.  Bilinen capalar:

    +484   eSt              (aciyoruz, geciyor)
    +799   gDataProtected
    +2487  DebugLogAddExt   "ABYSS SPAWNED"  <- buraya ULASILIYOR
    +2572  x
    +2651  y
    +2969  gml_Script_sCP           <- 1. suphel: bos donuyor olabilir
    +3602  x
    +3934  y
    +6002  gml_Script_IsObtainablePlace   <- ELENDI
    +6431  room
    +6489  gameLayer -> instance_create   <- buraya ULASILAMIYOR

Sonraki adim: +2487 ile +6002 arasini tam sokup (vn.ann, only=False)
`sCP` donusunun nasil kullanildigina ve aradaki dallanmalara bakmak.
`vn.py` degisken adi cozucusu hazir (ad = rip hedefi - 8).

Not: `abyssforce` deneysel bir kancadir, YAYIN paketinde varsayilan KAPALI
kalmali.

---

## Cokme davranisi - olculen degerler  (26.08.2026)

Butun olcumler `bp_ipc/census.txt` (gelistirme derlemesindeki `census` komutu,
her 60 karede `instance_number(all)` + marker sayilari).

### Ozel icerik tek basina  (density 1x)
    10x              plato 12.100-13.600      YASADI
    20x kuyruksuz    ~13.400 sonra olum       OLDU
    20x kuyruk+butce plato 8.400-10.800       YASADI  (iki farkli haritada)

### Density ile birlikte
    density 5 + icerik 10-20x    OLDU
    density 3 + icerik 20x       YASADI     <- dogrulanmis stabil kombinasyon

### Yaniltici olan sey: yukleme tepesi
Oda girisinde oyun bir kare boyunca 20.000-30.000 etkin ornege cikip hemen
8.000-11.000'e oturuyor (odanin butun nesneleri once etkin yaratiliyor,
sonra uzaktakiler devre disi birakiliyor).  Ornek:

    kare=15720  TOPLAM=836     marker=0
    kare=15780  TOPLAM=29613   marker=1     <- TEK KARELIK
    kare=15840  TOPLAM=8369    marker=20

Bu yuzden "22.575'te oldu" okumasi bir tavan DEGIL, olumden onceki son
orneklemin yukleme tepesine denk gelmesi.  Ayni sekilde sabit bir ornek
butcesi bu tepede devreye girip her seyi iptal edebiliyor - ilk denemede
tam olarak bu oldu (95 yaratimin 95'i atildi, hic icerik dogmadi).

### Uygulanan koruma  (her iki derlemede de acik, varsayilan)
- Marker kopyalari kuyruga alinir, karede `spread` (varsayilan 3) tanesi yaratilir
- `budget` (varsayilan 14000) asilmissa o kare ATLANIR - kuyruk KORUNUR
- 900 kareden fazla bekleyen oge dusurulur (sonraki haritaya tasmasin)

### ACIK KALAN IS: density korumasiz
Koruma yalnizca `g_ObjMult` (specialrate/multobj/multname) yolunu kapsiyor.
Density yolu (`g_CreatorMult`, `IsCreatorObject`) bilerek disarida birakildi
ve sinirsiz.  Density 5 + yuksek icerik bu yuzden cokuyor.

Dogru cozum: oda basina uretilecek EK URETICI sayisini sinirlamak.  Ornek
butcesi burada ise yaramaz (yukleme tepesi yuzunden).  Gereken capa:
Room Start (`gml_Object_Controller_obj_Other_5`) - mevcut `HookOneScript`
yalnizca `gml_Script_` onekini cozuyor, once onu genisletmek gerekiyor.
Sinir degeri, density 5'te olusan ek uretici sayisi olculerek belirlenmeli
(`proof` komutu `extra spawners created` sayisini veriyor).
