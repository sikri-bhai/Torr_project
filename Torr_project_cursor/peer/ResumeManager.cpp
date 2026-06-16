#include "ResumeManager.h"

#include "Utilities.h"

#include <fstream>

using namespace std;

ResumeManager::ResumeManager(const Config& cfg) : cfg_(cfg) {}

string ResumeManager::statePath(const string& fileName) const {
    return cfg_.downloadDir + "/" + fileName + ".state";
}

bool ResumeManager::loadState(const string& fileName, vector<bool>& owned) {
    LockGuard lock(mu_);
    string path = statePath(fileName);
    ifstream f(path);
    if (!f) return false;
    string bits;
    f >> bits;
    if (bits.size() != owned.size()) return false;
    for (size_t i = 0; i < bits.size(); ++i) {
        owned[i] = (bits[i] == '1');
    }
    return true;
}

bool ResumeManager::saveState(const string& fileName, const vector<bool>& owned) {
    LockGuard lock(mu_);
    string bits;
    for (bool b : owned) bits += b ? '1' : '0';
    return Util::atomicWrite(statePath(fileName), bits);
}
