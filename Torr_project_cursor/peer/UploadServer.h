#pragma once

#include "Config.h"
#include "Net.h"
#include "PieceManager.h"
#include "StatsManager.h"
#include "Threading.h"

#include <atomic>
#include <queue>

using namespace std;

class UploadServer {
public:
    UploadServer(const Config& cfg, const string& peerId, PieceManager& pieceMgr, StatsManager& stats);

    bool start();
    void stop();

private:
    void acceptLoop();
    void workerLoop();
    void handleClient(sock_t sock);

    const Config& cfg_;
    string peerId_;
    PieceManager& pieceMgr_;
    StatsManager& stats_;
    sock_t listenSock_ = INVALID_SOCKET;
    atomic<bool> running_{false};
    Thread acceptTh_;
    vector<Thread> workers_;
    Mutex qMu_;
    CondVar qCv_;
    queue<sock_t> pending_;
};
