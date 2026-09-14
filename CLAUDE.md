# CLAUDE.md

ForgePact is normally developed as a submodule of the
[hero-siege-offline-toolkit](https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit)
hub, and the rules for working on it live in that hub, not here:

- **`../agents.md`** — repository-wide agent guidelines. Read first. Among other
  things: the hard rule that decompiled or disassembled game source never enters
  a tracked file (including commit messages and PR text), and the requirement to
  update documentation as part of the change rather than after it.
- **`../docs/submodules/ForgePact/instructions.md`** — this module's guide:
  architecture, the IPC contract, the change workflow, packaging guardrails, and
  the Known Limitations log of approaches already tried and ruled out.

If you are in a standalone ForgePact checkout those paths do not exist; clone the
hub alongside this repository, which you need anyway — `hs-game-sdk/` is a build
dependency of both the plugin and the packager.

## Two things that are easy to get wrong here

- **A player-visible change needs a `release-notes-vX.Y.Z.md` file.** The module
  guide treats a shipped version without one as an incomplete change. Player
  language, symptom before fix, and never claim a fix that is not real.
- **`plugin_build\build.bat dev` needs the literal `dev`.** Any other argument,
  including none at all, produces the shipping build. Ship builds compile out the
  diagnostic commands and the `BP_DIAG` counters, so a research command that
  "does nothing" is usually just the wrong build.
