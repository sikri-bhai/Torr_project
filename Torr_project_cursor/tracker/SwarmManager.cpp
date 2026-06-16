#include "SwarmManager.h"

#include "Protocol.h"

#include <sstream>

using namespace std;

SwarmManager::SwarmManager(TrackerDatabase& db) : db_(db) {}

string SwarmManager::updatePieces(const vector<string>& parts) {
    if (parts.size() < 4) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad UPDATE_PIECES"});
    }
    LockGuard lock(mu_);
    const string& peerId = parts[1];
    const string& fileName = parts[2];
    auto indices = Protocol::parseIntCsv(parts[3]);

    auto fit = db_.files.find(fileName);
    if (fit == db_.files.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"file not found"});
    }

    set<int> owned(indices.begin(), indices.end());
    fit->second.peerPieces[peerId] = owned;
    return Protocol::buildMsg("OK", {"pieces updated"});
}

string SwarmManager::getPeerList(const string& fileName) {
    LockGuard lock(mu_);
    auto fit = db_.files.find(fileName);
    if (fit == db_.files.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"file not found"});
    }

    ostringstream out;
    for (const auto& kv : fit->second.peerPieces) {
        const string& peerId = kv.first;
        auto pit = db_.peers.find(peerId);
        if (pit == db_.peers.end()) continue;
        const auto& peer = pit->second;
        out << Protocol::buildMsg(Protocol::PEER_LIST,
                                  {peerId, peer.ipAddress, to_string(peer.port),
                                   Protocol::joinInts(kv.second)});
    }
    if (out.str().empty()) {
        return Protocol::buildMsg("OK", {"no peers"});
    }
    return out.str();
}

string SwarmManager::getAvailability(const string& fileName) {
    LockGuard lock(mu_);
    auto fit = db_.files.find(fileName);
    if (fit == db_.files.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"file not found"});
    }

    int pieceCnt = fit->second.metadata.pieceCount;
    vector<int> counts(pieceCnt, 0);
    for (const auto& kv : fit->second.peerPieces) {
        for (int idx : kv.second) {
            if (idx >= 0 && idx < pieceCnt) counts[idx]++;
        }
    }

    ostringstream csv;
    for (int i = 0; i < pieceCnt; ++i) {
        if (i > 0) csv << ',';
        csv << i << ':' << counts[i];
    }
    return Protocol::buildMsg(Protocol::AVAILABILITY, {fileName, csv.str()});
}
