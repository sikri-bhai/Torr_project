#include "HeartbeatMonitor.h"

#include "Threading.h"

using namespace std;

HeartbeatMonitor::HeartbeatMonitor(PeerManager& peerMgr, int intervalSec, int maxAgeSec)
    : peerMgr_(peerMgr), intervalSec_(intervalSec), maxAgeSec_(maxAgeSec) {}

void HeartbeatMonitor::start() {
    running_ = true;
    th_ = Thread([this]() {
        while (running_) {
            peerMgr_.removeStalePeers(maxAgeSec_);
            for (int i = 0; i < intervalSec_ && running_; ++i) {
                sleepSec(1);
            }
        }
    });
}

void HeartbeatMonitor::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();
}
