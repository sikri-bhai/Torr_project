#pragma once

#include <ctime>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct PeerInfo {
    std::string peerId;
    std::string ipAddress;
    int port = 0;
    std::time_t lastHeartbeat = 0;
};

struct FileInfo {
    std::string fileName;
    long long fileSize = 0;
    int pieceCount = 0;
    std::vector<std::string> pieceHashes;
};

struct TrackerRecord {
    FileInfo metadata;
    std::unordered_map<std::string, std::set<int>> peerPieces;
};

struct TrackerDatabase {
    std::unordered_map<std::string, PeerInfo> peers;
    std::unordered_map<std::string, TrackerRecord> files;
};

enum class PieceState {
    MISSING,
    REQUESTED,
    COMPLETE
};

struct SwarmPeer {
    std::string peerId;
    std::string ip;
    int port = 0;
    std::set<int> pieces;
};
