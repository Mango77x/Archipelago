#include "replay.h"

#include <fstream>
#include <iostream>
#include <string>

namespace archipelago {

namespace {
constexpr const char* kReplayFile = "archipelago_replay.txt";
constexpr const char* kReplayHeader = "ARCHIPELAGO_REPLAY_V1";
}  // namespace

void SaveReplay(const std::vector<ReplayFrame>& frames) {
    std::ofstream out(kReplayFile);
    if (!out) {
        std::cerr << "SaveReplay: could not open " << kReplayFile << " for writing\n";
        return;
    }
    out << kReplayHeader << "\n";
    out << frames.size() << "\n";
    for (const ReplayFrame& f : frames) {
        out << (f.thrustForward ? 1 : 0) << " " << (f.thrustBackward ? 1 : 0) << " " << (f.turnLeft ? 1 : 0) << " "
            << (f.turnRight ? 1 : 0) << " " << f.buyAction << "\n";
    }
    std::cout << "Replay saved to " << kReplayFile << " (" << frames.size() << " frames)\n";
}

bool LoadReplay(std::vector<ReplayFrame>& frames) {
    std::ifstream in(kReplayFile);
    if (!in) {
        std::cerr << "LoadReplay: could not open " << kReplayFile << "\n";
        return false;
    }
    std::string header;
    std::getline(in, header);
    if (header != kReplayHeader) {
        std::cerr << "LoadReplay: unrecognized replay file header\n";
        return false;
    }
    size_t count = 0;
    in >> count;
    frames.clear();
    frames.reserve(count);
    for (size_t i = 0; i < count && in; ++i) {
        int fwd, back, left, right, buy;
        in >> fwd >> back >> left >> right >> buy;
        ReplayFrame f;
        f.thrustForward = fwd != 0;
        f.thrustBackward = back != 0;
        f.turnLeft = left != 0;
        f.turnRight = right != 0;
        f.buyAction = buy;
        frames.push_back(f);
    }
    if (!in || frames.size() != count) {
        std::cerr << "LoadReplay: replay file is truncated or malformed\n";
        return false;
    }
    std::cout << "Replay loaded from " << kReplayFile << " (" << frames.size() << " frames)\n";
    return true;
}

}  // namespace archipelago
