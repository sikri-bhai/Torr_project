#include "TrackerClient.h"

#include "Net.h"
#include "Protocol.h"

#include <iostream>
#include <sstream>

using namespace std;

TrackerClient::TrackerClient(const Config& cfg) : cfg_(cfg) {}

bool TrackerClient::start() {
    if (!registerPeer()) return false;
    running_ = true;
    hbTh_ = Thread([this]() { heartbeatLoop(); });
    return true;
}

void TrackerClient::stop() {
    running_ = false;
    if (hbTh_.joinable()) hbTh_.join();
}

string TrackerClient::sendCommand(const string& msg) {
    sock_t sock = Net::tcpConnect(cfg_.trackerHost, cfg_.trackerPort, cfg_.connectTimeout);
    if (sock == INVALID_SOCKET) return "";
    if (!Net::sendAll(sock, msg.c_str(), (int)msg.size())) {
        Net::closeSock(sock);
        return "";
    }
    string line;
    if (!Net::recvLine(sock, line)) {
        Net::closeSock(sock);
        return "";
    }
    Net::closeSock(sock);
    return line;
}

bool TrackerClient::registerPeer() {
    auto msg = Protocol::buildMsg(Protocol::REGISTER_PEER, {cfg_.peerId, to_string(cfg_.peerPort)});
    string resp = sendCommand(msg);
    return !resp.empty() && resp.rfind("OK", 0) == 0;
}

void TrackerClient::heartbeatLoop() {
    while (running_) {
        auto msg = Protocol::buildMsg(Protocol::HEARTBEAT, {cfg_.peerId});
        sendCommand(msg);
        for (int i = 0; i < cfg_.heartbeatInterval && running_; ++i) {
            sleepSec(1);
        }
    }
}

bool TrackerClient::registerFile(const string& fileName, long long fileSz, int pieceCnt,
                                 const vector<string>& hashes) {
    ostringstream hashCsv;
    for (size_t i = 0; i < hashes.size(); ++i) {
        if (i > 0) hashCsv << ',';
        hashCsv << hashes[i];
    }
    auto msg = Protocol::buildMsg(Protocol::REGISTER_FILE,
                                  {cfg_.peerId, fileName, to_string(fileSz), to_string(pieceCnt),
                                   hashCsv.str()});
    string resp = sendCommand(msg);
    return !resp.empty() && resp.rfind("OK", 0) == 0;
}

bool TrackerClient::updatePieces(const string& fileName, const set<int>& pieces) {
    auto msg = Protocol::buildMsg(Protocol::UPDATE_PIECES,
                                  {cfg_.peerId, fileName, Protocol::joinInts(pieces)});
    string resp = sendCommand(msg);
    return !resp.empty() && resp.rfind("OK", 0) == 0;
}

bool TrackerClient::searchFile(const string& fileName, FileInfo& out) {
    auto msg = Protocol::buildMsg(Protocol::SEARCH_FILE, {fileName});
    string resp = sendCommand(msg);
    auto parts = Protocol::split(resp);
    if (parts.empty() || parts[0] != Protocol::FILE_FOUND) return false;
    if (parts.size() < 4) return false;
    out.fileName = parts[1];
    out.fileSize = stoll(parts[2]);
    out.pieceCount = stoi(parts[3]);
    return true;
}

bool TrackerClient::getMetadata(const string& fileName, FileInfo& out) {
    auto msg = Protocol::buildMsg(Protocol::GET_METADATA, {fileName});
    string resp = sendCommand(msg);
    auto parts = Protocol::split(resp);
    if (parts.empty() || parts[0] != Protocol::FILE_METADATA) return false;
    if (parts.size() < 5) return false;
    out.fileName = parts[1];
    out.fileSize = stoll(parts[2]);
    out.pieceCount = stoi(parts[3]);
    istringstream iss(parts[4]);
    string tok;
    while (getline(iss, tok, ',')) {
        if (!tok.empty()) out.pieceHashes.push_back(tok);
    }
    return true;
}

bool TrackerClient::getPeers(const string& fileName, vector<SwarmPeer>& out) {
    auto msg = Protocol::buildMsg(Protocol::GET_PEERS, {fileName});
    string resp = sendCommand(msg);
    out.clear();
    istringstream iss(resp);
    string line;
    while (getline(iss, line)) {
        if (line.empty()) continue;
        auto parts = Protocol::split(line);
        if (parts.size() < 5 || parts[0] != Protocol::PEER_LIST) continue;
        SwarmPeer p;
        p.peerId = parts[1];
        p.ip = parts[2];
        p.port = stoi(parts[3]);
        auto idxs = Protocol::parseIntCsv(parts[4]);
        p.pieces = set<int>(idxs.begin(), idxs.end());
        if (p.peerId != cfg_.peerId) out.push_back(p);
    }
    return !out.empty();
}

bool TrackerClient::getAvailability(const string& fileName, unordered_map<int, int>& out) {
    auto msg = Protocol::buildMsg(Protocol::GET_AVAILABILITY, {fileName});
    string resp = sendCommand(msg);
    auto parts = Protocol::split(Protocol::split(resp, '\n')[0]);
    if (parts.empty() || parts[0] != Protocol::AVAILABILITY) return false;
    if (parts.size() < 3) return false;
    out.clear();
    istringstream iss(parts[2]);
    string tok;
    while (getline(iss, tok, ',')) {
        auto colon = tok.find(':');
        if (colon == string::npos) continue;
        int idx = stoi(tok.substr(0, colon));
        int cnt = stoi(tok.substr(colon + 1));
        out[idx] = cnt;
    }
    return true;
}

bool TrackerClient::notifyComplete(const string& fileName) {
    auto msg = Protocol::buildMsg(Protocol::DOWNLOAD_COMPLETE, {cfg_.peerId, fileName});
    string resp = sendCommand(msg);
    return !resp.empty();
}

void TrackerClient::goodbye() {
    auto msg = Protocol::buildMsg(Protocol::GOODBYE, {cfg_.peerId});
    sendCommand(msg);
}
