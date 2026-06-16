#pragma once

#include "Config.h"
#include "Threading.h"
#include "Types.h"

#include <atomic>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;

class TrackerClient {
public:
    TrackerClient(const Config& cfg);

    bool start();
    void stop();
    bool registerPeer();
    bool registerFile(const string& fileName, long long fileSz, int pieceCnt,
                      const vector<string>& hashes);
    bool updatePieces(const string& fileName, const set<int>& pieces);
    bool searchFile(const string& fileName, FileInfo& out);
    bool getMetadata(const string& fileName, FileInfo& out);
    bool getPeers(const string& fileName, vector<SwarmPeer>& out);
    bool getAvailability(const string& fileName, unordered_map<int, int>& out);
    bool notifyComplete(const string& fileName);
    void goodbye();

private:
    string sendCommand(const string& msg);
    void heartbeatLoop();

    const Config& cfg_;
    atomic<bool> running_{false};
    Thread hbTh_;
};
