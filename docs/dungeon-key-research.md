# Zindan anahtarlari - dogal dusus arastirmasi (27.08.2026)

## 1. Neden düşmüyor — tek cümle

**Zindan anahtarları bozuk değil: LoadDrops case 12'nin kapısı, çalıştığı ölçülen DropKeys/DropChaosKey/DropBifrostKey kapılarıyla BAYT BAYT aynı ve aynı paydayı kullanıyor; tip 12 canavar `dropTable`'larında yerli olarak fazlasıyla var — sorun kodda değil, o tabloları taşıyan canavarların öldürülmemiş olmasında (ya da ölçümün kendisinde).**

### Bu turda YENİ ölçülenler (önceki turların üçü de bu noktada yanılmıştı)

`sub_c60d550` = GML dizi-literali kurucusu. Çağrı düzeni sökülerek doğrulandı (0xc60d550 +0 `mov [rsp+0x18],r8d` / +5 `mov [rsp+0x20],r9` / +63 `lea rdi,[rsp+0x68]` → **eleman[0]=r9, eleman[1]=[rsp+0x20]**, sayı=r8d). Buna göre `.text`'in tamamı tarandı (14580 literal çağrısı):

**`gml_Script_LoadMonsterDropTables` (0x432d190) — tip 12, 10 kez, yerli:**

| adres | çift | | adres | çift |
|---|---|---|---|---|
| 0x432e2e7 | [12, 5] | | 0x433514e | [12, 100] |
| 0x432e8ca | [12, 5] | | 0x4336e41 | [12, 100] |
| 0x432fbcf | [12, 40] | | 0x4338f8e | [12, 100] |
| 0x4330559 | [12, 40] | | 0x4339aed | [12, 100] |
| 0x43316a3 | [12, 75] | | | |
| 0x4333327 | [12, 75] | | | |

Karşılaştırma — **tip 11 (DropKeys, çalıştığı ölçülen) sadece 3 kez**: 0x432f33b [11,**5**], 0x432f945 [11,**9**], 0x43330d4 [11,**18**].

**`gml_Script_EnemyRaritySettings` (0x19bad60, 0x232900 bayt) — nadirlik dalları da ekliyor:**
- 0x1b80caf / 0x1b83f45 / 0x1b8c705 → **[12, 60]**
- 0x1b8151e / 0x1b847b4 / 0x1b8cf74 → **[12, 70]**
- 0x1b82183 / 0x1b859f7 / 0x1b8819d / 0x1b8a943 / 0x1b8dde8 → [11, 18]
- 0x19cb27b → **[13, 0.7]** ← bu, DropDungeonKeys'in çarpanı (chances[13]); yani zindan anahtarı hattı oyunda **ayarlanmış**, ölü değil.

Bu beş dalın üçünde 11 ve 12 **birlikte** var ve chances[12]=60/70 iken chances[11]=18. Yani o canavarlarda zindan anahtarı kapısı normal anahtar kapısından **~3,9 kat daha açık**.

**Payda ortak — önceki turun "farklı string slotu" iddiası yanlış.** Dört case de aynı slotu okuyor:

```
case 11 / 12 / 31 / 40 : mov rdx,[rip] -> 0x11C68CB0 ; sub_1afb80
                         mov edx,[rip] -> varid 0x11BE1DF0 = "gDataProtected" ; r8d=0xAF
```
(0x4289a10 case 12; case 11/31/40 birebir aynı). Zar: `irandom(gDataProtected.<0xAF>) < chances[tip]`.

**Yapı da birebir aynı** — her blokta bloktan TEK şartlı çıkış var, hepsi 0x429714d'ye:

| case | blok | kapı JE | çağrı |
|---|---|---|---|
| 11 DropKeys | 0x428946b–0x42899b3 | +1079 `0x42898a2` | `0x42899a9` |
| **12 DropDungeonKeys** | 0x42899b3–0x428a013 | +1087 `0x4289df2` | `0x428a009` |
| 31 DropBifrostKey | 0x428a013–0x428a563 | +1087 `0x428a452` | `0x428a559` |
| 40 DropChaosKey | 0x428a563–0x428aa7e | +1087 `0x428a9a2` | `0x428aa74` |

31 ve 40 çalışıyor (kullanıcı chaos/bifrost alıyor). Aynı payda + aynı yapı + daha yüksek chances[12] → **case 12'nin sistematik olarak kapalı olması matematiksel olarak mümkün değil.**

