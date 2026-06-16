#pragma once

#include "Threading.h"
#include "Types.h"

#include <string>

using namespace std;

class PeerManager {
public:
    explicit PeerManager(TrackerDatabase& db);

    string registerPeer(const string& peerId, const string& ip, int port);
    string heartbeat(const string& peerId);
    string removePeer(const string& peerId);
    void removeStalePeers(int maxAgeSec);

private:
    TrackerDatabase& db_;
    Mutex mu_;
};
