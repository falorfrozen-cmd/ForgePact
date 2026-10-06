# Modified AurieCore — corresponding source notice

The `AurieCore.dll` shipped in the ForgePact release is **Aurie (Aurie Core),
modified** by the Hero Siege Offline Toolkit project, and is covered by
**AGPL-3.0** (https://github.com/AurieFramework/Aurie). It is not upstream's
binary, and upstream did not make, review or endorse the change. Problems with
this build belong in the toolkit's issue tracker, not upstream's.

Every ForgePact release before v2.2.0 shipped upstream's own v2.0.2
release DLL, unmodified (`18e3a1de980f487a6b3858b673d2030e96984dd96de3a047b43a263a5ba829ae`).
This one replaces it. `AuriePatcher.exe` is still upstream's v2.0.2 release
binary, unmodified.

## What is shipped

| | |
| --- | --- |
| File | `modfiles/AurieCore.dll` (`modfiles_shipped/AurieCore.dll` in a ForgePact checkout) |
| Size | 968,704 bytes |
| SHA-256 | `3cf98af99a0ef38dea1a2d4627e6466631f6e7dd2b254b1a80e5438ac06800cb` |
| Base | Aurie tag `v2.0.2` |
| Series revision | `hs.1` (the same string this DLL writes to `aurie.log` straight after Aurie's own `loaded at` line) |
| Built from | hub repository `hero-siege-offline-toolkit`, at the commit `AurieCore-BUILD-INFO.json` records as `hub.commit` |
| Published as | hub release `aurie-v2.0.2-hs.1` (a library release, not a hub build) |

## What changed, in one paragraph

While a hook is written or removed, Aurie suspends every other thread of the
game. Upstream finds those threads by taking two snapshots of every thread on
the whole system, which cost ForgePact about 69 ms per hook it installed
(ForgePact issue #151). The modified freeze lists only the game's own threads,
repeats the walk until no new thread appears, waits until each suspension has
taken effect, and resumes exactly the threads it suspended. When it cannot do
that it falls back to upstream's walk and says so in one log line. Hooks
attach exactly as before: no exported function, signature or plugin-facing
header changes, and a plugin built against upstream's v2.0.2 headers loads
unchanged.

## Corresponding source

The complete corresponding source of this binary is, in the hub repository
(`https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit`) at the commit
`AurieCore-BUILD-INFO.json` names:

1. **Upstream Aurie** at the pin recorded in `third_party/aurie/upstream.json`:
   repository `https://github.com/AurieFramework/Aurie`, tag `v2.0.2`.
2. **The patch series**, `third_party/aurie/patches/`, applied in the order
   given by `patches/series` — both patches, none skipped:
   1. `0001-series-identity.patch`
   2. `0002-per-process-hook-freeze.patch`
3. **The build tool**, `tools/build_aurie.py`, from the same hub commit — it
   exports the pin, applies the series, builds upstream's own
   `Aurie/AurieCore.vcxproj` (Release, x64), runs the series' host test and
   checks the resulting DLL for every log line the patches declare.

`third_party/aurie/README.md` is the guide to the series and
`third_party/aurie/NOTICE.md` the hub's own modification notice: each patch's
message states why it exists, the evidence, what happens when it cannot do its
job, the log lines it adds and its upstream status (not submitted).

`AurieCore-BUILD-INFO.json`, committed beside this file and shipped with the
binary, records the upstream commit and tree, the hub commit, the sha256 of
every patch, the toolchain versions and the DLL's own size and sha256. The hub
release that carries this DLL also carries a deterministic zip of
`third_party/aurie/` itself, so the patches and pin travel with the binary even
without a hub checkout. The licence text is upstream's, byte for byte, at
`third_party/aurie/LICENSE` in the hub.

## Verification status

`live_gameplay_verified` in `AurieCore-BUILD-INFO.json` is `false` and always
will be — the build tool cannot know whether a session played correctly. The
hub's `third_party/aurie/README.md` launch gate is the record of what has
actually been launched and observed instead.