### Dolayısıyla "c=0" ölçümünün iki açıklaması var, ÖNCE bunu ayırt edin

**(a) Doğru ölçüm, veri açıklaması:** tip 12, `LoadMonsterDropTables`'ta **her zaman** 18 (RubyKey) ve 17 (SatanicDice) ile aynı blokta; tip 11 ise 1/3/5/6/9/10 ile birlikte ayrı bloklarda. İkisi LoadMonsterDropTables'ta **hiç birlikte geçmiyor**. Yani farm edilen canavarlar "normal" tabloları kullanıyorsa 12 hiç görülmez.
> Hızlı testi: **Ruby Key veya Satanic Dice düşüyor mu?** Düşmüyorsa o tabloları hiç açmamışsınız demektir ve c=0 tamamen normaldir.

**(b) Ölçüm hatalı.** `ModuleMain.cpp`'deki "LootGroundCreate çalışma anında HİÇ çağrılmadı (ölçüldü: 0)" notu **kesinlikle yanlış** (DropKeys/DropChaosKey/DropBifrostKey'in tek yere-koyma yolu o ve anahtarlar düşüyor). Yani sayaç altyapısı en az bir kez yanlış sıfır üretmiş. Oyun logunda `HOOK INSTALLED on DropDungeonKeys` satırının gerçekten olup olmadığını doğrulayın.

---

## 2. Doğal oranı koruyarak açmanın yolu — VAR

Amaç: "normal anahtar zarı atılan her canavarda, zindan anahtarı için de **tam olarak bir tane** vanilya zarı atılsın". Zorlama yok, ölçek yok, sihirli sayı yok.

### Kancalanacak script: `gml_Script_LoadDrops` (rva 0x4283ca0)

