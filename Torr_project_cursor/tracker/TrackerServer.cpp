#include "TrackerServer.h"

#include "Config.h"
#include "Net.h"
#include "Protocol.h"

#include <iostream>

using namespace std;

TrackerServer::TrackerServer(int port)
    : port_(port),
      peerMgr_(db_),
      fileMgr_(db_),
      swarmMgr_(db_),
      heartbeat_(peerMgr_, 5, 15) {}

TrackerServer::~TrackerServer() {
    stop();
}

bool TrackerServer::start() {
    listenSock_ = Net::tcpListen(port_);
    if (listenSock_ == INVALID_SOCKET) {
        cerr << "tracker: failed to listen on port " << port_ << "\n";
        return false;
    }
    running_ = true;
    heartbeat_.start();
    acceptTh_ = Thread([this]() { acceptLoop(); });
    cout << "tracker: listening on port " << port_ << "\n";
    return true;
}

void TrackerServer::stop() {
    running_ = false;
    if (listenSock_ != INVALID_SOCKET) {
        Net::closeSock(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    heartbeat_.stop();
    if (acceptTh_.joinable()) acceptTh_.join();
}

void TrackerServer::acceptLoop() {
    while (running_) {
        string clientIp;
        sock_t client = Net::tcpAccept(listenSock_, clientIp);
        if (client == INVALID_SOCKET) {
            if (running_) sleepMs(100);
            continue;
        }
        Thread t([this, client, clientIp]() { handleClient(client, clientIp); });
        t.detach();
    }
}

void TrackerServer::handleClient(sock_t sock, const string& clientIp) {
    string line;
    if (!Net::recvLine(sock, line)) {
        Net::closeSock(sock);
        return;
    }
    auto parts = Protocol::split(line);
    if (parts.empty()) {
        Net::closeSock(sock);
        return;
    }
    string resp = routeMessage(parts, clientIp);
    Net::sendAll(sock, resp.c_str(), (int)resp.size());
    Net::closeSock(sock);
}

string TrackerServer::routeMessage(const vector<string>& parts, const string& clientIp) {
    const string& cmd = parts[0];

    if (cmd == Protocol::REGISTER_PEER) {
        if (parts.size() < 3) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return peerMgr_.registerPeer(parts[1], clientIp, stoi(parts[2]));
    }
    if (cmd == Protocol::HEARTBEAT) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return peerMgr_.heartbeat(parts[1]);
    }
    if (cmd == Protocol::GOODBYE) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return peerMgr_.removePeer(parts[1]);
    }
    if (cmd == Protocol::REGISTER_FILE) {
        return fileMgr_.registerFile(parts);
    }
    if (cmd == Protocol::UPDATE_PIECES) {
        return swarmMgr_.updatePieces(parts);
    }
    if (cmd == Protocol::SEARCH_FILE) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return fileMgr_.searchFile(parts[1]);
    }
    if (cmd == Protocol::GET_METADATA) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return fileMgr_.getMetadata(parts[1]);
    }
    if (cmd == Protocol::GET_PEERS) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return swarmMgr_.getPeerList(parts[1]);
    }
    if (cmd == Protocol::GET_AVAILABILITY) {
        if (parts.size() < 2) return Protocol::buildMsg(Protocol::ERROR_MSG, {"bad args"});
        return swarmMgr_.getAvailability(parts[1]);
    }
    if (cmd == Protocol::DOWNLOAD_COMPLETE) {
        return Protocol::buildMsg("OK", {"noted"});
    }

    return Protocol::buildMsg(Protocol::ERROR_MSG, {"unknown command"});
}
