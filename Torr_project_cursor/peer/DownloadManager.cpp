#include "DownloadManager.h"

#include "HashManager.h"
#include "Net.h"
#include "Protocol.h"
#include "Utilities.h"

#include <iostream>

using namespace std;

DownloadManager::DownloadManager(const Config& cfg, TrackerClient& tracker, PieceManager& pieceMgr,
                                 ResumeManager& resume, StatsManager& stats)
    : cfg_(cfg),
      tracker_(tracker),
      pieceMgr_(pieceMgr),
      resume_(resume),
      stats_(stats),
      assembler_(cfg) {}

bool DownloadManager::startDownload(const string& fileName) {
    if (dlActive_) {
        cerr << "download already active\n";
        return false;
    }

    FileInfo meta;
    if (!tracker_.searchFile(fileName, meta)) {
        cerr << "file not found on tracker\n";
        return false;
    }
    if (!tracker_.getMetadata(fileName, meta)) {
        cerr << "failed to get metadata\n";
        return false;
    }

    hashes_ = meta.pieceHashes;
    if (!pieceMgr_.initDownload(fileName, meta.pieceCount, meta.fileSize)) {
        cerr << "piece manager init failed\n";
        return false;
    }

    sched_ = make_unique<Scheduler>(meta.pieceCount);
    vector<bool> owned(meta.pieceCount, false);
    if (resume_.loadState(fileName, owned)) {
        pieceMgr_.setOwned(owned);
        sched_->initFromOwned(owned);
    }

    if (!tracker_.getPeers(fileName, peers_)) {
        cerr << "no peers available\n";
        return false;
    }

    refreshAvailability();

    {
        LockGuard lock(statusMu_);
        status_.fileName = fileName;
        status_.piecesTotal = meta.pieceCount;
        status_.piecesDone = sched_->completeCount();
        status_.progressPct =
            meta.pieceCount > 0 ? (status_.piecesDone * 100 / meta.pieceCount) : 0;
        status_.connectedPeers = (int)peers_.size();
        status_.active = true;
    }

    running_ = true;
    dlActive_ = true;

    for (const auto& peer : peers_) {
        workers_.emplace_back([this, peer]() { workerLoop(peer); });
    }

    syncTh_ = Thread([this]() {
        while (running_) {
            syncTracker();
            {
                LockGuard lock(statusMu_);
                if (sched_) {
                    status_.piecesDone = sched_->completeCount();
                    status_.progressPct = status_.piecesTotal > 0
                                              ? (status_.piecesDone * 100 / status_.piecesTotal)
                                              : 0;
                }
            }
            if (sched_ && sched_->allComplete()) {
                running_ = false;
                break;
            }
            for (int i = 0; i < cfg_.heartbeatInterval && running_; ++i) {
                sleepSec(1);
            }
        }
    });

    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    if (syncTh_.joinable()) syncTh_.join();

    if (sched_ && sched_->allComplete()) {
        syncState();
        if (assembler_.assemble(fileName, meta.pieceCount, meta.fileSize)) {
            cout << "download complete: " << assembler_.outputPath(fileName) << "\n";
            tracker_.notifyComplete(fileName);
        } else {
            cerr << "assembly failed\n";
        }
    }

    dlActive_ = false;
    {
        LockGuard lock(statusMu_);
        status_.active = false;
    }
    workers_.clear();
    sched_.reset();
    return true;
}

void DownloadManager::stop() {
    running_ = false;
}

DownloadStatus DownloadManager::status() const {
    LockGuard lock(statusMu_);
    return status_;
}

void DownloadManager::refreshAvailability() {
    unordered_map<int, int> avail;
    if (tracker_.getAvailability(pieceMgr_.fileName(), avail)) {
        sched_->setAvailability(avail);
    }
}

void DownloadManager::syncState() {
    auto owned = pieceMgr_.ownedPieces();
    vector<bool> bits(pieceMgr_.pieceCount(), false);
    for (int i : owned) bits[i] = true;
    resume_.saveState(pieceMgr_.fileName(), bits);
}

void DownloadManager::syncTracker() {
    auto owned = pieceMgr_.ownedPieces();
    if (!owned.empty()) {
        tracker_.updatePieces(pieceMgr_.fileName(), owned);
    }
    refreshAvailability();
}

bool DownloadManager::requestPiece(sock_t sock, int idx, vector<char>& data) {
    auto req = Protocol::buildMsg(Protocol::REQUEST_PIECE, {pieceMgr_.fileName(), to_string(idx)});
    if (!Net::sendAll(sock, req.c_str(), (int)req.size())) return false;

    string line;
    if (!Net::recvLine(sock, line)) return false;
    auto parts = Protocol::split(line);
    if (parts.empty()) return false;

    if (parts[0] == Protocol::PIECE_NOT_FOUND) return false;

    if (parts[0] == Protocol::SEND_PIECE && parts.size() >= 4) {
        int payloadSz = stoi(parts[3]);
        data.resize(payloadSz);
        if (!Net::recvExact(sock, data.data(), payloadSz)) return false;
        stats_.addDownload(payloadSz);
        return true;
    }
    return false;
}

void DownloadManager::workerLoop(SwarmPeer peer) {
    sock_t sock = Net::tcpConnect(peer.ip, peer.port, cfg_.connectTimeout);
    if (sock == INVALID_SOCKET) return;

    auto hello = Protocol::buildMsg(Protocol::HELLO, {cfg_.peerId});
    Net::sendAll(sock, hello.c_str(), (int)hello.size());

    while (running_ && sched_ && !sched_->allComplete()) {
        int idx = -1;
        if (!sched_->pickPiece(idx, peer.pieces)) {
            sleepMs(200);
            refreshAvailability();
            continue;
        }

        vector<char> data;
        if (!requestPiece(sock, idx, data)) {
            sched_->markMissing(idx);
            Net::closeSock(sock);
            return;
        }

        if (idx < 0 || idx >= (int)hashes_.size()) {
            sched_->markMissing(idx);
            continue;
        }

        if (!HashManager::verify(data, hashes_[idx])) {
            sched_->markMissing(idx);
            continue;
        }

        if (!pieceMgr_.writePiece(idx, data)) {
            sched_->markMissing(idx);
            continue;
        }

        sched_->markComplete(idx);
        syncState();
    }

    Net::closeSock(sock);
}
