#pragma once

#include <ctime>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct PeerInfo {
    string peerId;
    string ipAddress;
    int port = 0;
    time_t lastHeartbeat = 0;
};

struct FileInfo {
    string fileName;
    long long fileSize = 0;
    int pieceCount = 0;
    vector<string> pieceHashes;
};

struct TrackerRecord {
    FileInfo metadata;
    unordered_map<string, set<int>> peerPieces;
};

struct TrackerDatabase {
    unordered_map<string, PeerInfo> peers;
    unordered_map<string, TrackerRecord> files;
};

enum class PieceState {
    MISSING,
    REQUESTED,
    COMPLETE
};

struct SwarmPeer {
    string peerId;
    string ip;
    int port = 0;
    set<int> pieces;
};
