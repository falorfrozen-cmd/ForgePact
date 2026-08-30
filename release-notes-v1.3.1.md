# ForgePact v1.3.1

This hotfix makes plugin installation and removal safe when Hero Siege has been
updated or an old `.aurie_backup` is still present.

## Fixed

- A stale backup from an older game build can no longer be restored over the
  currently installed `Hero_Siege.exe`.
- When the selected game executable is clean, ForgePact preserves an outdated
  backup as `Hero_Siege.exe.aurie_backup.stale-<date>` and creates a new,
  hash-verified backup atomically before patching.
- When the executable is already Aurie-patched, installation stops before any
  mod DLL is changed unless a matching clean backup can be proven.
- **Remove Plugin** restores only a clean backup that exactly matches the base
  of the currently patched executable. It never downgrades a newer clean game
  executable.
- If AuriePatcher fails, ForgePact rolls the live executable back only from the
  verified clean backup.

## Strict verification

ForgePact now validates the complete PE relationship between the clean backup
and the patched executable: the final `.aurie` section geometry, the embedded
original entry point, all original headers, every original section, and all
bytes up to the clean file boundary must match. Missing, modified, truncated,
or cross-build backups fail closed without changing the game installation.

## Updating

Extract the complete ZIP and replace the previous ForgePact folder. Existing
settings are kept in `%LOCALAPPDATA%/Hero_Siege/forgepact.json`.

Offline / EAC-off play only.
