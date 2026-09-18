# Zindan anahtarlari - dogal dusus arastirmasi (27.08.2026)

Bu not ForgePact'in diğer araştırma belgeleriyle aynı kuralla yazıldı: ölçülen
davranış, script/nesne adları ve indeksleri, bizim kendi kodumuz ve
komutlarımız — no decompiled script text. Oyunun kodu yerelde okundu; ne
yaptığı burada kendi sözlerimizle anlatılıyor, adres tabloları, disassembly ve
bayt imzaları bilerek dışarıda (hub `AGENTS.md` › "Legal: Decompiled Output
Never Reaches Any Origin"). `tests/test_research_docs_no_decompiler_output.py`
bu belgeyi bu standartta tutar.

## 1. Neden düşmüyor — tek cümle

**Zindan anahtarları bozuk değil: LoadDrops case 12'nin kapısı, çalıştığı ölçülen DropKeys/DropChaosKey/DropBifrostKey kapılarıyla BAYT BAYT aynı ve aynı paydayı kullanıyor; tip 12 canavar `dropTable`'larında yerli olarak fazlasıyla var — sorun kodda değil, o tabloları taşıyan canavarların öldürülmemiş olmasında (ya da ölçümün kendisinde).**

### Bu turda YENİ ölçülenler (önceki turların üçü de bu noktada yanılmıştı)

Oyunun GML dizi-literali kurucusu yerel araçla tanındı ve çağrı düzeni
doğrulandı (eleman sayısı, ilk iki elemanın nasıl geçildiği). Buna göre oyunun
kod bölümünün tamamı tarandı: 14580 dizi-literali çağrısı. Aşağıdaki çiftler
`[tip, şans]` biçiminde `dropTable` girdileri.

**`gml_Script_LoadMonsterDropTables` — tip 12, 10 kez, yerli:**

| çift | kaç kez |
|---|---|
| [12, 5] | 2 |
| [12, 40] | 2 |
| [12, 75] | 2 |
| [12, 100] | 4 |

Karşılaştırma — **tip 11 (DropKeys, çalıştığı ölçülen) sadece 3 kez**: [11,**5**], [11,**9**], [11,**18**].

**`gml_Script_EnemyRaritySettings` — nadirlik dalları da ekliyor:**
- 3 kez **[12, 60]**
- 3 kez **[12, 70]**
- 5 kez [11, 18]
- 1 kez **[13, 0.7]** ← bu, DropDungeonKeys'in çarpanı (chances[13]); yani zindan anahtarı hattı oyunda **ayarlanmış**, ölü değil.

Bu beş dalın üçünde 11 ve 12 **birlikte** var ve chances[12]=60/70 iken chances[11]=18. Yani o canavarlarda zindan anahtarı kapısı normal anahtar kapısından **~3,9 kat daha açık**.

**Payda ortak — önceki turun "farklı string slotu" iddiası yanlış.** Dört case
de (11 / 12 / 31 / 40) zarın üst sınırını aynı yerden okuyor: `gDataProtected`
global'inin (değişken kimliği 104359, bkz. S10 notu) 0xAF sıradaki üyesi,
aynı string slotu üzerinden. Her case, üst sınırı bu üye olan bir tamsayı zarı
atıyor ve sonucu canavarın o tip için `chances` değeriyle karşılaştırıyor;
zar şanstan küçükse ilgili Drop* scripti çağrılıyor.

**Yapı da birebir aynı** — dört case bloğunun her birinde bloktan TEK şartlı
çıkış var (zar tutmazsa) ve dördü de aynı ortak çıkış noktasına gidiyor:

| case | script |
|---|---|
| 11 | DropKeys |
| **12** | **DropDungeonKeys** |
| 31 | DropBifrostKey |
| 40 | DropChaosKey |

31 ve 40 çalışıyor (kullanıcı chaos/bifrost alıyor). Aynı payda + aynı yapı + daha yüksek chances[12] → **case 12'nin sistematik olarak kapalı olması matematiksel olarak mümkün değil.**

### Dolayısıyla "c=0" ölçümünün iki açıklaması var, ÖNCE bunu ayırt edin

**(a) Doğru ölçüm, veri açıklaması:** tip 12, `LoadMonsterDropTables`'ta **her zaman** 18 (RubyKey) ve 17 (SatanicDice) ile aynı blokta; tip 11 ise 1/3/5/6/9/10 ile birlikte ayrı bloklarda. İkisi LoadMonsterDropTables'ta **hiç birlikte geçmiyor**. Yani farm edilen canavarlar "normal" tabloları kullanıyorsa 12 hiç görülmez.
> Hızlı testi: **Ruby Key veya Satanic Dice düşüyor mu?** Düşmüyorsa o tabloları hiç açmamışsınız demektir ve c=0 tamamen normaldir.

**(b) Ölçüm hatalı.** `ModuleMain.cpp`'deki "LootGroundCreate çalışma anında HİÇ çağrılmadı (ölçüldü: 0)" notu **kesinlikle yanlış** (DropKeys/DropChaosKey/DropBifrostKey'in tek yere-koyma yolu o ve anahtarlar düşüyor). Yani sayaç altyapısı en az bir kez yanlış sıfır üretmiş. Oyun logunda `HOOK INSTALLED on DropDungeonKeys` satırının gerçekten olup olmadığını doğrulayın.

