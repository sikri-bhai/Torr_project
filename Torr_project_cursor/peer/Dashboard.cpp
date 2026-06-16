#include "Dashboard.h"

#include <iomanip>
#include <iostream>

using namespace std;

Dashboard::Dashboard(const Config& cfg, StatsManager& stats, DownloadManager& dlMgr)
    : cfg_(cfg), stats_(stats), dlMgr_(dlMgr) {}

void Dashboard::start() {
    running_ = true;
    th_ = Thread([this]() { loop(); });
}

void Dashboard::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();
}

void Dashboard::printOnce() const {
    auto st = dlMgr_.status();
    cout << "====================================\n";
    cout << "File:\n" << (st.fileName.empty() ? "(none)" : st.fileName) << "\n\n";
    cout << "Progress:\n" << st.progressPct << "%\n\n";
    cout << "Pieces:\n" << st.piecesDone << " / " << st.piecesTotal << "\n\n";
    cout << "Connected Peers:\n" << st.connectedPeers << "\n\n";
    cout << "Download Speed:\n" << fixed << setprecision(1) << stats_.downloadSpeedMbps()
         << " MB/s\n\n";
    cout << "Upload Speed:\n" << fixed << setprecision(1) << stats_.uploadSpeedMbps() << " MB/s\n";
    cout << "====================================\n";
}

void Dashboard::loop() {
    while (running_) {
        stats_.tick();
        if (dlMgr_.status().active) {
            printOnce();
        }
        for (int i = 0; i < cfg_.dashboardRefresh && running_; ++i) {
            sleepSec(1);
        }
    }
}
