#include "Peer.h"

#include "HashManager.h"
#include "Utilities.h"

#include <iostream>

using namespace std;

Peer::Peer(Config cfg)
    : cfg_(move(cfg)),
      pieceMgr_(cfg_),
      resume_(cfg_),
      tracker_(cfg_),
      upload_(cfg_, cfg_.peerId, pieceMgr_, stats_),
      dlMgr_(cfg_, tracker_, pieceMgr_, resume_, stats_),
      dashboard_(cfg_, stats_, dlMgr_) {}

bool Peer::start() {
    Util::ensureDir(cfg_.sharedDir);
    Util::ensureDir(cfg_.downloadDir);

    if (!upload_.start()) {
        cerr << "upload server failed\n";
        return false;
    }
    if (!tracker_.start()) {
        cerr << "tracker registration failed\n";
        upload_.stop();
        return false;
    }
    dashboard_.start();
    started_ = true;
    cout << "peer " << cfg_.peerId << " ready on port " << cfg_.peerPort << "\n";
    return true;
}

void Peer::stop() {
    if (!started_) return;
    dlMgr_.stop();
    if (dlTh_.joinable()) dlTh_.join();
    tracker_.goodbye();
    dashboard_.stop();
    tracker_.stop();
    upload_.stop();
    started_ = false;
}

bool Peer::shareFile(const string& path) {
    string name = Util::fileNameFromPath(path);
    long long sz = Util::fileSize(path);
    if (sz < 0) {
        cerr << "cannot read file: " << path << "\n";
        return false;
    }

    auto hashes = HashManager::hashFile(path, cfg_.pieceSize);
    int pieceCnt = Util::calcPieceCount(sz, cfg_.pieceSize);
    if ((int)hashes.size() != pieceCnt) {
        cerr << "hashing failed\n";
        return false;
    }

    if (!pieceMgr_.initSeed(path, name)) {
        cerr << "piece manager seed init failed\n";
        return false;
    }

    if (!tracker_.registerFile(name, sz, pieceCnt, hashes)) {
        cerr << "tracker register failed\n";
        return false;
    }

    auto owned = pieceMgr_.ownedPieces();
    tracker_.updatePieces(name, owned);
    cout << "sharing " << name << " (" << pieceCnt << " pieces)\n";
    return true;
}

bool Peer::downloadFile(const string& fileName) {
    if (dlTh_.joinable()) {
        cerr << "wait for current download to finish\n";
        return false;
    }
    dlTh_ = Thread([this, fileName]() {
        if (!dlMgr_.startDownload(fileName)) {
            cerr << "download failed: " << fileName << "\n";
        }
    });
    return true;
}

void Peer::waitForDownload() {
    if (dlTh_.joinable()) dlTh_.join();
}

void Peer::printStatus() const {
    dashboard_.printOnce();
}
