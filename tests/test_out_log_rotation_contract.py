#!/usr/bin/env python3
"""Source-contract tests for the bp_ipc log rotation (out.txt / itemdrops.jsonl).

Nothing ever trimmed either file: out.txt is append-only in both builds and
one player's copy reached 7.8 MB; the research-only itemdrops.jsonl reached
52 MB on the same machine. These pin the fix at the source level - the
rotation runs once, at load, before the boot banner; it moves the file
instead of truncating it; and only the itemdrops.jsonl half is research-only,
since the player build never writes that file at all (BP_LOGDROP is a no-op
there, see plugin/ModuleMain.cpp's own comment beside the macro).

No native harness here (see AGENTS.md "Limit Rebuilds & Reruns" / the
map-reveal and prospect-window harnesses for the pattern that would apply):
unlike ProspectWindowMod.hpp's core, the rotation helpers are not
game-independent by contract - they call GetModuleFileNameA (via IPC_DIR),
fs::exists/fs::file_size and MoveFileExW directly against real files, so a
harness would need to fake the Win32 file APIs the same way the map-reveal
harness fakes distance_to_object, which is a larger lift than a rotation fix
justifies. The two real `plugin_build\\build.bat` runs (dev and release) are
the compile-time check that the source actually builds as intended.
"""

import re
import sys
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"
MODMANAGER_PATH = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "ModManager.hpp"


def strip_research_blocks(source: str) -> str:
    """What the player (release) build compiles, i.e. with FORGEPACT_RELEASE
    defined. Mirrors test_release_hook_contract.py's helper of the same name:
    `#ifndef FORGEPACT_RELEASE ... #endif` blocks are dropped, `#ifdef
    FORGEPACT_RELEASE ... #else ... #endif` keeps its `#ifdef` half.
    Unrelated conditionals keep both halves; nesting is tracked so an inner
    `#if` cannot end an outer FORGEPACT_RELEASE block early.
    """
    kept = []
    stack = []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif all(active for _, active in stack):
            kept.append(line)
    return "\n".join(kept)


class OutLogRotationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin_source = PLUGIN_PATH.read_text(encoding="utf-8")
        cls.modmanager_source = MODMANAGER_PATH.read_text(encoding="utf-8")
        cls.plugin_release = strip_research_blocks(cls.plugin_source)

    def test_rotation_helpers_exist_with_their_own_thresholds(self):
        self.assertIn("RotateOutLogIfNeeded", self.plugin_source)
        self.assertIn("kOutLogRotateBytes", self.plugin_source)
        self.assertIn("RotateItemDropsLogIfNeeded", self.plugin_source)
        self.assertIn("kItemDropsRotateBytes", self.plugin_source)

    def test_out_log_rotation_is_compiled_in_both_builds(self):
        # The player build must carry the exact same out.txt rotation as the
        # research build - the player's copy is the one that reached 7.8 MB.
        self.assertIn("RotateOutLogIfNeeded", self.plugin_release,
                      "the out.txt rotation must survive stripping research-only code")

    def test_item_drops_rotation_is_research_only(self):
        # BP_LOGDROP compiles to a no-op in the player build, so a player's
        # copy never grows itemdrops.jsonl in the first place; its rotation
        # must not ship either.
        self.assertNotIn("RotateItemDropsLogIfNeeded", self.plugin_release,
                         "itemdrops.jsonl rotation must be stripped from the player build")
        self.assertIn("RotateItemDropsLogIfNeeded", self.plugin_source,
                      "itemdrops.jsonl rotation must still compile in the research build")

    def test_both_rotations_use_movefileexw_with_replace_existing(self):
        for name in ("RotateOutLogIfNeeded", "RotateItemDropsLogIfNeeded"):
            start = self.plugin_source.index(f"static void {name}(")
            brace = self.plugin_source.index("{", start)
            depth = 0
            end = None
            for index in range(brace, len(self.plugin_source)):
                if self.plugin_source[index] == "{":
                    depth += 1
                elif self.plugin_source[index] == "}":
                    depth -= 1
                    if depth == 0:
                        end = index
                        break
            body = self.plugin_source[brace + 1:end]
            self.assertIn("MoveFileExW(", body, f"{name} must move, not copy or delete")
            self.assertIn("MOVEFILE_REPLACE_EXISTING", body,
                          f"{name} must replace an existing .prev file rather than fail")

    def test_out_txt_is_never_truncated(self):
        # A move-that-failed must fall back to appending, never truncating -
        # losing the log to a failed rotation is worse than an oversized one.
        # Checked as the CODE TOKEN "ios::trunc" (not the bare substring
        # "trunc"), since the surrounding comment legitimately discusses
        # truncation in English prose.
        for match in re.finditer(r'OutPath\(\)\s*,\s*([^)]*)\)', self.plugin_source):
            self.assertNotIn("ios::trunc", match.group(1),
                             "out.txt must never be opened with std::ios::trunc")
        # RotateOutLogIfNeeded's own failure path re-opens the literal path in
        # append mode, not OutPath() by name, so check it directly too.
        start = self.plugin_source.index("static void RotateOutLogIfNeeded(")
        brace = self.plugin_source.index("{", start)
        depth = 0
        end = None
        for index in range(brace, len(self.plugin_source)):
            if self.plugin_source[index] == "{":
                depth += 1
            elif self.plugin_source[index] == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break
        body = self.plugin_source[brace + 1:end]
        self.assertIn("std::ios::app", body)
        self.assertNotIn("ios::trunc", body)

    def test_rotation_runs_before_the_boot_banner_and_after_the_directory_exists(self):
        init = self.modmanager_source
        create_dir = init.index("CreateDirectoryA")
        rotate_out = init.index("RotateOutLogIfNeeded()")
        banner = init.index('BloodPact plugin loaded')
        self.assertLess(create_dir, rotate_out,
                        "bp_ipc must exist before rotation looks for out.txt in it")
        self.assertLess(rotate_out, banner,
                        "rotation must run BEFORE the banner that marks this session's start")
        # The item-drops rotation call site (not just the function) must also
        # be research-only, and must also run before the banner.
        research_call = re.search(
            r"#ifndef FORGEPACT_RELEASE\s*\n\s*RotateItemDropsLogIfNeeded\(\);\s*\n#endif",
            init)
        self.assertIsNotNone(research_call,
                             "the itemdrops.jsonl rotation call site must be guarded by "
                             "#ifndef FORGEPACT_RELEASE")
        self.assertLess(research_call.start(), banner)

    def test_rotation_never_writes_ahead_of_the_directory_creation_guard(self):
        # Belt-and-suspenders on the ordering above: Initialize()'s body as a
        # whole must mention the directory creation exactly once, before any
        # rotation reference.
        body_start = self.modmanager_source.index("void Initialize()")
        body = self.modmanager_source[body_start:]
        self.assertEqual(body.count("CreateDirectoryA"), 1)


if __name__ == "__main__":
    unittest.main()
