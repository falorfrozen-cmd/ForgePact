"""Source contract for the coop receive thread's owner.

tests/test_coop_thread_exit_behavior.py shows the thread owner's shapes at the
game's exit. This pins what that test cannot see: that ModuleMain.cpp owns the
thread through the header it compiles, that coopstop's wait stays bounded,
that the plugin declares no other std::thread a static destructor could still
find joinable at exit, and that the player build cannot start the thread at
all (checked 2026-09-26: coopstart is not a player command, and the only other
way to start it, coop.ini's auto-start, is research-only, as is the per-frame
send)."""
import re
import unittest
from pathlib import Path

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
INCLUDE = ROOT / "plugin" / "include" / "ForgePact"
HEADER = INCLUDE / "ExitSafeThread.hpp"

#: Every std::thread (or std::jthread) object the plugin declares by name, and
#: why none can be joinable when a static destructor reaches it. A new one
#: fails the test until it is owned by ForgePact::ExitSafeThread or listed here
#: with its reason.
NAMED_THREADS = {
    ("ItemTruth.hpp", "thread_"):
        "a Journal member; ModuleMain.cpp creates its only Journal with new and never deletes it "
        "(test_item_truth_contract's test_the_journal_lives_on_the_heap_for_the_whole_process)",
}

NAMED_THREAD = re.compile(r"\bstd::j?thread\s+(\w+)\s*[;={(]")
AUTO_THREAD = re.compile(r"\bauto\s+(\w+)\s*=\s*std::j?thread\s*[({]")
THREAD_ALIAS = re.compile(r"\b(?:using\s+\w+\s*=\s*|typedef\s+)std::j?thread\b")

COOP_COMMANDS = ("coopstart", "coopstop", "coopstats", "cooprender", "coopobj", "coopclear")


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


class CoopThreadExitContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = read(PLUGIN)
        cls.plugin = strip_comments(plugin)
        cls.player = strip_comments(strip_research_blocks(plugin))

    def player_commands(self):
        start = self.player.index("kPlayerCommands = {")
        return re.findall(r'"([^"]+)"', self.player[start:self.player.index("};", start)])

    def assertBefore(self, first, second, code, where):
        # Presence first, so a missing line fails with its name, not a ValueError.
        for text in (first, second):
            self.assertTrue(text in code, f"{where} lacks {text!r}")
        self.assertLess(code.index(first), code.index(second), f"{where}: {first!r} must come before {second!r}")

    def test_the_receive_thread_is_owned_by_the_exit_safe_holder(self):
        # assertTrue, not assertIn: a miss would print all of ModuleMain.cpp.
        for text in ("#include <ForgePact/ExitSafeThread.hpp>",
                     "static ForgePact::ExitSafeThread g_CoopRecvThread;",
                     "static void CoopRecvLoop() noexcept"):
            self.assertTrue(text in self.plugin, f"ModuleMain.cpp lacks {text!r}")
        self.assertIn("g_CoopRecvThread.Start(&CoopRecvLoop)", function_body(self.plugin, "static void CoopStart("))

    def test_coopstop_closes_the_socket_then_waits_a_bounded_time(self):
        body = function_body(self.plugin, "static void CoopStop()")
        self.assertNotIn(".join()", body)
        # Closing the socket is what ends the recvfrom the thread waits in.
        self.assertBefore("closesocket(g_CoopSock)", "g_CoopRecvThread.JoinFor(kCoopStopJoinTimeout)", body,
                          "CoopStop")
        text = "static constexpr std::chrono::milliseconds kCoopStopJoinTimeout{ 2000 };"
        self.assertTrue(text in self.plugin, f"ModuleMain.cpp lacks {text!r}")

    def test_coopstart_waits_for_a_thread_coopstop_could_not_end(self):
        # That thread still reads g_CoopSock, so no new socket until it ends.
        body = function_body(self.plugin, "static void CoopStart(")
        self.assertBefore("g_CoopRecvThread.JoinFor(std::chrono::milliseconds(0))", "g_CoopSock = socket(", body,
                          "CoopStart")

    def test_the_holder_has_no_destructor(self):
        header = strip_comments(read(HEADER))
        self.assertIn("static_assert(std::is_trivially_destructible_v<ExitSafeThread>", header)
        self.assertIn("std::thread* thread_ = nullptr;", header)
        self.assertNotIn("~ExitSafeThread", header)
        # ModuleMain.cpp includes <windows.h> without NOMINMAX.
        self.assertNotRegex(header, r"std::(?:min|max)\s*\(")

    def test_every_named_thread_in_the_plugin_is_accounted_for(self):
        found = set()
        for path in [PLUGIN, *sorted(INCLUDE.glob("*.hpp"))]:
            code = strip_comments(read(path))
            self.assertIsNone(THREAD_ALIAS.search(code), f"{path.name} names std::thread under another name")
            for pattern in (NAMED_THREAD, AUTO_THREAD):
                found.update((path.name, match.group(1)) for match in pattern.finditer(code))
        self.assertEqual(set(NAMED_THREADS), found,
                         "own a module's thread with ForgePact::ExitSafeThread (plugin/include/ForgePact/"
                         "ExitSafeThread.hpp), or list it in NAMED_THREADS with why it cannot be joinable "
                         "when a static destructor reaches it")

    def test_the_scan_sees_the_old_declaration(self):
        # Positive control: the declaration this change removed is one the scan reports.
        self.assertEqual(["g_CoopRecvThread"],
                         NAMED_THREAD.findall("static sockaddr_in g_CoopPeer{};\nstatic std::thread g_CoopRecvThread;\n"))
        self.assertEqual(["worker"], AUTO_THREAD.findall("static auto worker = std::thread(Loop);"))
        self.assertIsNotNone(THREAD_ALIAS.search("using Worker = std::thread;"))
        self.assertEqual([], NAMED_THREAD.findall("thread_ = new std::thread(routine);\nstd::thread* thread_ = nullptr;"))

    def test_the_player_build_cannot_start_the_receive_thread(self):
        commands = self.player_commands()
        for command in COOP_COMMANDS:
            self.assertNotIn(command, commands)
        # coop.ini's auto-start and the per-frame send are research-only.
        frame = function_body(self.player, "void FrameCallback(FWFrame& FrameContext)")
        self.assertNotIn("LoadCoopConfigAndMaybeStart", frame)
        self.assertNotIn("CoopTick", frame)
        # Positive control: the research build keeps both.
        research_frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertIn("LoadCoopConfigAndMaybeStart();", research_frame)
        self.assertIn("CoopTick();", research_frame)


if __name__ == "__main__":
    unittest.main()
