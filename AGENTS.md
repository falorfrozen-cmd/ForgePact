# AGENTS.md

`ForgePact` is developed as a submodule of the
[hero-siege-offline-toolkit](https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit)
hub. The rules for working on it live there, not here:

- **Repository-wide guidance:** [`../AGENTS.md`](../AGENTS.md) —
  [view on GitHub](https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit/blob/main/AGENTS.md)
- **This module's guide:** [`../docs/submodules/ForgePact/instructions.md`](../docs/submodules/ForgePact/instructions.md) —
  [view on GitHub](https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit/blob/main/docs/submodules/ForgePact/instructions.md)

Read both before changing anything. The module guide is not optional
background: it carries the architecture, entry points, test commands, build and
packaging steps, and the record of approaches already tried and ruled out, and
several of its requirements are invisible in the code itself.

**Do not add rules to this file.** Module rules belong in that `instructions.md`
and repository-wide rules in the hub's `AGENTS.md`, so there is only ever one
copy to keep true. This file is a signpost.

## If you only have this repository

Those relative paths resolve inside a full toolkit checkout, where this module
sits next to the hub's `docs/`. In a standalone clone they do not — use the
GitHub links above, and clone the hub alongside this repository if you are going
to do real work, since several modules also build against `hs-game-sdk/` from it.

Two rules matter too much to leave behind a link you might not follow:

### Never commit decompiled or disassembled game source

Reading the game's own script bodies locally (Ghidra, IDA, UndertaleModTool,
dnSpy) to understand a mechanism is legitimate research. Committing that output
is not — **and the rule covers this repository's own remote, not just the hub.**
No GML script bodies, decompiled bytecode, decompiler listings or exports, in
any tracked file, commit message, comment, issue or pull request.

What is fine, because it documents interoperability rather than the game's
expression: object/script/room/sprite/sound names and their indices, measured
runtime behaviour (what a call does, what it reads or writes, crash signatures,
before/after values), our own code that reacts to it, and the offsets and
calling conventions needed to hook or read memory. Paraphrase mechanisms in your
own words; never paste the script text. The full rule, including the artifact
paths `.gitignore` blocks as a backstop, is in the hub's `AGENTS.md`.

### Documentation is part of the change, not a follow-up

When a change touches features, workflows, architecture or dependencies, update
the `README.md`, the module guide, and any affected docs in the same change.
A pull request that leaves them stale is unfinished.

## Two things that are easy to get wrong in this module

Both are spelled out in the module guide; they are repeated here only because
each has already cost a shipped mistake.

- **A player-visible change adds player-language notes to
  `release-notes-vX.Y.Z.md`.** Player language, symptom before fix, and never
  claim a fix that is not real — release notes are read by players deciding
  whether to update. The tag workflow (`forgepact-tag.yml`) composes the
  draft release body from these files, newest first, so writing the file in
  the PR is what makes tagging need no extra step later. When a version has
  no file yet, the workflow falls back to GitHub's own generated notes under
  a banner — those must be rewritten into player language before the draft is
  published, the same as a hand-written file would be. **Once a release is
  published, delete the notes files it carried** (its own and every skipped
  version it rolled up): the published release page is then the record, and
  git history keeps the file. Only unpublished versions' notes live in the
  tree, so the repo root never accumulates them.
- **`plugin_build\build.bat dev` needs the literal `dev`.** Any other argument,
  including none at all, produces the *shipping* build. Ship builds compile out
  the research commands and the `BP_DIAG` counters, so a diagnostic that "does
  nothing" is usually just the wrong build. The README claimed the opposite
  until 1.3.19.
