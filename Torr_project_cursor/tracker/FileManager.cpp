#include "FileManager.h"

#include "Protocol.h"

#include <sstream>

using namespace std;

FileManager::FileManager(TrackerDatabase& db) : db_(db) {}

string FileManager::registerFile(const vector<string>& parts) {
    if (parts.size() < 6) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad REGISTER_FILE"});
    }
    LockGuard lock(mu_);
    const string& peerId = parts[1];
    const string& fileName = parts[2];
    long long fileSz = stoll(parts[3]);
    int pieceCnt = stoi(parts[4]);
    const string& hashCsv = parts[5];

    FileInfo info;
    info.fileName = fileName;
    info.fileSize = fileSz;
    info.pieceCount = pieceCnt;

    istringstream iss(hashCsv);
    string tok;
    while (getline(iss, tok, ',')) {
        if (!tok.empty()) info.pieceHashes.push_back(tok);
    }

    if ((int)info.pieceHashes.size() != pieceCnt) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"hash count mismatch"});
    }

    auto it = db_.files.find(fileName);
    if (it == db_.files.end()) {
        TrackerRecord rec;
        rec.metadata = info;
        db_.files[fileName] = rec;
    } else {
        db_.files[fileName].metadata = info;
    }

    vector<int> all;
    for (int i = 0; i < pieceCnt; ++i) all.push_back(i);
    db_.files[fileName].peerPieces[peerId] = set<int>(all.begin(), all.end());

    return Protocol::buildMsg("OK", {"file registered"});
}

string FileManager::searchFile(const string& fileName) {
    LockGuard lock(mu_);
    auto it = db_.files.find(fileName);
    if (it == db_.files.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"file not found"});
    }
    const auto& m = it->second.metadata;
    return Protocol::buildMsg(Protocol::FILE_FOUND,
                              {m.fileName, to_string(m.fileSize), to_string(m.pieceCount)});
}

string FileManager::getMetadata(const string& fileName) {
    LockGuard lock(mu_);
    auto it = db_.files.find(fileName);
    if (it == db_.files.end()) {
        return Protocol::buildMsg(Protocol::ERROR_MSG, {"file not found"});
    }
    const auto& m = it->second.metadata;
    ostringstream hashes;
    for (size_t i = 0; i < m.pieceHashes.size(); ++i) {
        if (i > 0) hashes << ',';
        hashes << m.pieceHashes[i];
    }
    return Protocol::buildMsg(Protocol::FILE_METADATA,
                              {m.fileName, to_string(m.fileSize), to_string(m.pieceCount),
                               hashes.str()});
}
