#include "PieceManager.h"

#include "Utilities.h"

#include <fstream>

using namespace std;

PieceManager::PieceManager(const Config& cfg) : cfg_(cfg) {}

bool PieceManager::initSeed(const string& filePath, const string& fileName) {
    LockGuard lock(mu_);
    seedPath_ = filePath;
    fileName_ = fileName;
    fileSz_ = Util::fileSize(filePath);
    if (fileSz_ < 0) return false;
    pieceCnt_ = Util::calcPieceCount(fileSz_, cfg_.pieceSize);
    owned_.assign(pieceCnt_, true);
    isSeed_ = true;
    return true;
}

bool PieceManager::initDownload(const string& fileName, int pieceCnt, long long fileSz) {
    LockGuard lock(mu_);
    fileName_ = fileName;
    fileSz_ = fileSz;
    pieceCnt_ = pieceCnt;
    pieceDir_ = cfg_.downloadDir + "/" + fileName + ".pieces";
    Util::ensureDir(cfg_.downloadDir);
    Util::ensureDir(pieceDir_);
    owned_.assign(pieceCnt_, false);
    isSeed_ = false;
    return true;
}

void PieceManager::setOwned(const vector<bool>& owned) {
    LockGuard lock(mu_);
    for (size_t i = 0; i < owned.size() && i < owned_.size(); ++i) {
        owned_[i] = owned[i];
    }
}

bool PieceManager::hasPiece(int idx) const {
    LockGuard lock(mu_);
    if (idx < 0 || idx >= pieceCnt_) return false;
    return owned_[idx];
}

bool PieceManager::readSeedPiece(int idx, vector<char>& data) {
    LockGuard lock(mu_);
    if (!isSeed_ || idx < 0 || idx >= pieceCnt_) return false;
    ifstream f(seedPath_, ios::binary);
    if (!f) return false;
    f.seekg((long long)idx * cfg_.pieceSize);
    data.resize(cfg_.pieceSize);
    f.read(data.data(), cfg_.pieceSize);
    size_t n = (size_t)f.gcount();
    if (n == 0) return false;
    data.resize(n);
    return true;
}

bool PieceManager::readDownloadPiece(int idx, vector<char>& data) {
    LockGuard lock(mu_);
    if (isSeed_ || idx < 0 || idx >= pieceCnt_ || !owned_[idx]) return false;
    string path = pieceDir_ + "/piece_" + to_string(idx) + ".bin";
    return Util::readFile(path, data);
}

bool PieceManager::writePiece(int idx, const vector<char>& data) {
    LockGuard lock(mu_);
    if (isSeed_ || idx < 0 || idx >= pieceCnt_) return false;
    string path = pieceDir_ + "/piece_" + to_string(idx) + ".bin";
    if (!Util::writeFile(path, data.data(), data.size())) return false;
    owned_[idx] = true;
    return true;
}

set<int> PieceManager::ownedPieces() const {
    LockGuard lock(mu_);
    set<int> out;
    for (int i = 0; i < pieceCnt_; ++i) {
        if (owned_[i]) out.insert(i);
    }
    return out;
}

int PieceManager::completeCount() const {
    LockGuard lock(mu_);
    int cnt = 0;
    for (bool b : owned_) if (b) ++cnt;
    return cnt;
}
