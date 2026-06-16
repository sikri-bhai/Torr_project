#include "Scheduler.h"

using namespace std;

Scheduler::Scheduler(int pieceCnt) : pieceCnt_(pieceCnt), state_(pieceCnt, PieceState::MISSING) {}

void Scheduler::setAvailability(const unordered_map<int, int>& avail) {
    LockGuard lock(mu_);
    avail_ = avail;
}

void Scheduler::markComplete(int idx) {
    LockGuard lock(mu_);
    if (idx >= 0 && idx < pieceCnt_) state_[idx] = PieceState::COMPLETE;
}

void Scheduler::markMissing(int idx) {
    LockGuard lock(mu_);
    if (idx >= 0 && idx < pieceCnt_) state_[idx] = PieceState::MISSING;
}

void Scheduler::markRequested(int idx) {
    LockGuard lock(mu_);
    if (idx >= 0 && idx < pieceCnt_) state_[idx] = PieceState::REQUESTED;
}

bool Scheduler::pickPiece(int& idx, const set<int>& peerHas) {
    LockGuard lock(mu_);
    int bestCnt = 999999;
    vector<int> candidates;

    for (int i = 0; i < pieceCnt_; ++i) {
        if (state_[i] != PieceState::MISSING) continue;
        if (peerHas.find(i) == peerHas.end()) continue;
        int cnt = 1;
        auto it = avail_.find(i);
        if (it != avail_.end()) cnt = it->second;
        if (cnt < bestCnt) {
            bestCnt = cnt;
            candidates.clear();
            candidates.push_back(i);
        } else if (cnt == bestCnt) {
            candidates.push_back(i);
        }
    }

    if (candidates.empty()) return false;
    idx = candidates[rng_() % candidates.size()];
    state_[idx] = PieceState::REQUESTED;
    return true;
}

bool Scheduler::allComplete() const {
    LockGuard lock(mu_);
    for (auto s : state_) {
        if (s != PieceState::COMPLETE) return false;
    }
    return true;
}

int Scheduler::completeCount() const {
    LockGuard lock(mu_);
    int cnt = 0;
    for (auto s : state_) if (s == PieceState::COMPLETE) ++cnt;
    return cnt;
}

void Scheduler::initFromOwned(const vector<bool>& owned) {
    LockGuard lock(mu_);
    for (size_t i = 0; i < owned.size() && i < (size_t)pieceCnt_; ++i) {
        state_[i] = owned[i] ? PieceState::COMPLETE : PieceState::MISSING;
    }
}
