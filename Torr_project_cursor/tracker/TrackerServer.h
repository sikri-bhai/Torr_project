#pragma once

#include "FileManager.h"
#include "HeartbeatMonitor.h"
#include "Net.h"
#include "PeerManager.h"
#include "SwarmManager.h"
#include "Threading.h"
#include "Types.h"

#include <atomic>
#include <string>
#include <vector>

using namespace std;

class TrackerServer {
public:
    explicit TrackerServer(int port);
    ~TrackerServer();

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(sock_t sock, const string& clientIp);
    string routeMessage(const vector<string>& parts, const string& clientIp);

    int port_;
    sock_t listenSock_ = INVALID_SOCKET;
    TrackerDatabase db_;
    PeerManager peerMgr_;
    FileManager fileMgr_;
    SwarmManager swarmMgr_;
    HeartbeatMonitor heartbeat_;
    atomic<bool> running_{false};
    Thread acceptTh_;
};
