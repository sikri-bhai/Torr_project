#pragma once

#include "Threading.h"
#include "Types.h"

#include <random>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;

class Scheduler {
public:
    explicit Scheduler(int pieceCnt);

    void setAvailability(const unordered_map<int, int>& avail);
    void markComplete(int idx);
    void markMissing(int idx);
    void markRequested(int idx);
    bool pickPiece(int& idx, const set<int>& peerHas);
    bool allComplete() const;
    int completeCount() const;
    void initFromOwned(const vector<bool>& owned);

private:
    int pieceCnt_;
    mutable Mutex mu_;
    vector<PieceState> state_;
    unordered_map<int, int> avail_;
    mt19937 rng_{random_device{}()};
};
