# Modified YYToolkit — corresponding source notice

The `YYToolkit.dll` shipped in the ForgePact release is **YYToolkit, modified**,
and is covered by **AGPL-3.0** (https://github.com/AurieFramework/YYToolkit).

This directory used to hold a notice plus two whole-file copies. It no longer
does: those two files were not the complete source for the binary they
accompanied (the DLL's own strings showed undocumented changes; see the
provenance history below), and this repository is not where a change to
YYToolkit is made. **The complete corresponding source lives in the toolkit
hub, not here.**

## What is shipped

| | |
| --- | --- |
| File | `modfiles_shipped/YYToolkit.dll` |
| Size | 950,784 bytes |
| SHA-256 | `51a393d7e5291ad76bdb85b9f44faf5178b6b20e0ce8432fa26bdaf9e21eadf8` |
| Series revision | `hs.1` (the same string this DLL writes as the first line of `YYToolkit.log`) |
| Built from | hub repository `hero-siege-offline-toolkit`, commit `848a26edb4f9f826c68d85c8223fc117e07c95ed` |

## Corresponding source

The complete corresponding source of this binary is, in the hub repository
(`https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit`) at the commit
named above:

1. **Upstream YYToolkit** at the pin recorded in `third_party/yytoolkit/upstream.json`:
   repository `https://github.com/AurieFramework/YYToolkit`, tag `v4.0.1`,
   commit `5a95e46cc4b4e99f16ee3dc0f2ab9ea3c77d18db`.
2. **The patch series**, `third_party/yytoolkit/patches/`, applied in the order
   given by `patches/series` — all seven patches, none skipped:
   1. `0001-runner-interface-scan-hint.patch`
   2. `0002-runner-interface-quiet-init-dump.patch`
   3. `0003-executeit-hook-off-by-design.patch`
   4. `0004-functions-array-validation.patch`
   5. `0005-yyerror-report-once-and-measure.patch`
   6. `0006-release-build-optimised-and-series-identity.patch`
   7. `0007-runner-interface-refuse-unfound-interface.patch`
3. **The build tool**, `tools/build_yytoolkit.py`, from the same hub commit —
   it exports the pin, applies the series, builds upstream's own
   `YYToolkit.vcxproj` (Release, x64) and checks the resulting DLL for every
   log marker the patches declare.

`third_party/yytoolkit/README.md` is the guide to the series: each patch's own
message states why it exists, the evidence, what happens when its heuristic
does not match, the log lines it adds and its upstream status.

`YYToolkit-BUILD-INFO.json`, committed beside this file and shipped with the
binary, records the upstream commit and tree, the hub commit, the sha256 of
every patch, the toolchain versions and the DLL's own size and sha256. The
hub release that carries this DLL (tag `yytoolkit-v4.0.1-hs.1`) also carries a
deterministic zip of `third_party/yytoolkit/` itself, so the patches and pin
travel with the binary even without a hub checkout.

## About the previously distributed binary

The file this replaces — `bb113eefc9a5d485231ced1dc85d773dbc6b762ee680214851c56541359ad297`,
904,192 bytes, shipped in every ForgePact release from v1.3.1 to v1.3.16 — was
documented by this file's previous version as two changes to unmodified
upstream YYToolkit. That claim was wrong: strings in the binary showed
several undocumented changes (a functions-array candidate filter, a startup
breadcrumb tracer writing to a hardcoded path under the builder's user
profile, an unexplained `VirtualQuery` import, a page pre-filter present in
the committed file but never listed), and its source was never fully
recovered — a fresh build of the documented tree alone did not start the
game. See `third_party/yytoolkit/README.md` ("Why this exists") and
`third_party/yytoolkit/NOTICE.md` for the full history. This series replaces
that binary; it does not reproduce it, and it does not repeat that binary's
claim about what else in it was unmodified.

## Verification status

`live_gameplay_verified` in `YYToolkit-BUILD-INFO.json` is `false` and always
will be — the build tool cannot know whether a session played correctly. The
hub's `third_party/yytoolkit/README.md` launch gate is the record of what has
actually been launched and observed instead.
