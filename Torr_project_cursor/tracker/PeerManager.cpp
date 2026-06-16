#include "PeerManager.h"

#include "Protocol.h"

#include <ctime>
#include <vector>

using namespace std;

PeerManager::PeerManager(TrackerDatabase& db) : db_(db) {}

string PeerManager::registerPeer(const string& peerId, const string& ip, int port) {
    LockGuard lock(mu_);
    PeerInfo info;
    info.peerId = peerId;
    info.ipAddress = ip;
    info.port = port;
    info.lastHeartbeat = time(nullptr);
    db_.peers[peerId] = info;
    return Protocol::buildMsg("OK", {"registered"});
}

string PeerManager::heartbeat(const string& peerId) {
    LockGuard lock(mu_);
    auto it = db_.peers.find(peerId);
    if (it == db_.peers.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"unknown peer"});
    }
    it->second.lastHeartbeat = time(nullptr);
    return Protocol::buildMsg("OK", {"heartbeat"});
}

string PeerManager::removePeer(const string& peerId) {
    LockGuard lock(mu_);
    db_.peers.erase(peerId);
    for (auto& kv : db_.files) {
        kv.second.peerPieces.erase(peerId);
    }
    return Protocol::buildMsg("OK", {"goodbye"});
}

void PeerManager::removeStalePeers(int maxAgeSec) {
    LockGuard lock(mu_);
    auto now = time(nullptr);
    vector<string> stale;
    for (const auto& kv : db_.peers) {
        if (now - kv.second.lastHeartbeat > maxAgeSec) {
            stale.push_back(kv.first);
        }
    }
    for (const auto& id : stale) {
        db_.peers.erase(id);
        for (auto& kv : db_.files) {
            kv.second.peerPieces.erase(id);
        }
    }
}
