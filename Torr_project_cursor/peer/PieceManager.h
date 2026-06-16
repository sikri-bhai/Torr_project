#pragma once

#include "Config.h"
#include "Threading.h"

#include <set>
#include <string>
#include <vector>

using namespace std;

class PieceManager {
public:
    PieceManager(const Config& cfg);

    bool initSeed(const string& filePath, const string& fileName);
    bool initDownload(const string& fileName, int pieceCnt, long long fileSz);
    void setOwned(const vector<bool>& owned);
    bool hasPiece(int idx) const;
    bool readSeedPiece(int idx, vector<char>& data);
    bool readDownloadPiece(int idx, vector<char>& data);
    bool writePiece(int idx, const vector<char>& data);
    set<int> ownedPieces() const;
    int completeCount() const;
    int pieceCount() const { return pieceCnt_; }
    long long fileSize() const { return fileSz_; }
    const string& fileName() const { return fileName_; }
    const string& seedPath() const { return seedPath_; }
    bool isSeed() const { return isSeed_; }

private:
    const Config& cfg_;
    string fileName_;
    string seedPath_;
    string pieceDir_;
    long long fileSz_ = 0;
    int pieceCnt_ = 0;
    bool isSeed_ = false;
    mutable Mutex mu_;
    vector<bool> owned_;
};