Neden burası:
- `A[2]` = damla tipi, `A[8]` = chances dizisi — **ölçüldü** (0x4289a4a / 0x4289a59; 50 case'in hepsinde aynı slot).
- chances GML dizisi, **referans tipi** → `array_set` yerinde çalışır (DropItem 0x186ed5e'de `&[rbp+0x298]` geçiyor).
- Orijinali ikinci kez çağırmak = kapıyı **atlamak değil**, kapıdan **bir kez daha geçmek**: `irandom(gDataProtected.<0xAF>) < chances[12]`, geçerse DropDungeonKeys kendi 26'lık havuzundan uniform seçip o anahtarın kendi `GetDropRate()` zarını atıyor (0x182dc1b → 0x182e28e → 0x1830c37 → 0x18311ef).
- Instance introspection **gerekmiyor** (ForgePact'te CInstance→id yolu yok).
- Sandık filtresine **gerek yok**: `.text`'in tamamı tarandı, tip 11 veya 12 (INT64) içeren dropTable literali yalnızca `LoadMonsterDropTables`, `EnemyRaritySettings` ve `TalentsPirate`'ta var — **hiçbir sandık/varil/Goblin nesnesinde yok**. `A[2]==11` tetikleyicisi pratikte yalnızca canavarlarda ateşlenir.

### Kod iskeleti (ForgePact `ModuleMain.cpp`)

```cpp
// ===== Zindan anahtari: dogal kapiyi ac =====
static PFUNC_YYGMLScript g_OrigLoadDrops = nullptr;
static bool  g_DkOn      = false;
static int   g_DkChance  = -1;      // -1 = "auto": ayni canavarin chances[11]'ini kopyala
static volatile long g_DkRolls = 0, g_DkSkipNative = 0;
static int   g_DkProbe   = 0;       // >0 ise ilk N cagriyi logla

static RValue& Hook_LoadDrops(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    // 1) Once VANILYA davranis, hic dokunmadan.
    RValue& res = g_OrigLoadDrops ? g_OrigLoadDrops(S, O, R, argc, A) : R;
    if (!A || argc < 9 || !A[2] || !A[8]) return res;

    try {
        int t = (int)A[2]->ToDouble();

        if (g_DkProbe > 0) {                       // TESHIS MODU (once bunu calistirin)
            RValue c_t  = g_Yytk->CallBuiltin("array_get", { *A[8], RValue((double)t) });
            RValue c_12 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(12.0) });
            RValue c_13 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(13.0) });
            std::ofstream f(IPC_DIR + "\\loaddrops.txt", std::ios::app);
            f << "tip=" << t << " chances[tip]=" << c_t.ToDouble()
              << " chances[12]=" << c_12.ToDouble()
              << " chances[13]=" << c_13.ToDouble() << "\n";
            g_DkProbe--;
        }

        if (!g_DkOn || t != 11) return res;        // yalnizca normal-anahtar zarindan sonra

        // 2) Bu canavarda 12 ZATEN yerli mi?  Oyleyse dokunma (cift zar olmasin).
        RValue cur = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(12.0) });
        if (cur.ToDouble() > 0.0) { InterlockedIncrement(&g_DkSkipNative); return res; }

        // 3) Orani ayni canavarin normal-anahtar oranindan kopyala (ya da sabit).
        RValue base = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(11.0) });
        double n = (g_DkChance >= 0) ? (double)g_DkChance : base.ToDouble();
        if (n <= 0.0) return res;

        g_Yytk->CallBuiltin("array_set", { *A[8], RValue(12.0), RValue(n) });

        // 4) AYNI arguman dizisi, yalnizca slot 2 = 12.  Kapi ve zar %100 vanilya.
        RValue twelve(12.0);
        std::vector<RValue*> A2(A, A + argc);
        A2[2] = &twelve;
        RValue r2;
        g_OrigLoadDrops(S, O, r2, argc, A2.data());
        InterlockedIncrement(&g_DkRolls);

        // 5) Izi sil: chances[12]'yi geri 0 yap (ayni cerceve sonraki dList tipleri icin kullaniliyor).
        g_Yytk->CallBuiltin("array_set", { *A[8], RValue(12.0), RValue(0.0) });
    } catch (...) {}
    return res;
}

// kurulum (InstallEnemyHooks yaninda):
HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
```

### Eklenecek paneldeki komutlar

| komut | işi |
|---|---|
| `dungeonkey probe <N>` | **İLK BUNU ÇALIŞTIRIN.** N LoadDrops çağrısını `bp_ipc/loaddrops.txt`'e döker. `tip=12` satırı görürseniz sorun zaten yok, hiçbir yama gerekmiyor. |
| `dungeonkey on` / `off` | kapıyı açar/kapatır |
| `dungeonkey chance <N\|auto>` | `auto` = chances[11]'i kopyala (yerli 5/9/18); sabit sayı da verilebilir. Yerli ölçek 1..100'dür (100 = neredeyse kesin), o aralıkta kalın. |
| `dungeonkey stats` | `rolls` ve `skipNative` sayaçları |
| `droprate set <i> <deger>` | **zaten var** — anahtar başına ikinci katman. Havuz indeksleri: 9–27, 29, 30, 36–39 (+ argument7 açıksa 28). GetDungeonKeys 0x1cd0d80'de ölçüldü. |

### Zincirin tamamı, açıldıktan sonra (hepsi oyunun kendi kodu)

```
LoadDrops(tip=12) → irandom(gDataProtected.<0xAF>) < chances[12]      (0x4289df2)
  → DropDungeonKeys(0x182d950)
      → GetDungeonKeys → 26 anahtarlik havuz, irandom ile UNIFORM secim   (0x182dc82)
      → GetNormalRepoStruct(12, 0, i) → with(anahtar) GetDropRate()       (0x182e28e / 0x182e37a)
      → chance = floor(oran * chances[13]) * sans/luck * argument5        (chances[13] yerli 0.7)
      → irandom(chance) GERCEK ZAR                                        (0x1830c37)
      → LootGroundCreate                                                  (0x18311ef)
```

---

## 3. Bayt yaması — gerek yok, önerilmiyor

- **Kanca zaten yeterli.** Not: `HookOneScript` (ModuleMain.cpp:894) Aurie'nin `MmCreateHook`'unu kullanıyor, bu **inline trampolin** yani kod baytlarına yazıyor. Yani "bayt yaması yok" kelimesi kelimesine doğru değil; doğru olan "**sabit ofsetli** yama yok" — adres `GetNamedRoutinePointer` ile isimden çözülüyor, güncellemeye dayanıklı. İstenen tam olarak bu.
- **0x4289df2'deki JE'yi NOP'lamak (LootForge'un −0x217 notu) YANLIŞ.** O JE doğal zarın kendisi. NOP'lamak = "hiçbir drop oranı olmadan zorlama" = kullanıcının açıkça reddettiği şey.
- Doğal oranı koruyan bir bayt yaması **teknik olarak mümkün değil**: doğallık, dList içeriğinden ve chances[] verisinden geliyor; ikisi de instance verisi, kodda sabit değil. Bir bayt yaması ancak sabit bir dallanmayı zorlayabilir.
- Zorunlu kalınırsa imza araması şablonu (yine de önermiyorum): case 12 kapısının önündeki `48 8D 4C 24 30 / E8 …sub_1b50c0 / 48 8B C8 / E8 …sub_c605880 / 48 8B D3 / E8 …sub_186360 / 84 C0 / 0F 84` dizisi; E8 hedeflerini rel32 ile çözüp doğrulayın, sabit RVA kullanmayın. Riski: LoadDrops'ta bu desen 50 case'te de var, yanlış case'i yamalama ihtimali yüksek.

---

## 4. Önerilmeyen yollar ve nedenleri

| Yol | Neden hayır |
|---|---|
| **`DropDungeonKeys`'i eklentiden doğrudan çağırmak** (LootForge yolu) | LoadDrops'un giriş kapısını tamamen atlar → her ölümde zar. "Doğal zar" evet, "doğal sıklık" hayır. Ayrıca LootForge'un notları hatalı: `case_index=33` GML case değeri değil, **atlama tablosu ordinali**; gerçek drop_type **12**'dir. Ve "0, 1 ve 7. slotları tüketir" eksik — script 0,1,3,4,5,6,7'nin hepsini okuyor (arg3 = chances dizisinin kendisi, arg5 undefined ise `chance*undefined` çöker). Yanlış argümanlarla çağrılmış olması, LootForge'un "oransız" davranmasının teknik sebebi olabilir. |
| **`self.dropTable`'a elle `[12,N]` eklemek** (LoadMonsterDropModifiers / LoadMonsterDropTables kancası) | Tip 12 çoğu boss/elit tablosunda **zaten var** (yukarıdaki 16 adres). Eklemek: (a) boss'un yerli chances[12]=100'ünü ezer → **nerf**; (b) Alarm_4'teki `array_push(dList, e[0])` (0x89d2181) koşulsuz olduğu için dList'e ikinci bir 12 sokar → **aynı ölümde iki bağımsız zar**. Ayrıca `LoadMonsterDropModifiers` (0x1927020) 10 yerden çağrılıyor ve Pile_Parent / Destructible_Parent / Destructible_NoCollision_Parent / Cursed_Orb / TalentsPirate'ta `dropTable` değişkeni **hiç yok** → korumasız kanca `ds_list_add(undefined,…)` ile GML hatası verir. |
| **`DropKeys`'i kancalayıp repo indeksini çeşitlendirmek** | Oran DropKeys'in oranı olur; her zindan anahtarının kendi droprate'i devre dışı kalır; zindan anahtarları normal anahtarların **yerine** düşer, toplam artmaz. |
| **`chances` "array_create(59) sınır dışı" hipotezi** | Ölü. Ölçüm: `array_create(70, 0)` — DropItem 0x1856916, sabitler 0x11AC92F0 (INT64 70) ve 0x11FBB258 (0.0). İndeks 12 fazlasıyla sınır içinde. |
| **Case 11'in "farklı payda" kullandığı iddiası** | Ölü. Dört case de aynı `.data` slotu 0x11C68CB0 + aynı `gDataProtected` varid 0x11BE1DF0 + aynı `r8d=0xAF`. |

---

## Hâlâ ölçülmemiş tek şey

Paydanın **değeri**: `gDataProtected.<üye 0xAF>`. String slotu 0x11C68CB0 dosyada sıfır (çalışma anında doluyor), statik okunamaz. Yerli değerlerin 1..100 aralığında olması ve 100'ün "kesin düşüş" gibi kullanılması paydanın ~100 olduğunu **kuvvetle düşündürüyor**, ama bu tahmindir. `dungeonkey probe` bunu da yan ürün olarak çözer; zaten `chance auto` modu chances[11]'i kopyaladığı için paydayı bilmenize gerek yok.

**Kritik dosyalar:** `C:\Users\falor\OneDrive\Belgeler\Hero Siege\source\ForgePact\plugin\ModuleMain.cpp` (kanca: satır 894 `HookOneScript`, satır 1018 `DROP_HOOK`, satır 1032 `DROP_HOOK(DropDungeonKeys)`) · analiz betikleri `C:\Users\falor\AppData\Local\Temp\claude\C--Users-falor-OneDrive-Belgeler-Hero-Siege\3851382c-2a3a-4047-9507-49570d266a38\scratchpad\` (`lit.py`, `lit2.py`, `basestr.py`, `exits.py`, `dicall.py`, `ers.txt` bu turda eklendi).