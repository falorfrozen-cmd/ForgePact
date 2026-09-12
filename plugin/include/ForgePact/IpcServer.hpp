#pragma once

#include "Common.hpp"

namespace ForgePact {

// The `bp_ipc\cmd.txt` file poll: the ForgePact panel (and, in research
// builds, manual testing) writes one command per line; this drains it every
// ~6 runtime frames and forwards each line to RunCommand().
//
// Deliberately NOT a reimplementation of the dispatch table: RunCommand is a
// ~60-verb router touching almost every feature in this file (Headhunter,
// EstForce, custom item forge, debug logging, and more, on top of every
// class already split out) - the same kind of single shared chokepoint that
// kept DoMultiCreate and Hook_DropRelic in ModuleMain.cpp. This class proxies
// every real command to it unchanged, rather than re-listing a subset here
// that would drift from the real command set over time.
class IpcServer {
public:
    static IpcServer& Instance() {
        static IpcServer s_Instance;
        return s_Instance;
    }

    // Reads bp_ipc\cmd.txt (if present and non-empty), deletes it immediately
    // so a slow command can't be reprocessed on the next poll, then runs each
    // non-empty line through the real command dispatcher in turn.
    void PollCommands() {
        std::string cmdPath = CmdPath();
        std::ifstream in(cmdPath, std::ios::binary);
        if (!in.good()) return;
        std::stringstream ss; ss << in.rdbuf();
        std::string content = ss.str();
        in.close();
        if (content.empty()) return;

        // delete immediately so we don't reprocess
        DeleteFileA(cmdPath.c_str());

        Out("---- running command file ----");
        std::stringstream ls(content);
        std::string line;
        while (std::getline(ls, line)) {
            if (line.empty()) continue;
            try { RunCommand(line); }
            catch (...) { Out("command threw: " + line); }
        }
        Out("---- done ----");
    }

private:
    IpcServer() = default;
};

} // namespace ForgePact
