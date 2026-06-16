#include "UploadServer.h"

#include "Net.h"
#include "Protocol.h"

using namespace std;

UploadServer::UploadServer(const Config& cfg, const string& peerId, PieceManager& pieceMgr,
                           StatsManager& stats)
    : cfg_(cfg), peerId_(peerId), pieceMgr_(pieceMgr), stats_(stats) {}

bool UploadServer::start() {
    listenSock_ = Net::tcpListen(cfg_.peerPort);
    if (listenSock_ == INVALID_SOCKET) return false;
    running_ = true;
    for (int i = 0; i < cfg_.maxUploadThreads; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
    acceptTh_ = Thread([this]() { acceptLoop(); });
    return true;
}

void UploadServer::stop() {
    running_ = false;
    if (listenSock_ != INVALID_SOCKET) {
        Net::closeSock(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    qCv_.notify_all();
    if (acceptTh_.joinable()) acceptTh_.join();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void UploadServer::acceptLoop() {
    while (running_) {
        string ip;
        sock_t client = Net::tcpAccept(listenSock_, ip);
        if (client == INVALID_SOCKET) {
            if (running_) sleepMs(50);
            continue;
        }
        {
            LockGuard lock(qMu_);
            pending_.push(client);
        }
        qCv_.notify_one();
    }
}

void UploadServer::workerLoop() {
    while (running_) {
        sock_t client = INVALID_SOCKET;
        {
            UniqueLock lock(qMu_);
            qCv_.wait(lock, [this]() { return !pending_.empty() || !running_; });
            if (!running_ && pending_.empty()) return;
            if (pending_.empty()) continue;
            client = pending_.front();
            pending_.pop();
        }
        handleClient(client);
    }
}

void UploadServer::handleClient(sock_t sock) {
    while (running_) {
        string line;
        if (!Net::recvLine(sock, line)) break;
        auto parts = Protocol::split(line);
        if (parts.empty()) break;

        if (parts[0] == Protocol::HELLO) {
            continue;
        }

        if (parts[0] == Protocol::REQUEST_PIECE && parts.size() >= 3) {
            const string& fileName = parts[1];
            int idx = stoi(parts[2]);
            vector<char> data;
            bool ok = false;
            if (pieceMgr_.fileName() == fileName) {
                if (pieceMgr_.isSeed()) {
                    ok = pieceMgr_.readSeedPiece(idx, data);
                } else if (pieceMgr_.hasPiece(idx)) {
                    ok = pieceMgr_.readDownloadPiece(idx, data);
                }
            }
            if (!ok) {
                auto resp = Protocol::buildMsg(Protocol::PIECE_NOT_FOUND, {fileName, to_string(idx)});
                Net::sendAll(sock, resp.c_str(), (int)resp.size());
                continue;
            }
            auto header = Protocol::buildMsg(Protocol::SEND_PIECE,
                                             {fileName, to_string(idx), to_string(data.size())});
            Net::sendAll(sock, header.c_str(), (int)header.size());
            Net::sendAll(sock, data.data(), (int)data.size());
            stats_.addUpload((int)data.size());
            continue;
        }
        break;
    }
    Net::closeSock(sock);
}
