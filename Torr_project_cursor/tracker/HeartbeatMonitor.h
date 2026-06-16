#pragma once

#include "PeerManager.h"
#include "Threading.h"

#include <atomic>

using namespace std;

class HeartbeatMonitor {
public:
    HeartbeatMonitor(PeerManager& peerMgr, int intervalSec, int maxAgeSec);
    void start();
    void stop();

private:
    PeerManager& peerMgr_;
    int intervalSec_;
    int maxAgeSec_;
    atomic<bool> running_{false};
    Thread th_;
};