---

## 2. Doğal oranı koruyarak açmanın yolu — VAR

Amaç: "normal anahtar zarı atılan her canavarda, zindan anahtarı için de **tam olarak bir tane** vanilya zarı atılsın". Zorlama yok, ölçek yok, sihirli sayı yok.

### Kancalanacak script: `gml_Script_LoadDrops` (adla çözülür)

Neden burası:
- `A[2]` = damla tipi, `A[8]` = chances dizisi — **ölçüldü** (50 case'in hepsinde aynı slot).
- chances GML dizisi, **referans tipi** → `array_set` yerinde çalışır (DropItem diziyi referansla geçiriyor).
- Orijinali ikinci kez çağırmak = kapıyı **atlamak değil**, kapıdan **bir kez daha geçmek**: aynı `gDataProtected` 0xAF üst sınırlı zar bu kez chances[12] ile karşılaştırılır; geçerse DropDungeonKeys kendi 26'lık havuzundan uniform seçip o anahtarın kendi `GetDropRate()` zarını atıyor (zincir aşağıda).
- Instance introspection **gerekmiyor** (ForgePact'te CInstance→id yolu yok).
- Sandık filtresine **gerek yok**: oyunun kod bölümünün tamamı tarandı, tip 11 veya 12 (INT64) içeren dropTable literali yalnızca `LoadMonsterDropTables`, `EnemyRaritySettings` ve `TalentsPirate`'ta var — **hiçbir sandık/varil/Goblin nesnesinde yok**. `A[2]==11` tetikleyicisi pratikte yalnızca canavarlarda ateşlenir.

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

        // 5) Izi temizle: chances[12]'yi geri 0 yap (ayni cerceve sonraki dList tipleri icin kullaniliyor).
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
| `droprate set <i> <deger>` | **zaten var** — anahtar başına ikinci katman. Havuz indeksleri: 9–27, 29, 30, 36–39 (+ scriptin 8. argümanı açıksa 28). Havuz `GetDungeonKeys` üzerinden ölçüldü. |

### Zincirin tamamı, açıldıktan sonra (hepsi oyunun kendi kodu)

Kapı açıldığında bir zindan anahtarının yere düşmesi için geçilmesi gereken
her şey, kendi sözlerimizle:

- **LoadDrops, tip 12:** `gDataProtected` 0xAF üst sınırlı dış zar chances[12]'yi tutturmalı.
- **DropDungeonKeys:** `GetDungeonKeys`'in verdiği 26 anahtarlık havuzdan uniform rastgele bir anahtar seçer.
- **Anahtarın oranı:** seçilen anahtarın repo kaydı `GetNormalRepoStruct` ile (kategori 12) alınır ve o anahtarın `GetDropRate` değeri okunur.
- **İç zarın sınırı:** bu oran chances[13] (yerli 0.7), şans/luck ve scriptin 6. argümanıyla ölçeklenip tamsayıya indirilir.
- **İç zar:** gerçek zar bu sınırla atılır; tutarsa anahtar `LootGroundCreate` ile yere konur.

---

## 3. Bayt yaması — gerek yok, önerilmiyor

- **Kanca zaten yeterli.** Not: `HookOneScript` (ModuleMain.cpp:894) Aurie'nin `MmCreateHook`'unu kullanıyor, bu **inline trampolin** yani kod baytlarına yazıyor. Yani "bayt yaması yok" kelimesi kelimesine doğru değil; doğru olan "**sabit ofsetli** yama yok" — adres `GetNamedRoutinePointer` ile isimden çözülüyor, güncellemeye dayanıklı. İstenen tam olarak bu.
- **Case 12 kapısının şartlı atlamasını etkisiz kılmak (LootForge'un notundaki öneri) YANLIŞ.** O atlama doğal zarın kendisi. Etkisiz kılmak = "hiçbir drop oranı olmadan zorlama" = kullanıcının açıkça reddettiği şey.
- Doğal oranı koruyan bir bayt yaması **teknik olarak mümkün değil**: doğallık, dList içeriğinden ve chances[] verisinden geliyor; ikisi de instance verisi, kodda sabit değil. Bir bayt yaması ancak sabit bir dallanmayı zorlayabilir.
- Zorunlu kalınsa bile bir imza araması gerekirdi; burada bilerek verilmiyor. Riski: LoadDrops'ta aynı desen 50 case'in hepsinde var, yanlış case'i yamalama ihtimali yüksek — ve adla kancalama zaten yeterli.

---

## 4. Önerilmeyen yollar ve nedenleri

| Yol | Neden hayır |
|---|---|
| **`DropDungeonKeys`'i eklentiden doğrudan çağırmak** (LootForge yolu) | LoadDrops'un giriş kapısını tamamen atlar → her ölümde zar. "Doğal zar" evet, "doğal sıklık" hayır. Ayrıca LootForge'un notları hatalı: `case_index=33` GML case değeri değil, **atlama tablosu ordinali**; gerçek drop_type **12**'dir. Ve "0, 1 ve 7. slotları tüketir" eksik — script 0,1,3,4,5,6,7'nin hepsini okuyor (arg3 = chances dizisinin kendisi, arg5 undefined ise şans hesabı undefined ile çarpılıp çöker). Yanlış argümanlarla çağrılmış olması, LootForge'un "oransız" davranmasının teknik sebebi olabilir. |
| **`self.dropTable`'a elle `[12,N]` eklemek** (LoadMonsterDropModifiers / LoadMonsterDropTables kancası) | Tip 12 çoğu boss/elit tablosunda **zaten var** (yukarıdaki 16 literal). Eklemek: (a) boss'un yerli chances[12]=100'ünü ezer → **nerf**; (b) canavarın Alarm_4 olayı `dropTable`'daki her tipi koşulsuz olarak dList'e eklediği için dList'e ikinci bir 12 sokar → **aynı ölümde iki bağımsız zar**. Ayrıca `LoadMonsterDropModifiers` 10 yerden çağrılıyor ve Pile_Parent / Destructible_Parent / Destructible_NoCollision_Parent / Cursed_Orb / TalentsPirate'ta `dropTable` değişkeni **hiç yok** → korumasız kanca, tanımsız bir listeye ekleme yapmaya çalışıp GML hatası verir. |
| **`DropKeys`'i kancalayıp repo indeksini çeşitlendirmek** | Oran DropKeys'in oranı olur; her zindan anahtarının kendi droprate'i devre dışı kalır; zindan anahtarları normal anahtarların **yerine** düşer, toplam artmaz. |
| **`chances` "array_create(59) sınır dışı" hipotezi** | Ölü. Ölçüm: DropItem chances dizisini 70 elemanlı ve 0 ile dolu yaratıyor. İndeks 12 fazlasıyla sınır içinde. |
| **Case 11'in "farklı payda" kullandığı iddiası** | Ölü. Dört case de aynı string slotunu, aynı `gDataProtected` değişken kimliğini ve aynı 0xAF üyesini okuyor. |

---

## Hâlâ ölçülmemiş tek şey

Paydanın **değeri**: `gDataProtected`'in 0xAF üyesi. Üyenin adını tutan string slotu dosyada boş (çalışma anında doluyor), statik okunamaz. Yerli değerlerin 1..100 aralığında olması ve 100'ün "kesin düşüş" gibi kullanılması paydanın ~100 olduğunu **kuvvetle düşündürüyor**, ama bu tahmindir. `dungeonkey probe` bunu da yan ürün olarak çözer; zaten `chance auto` modu chances[11]'i kopyaladığı için paydayı bilmenize gerek yok.

**Kritik dosyalar:** `C:\Users\falor\OneDrive\Belgeler\Hero Siege\source\ForgePact\plugin\ModuleMain.cpp` (kanca: satır 894 `HookOneScript`, satır 1018 `DROP_HOOK`, satır 1032 `DROP_HOOK(DropDungeonKeys)`) · analiz betikleri yerel scratchpad'de (`lit.py`, `lit2.py`, `basestr.py`, `exits.py`, `dicall.py`, `ers.txt` bu turda eklendi; depoya girmez).
