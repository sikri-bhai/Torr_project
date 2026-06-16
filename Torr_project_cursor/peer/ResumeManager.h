#pragma once

#include "Config.h"
#include "Threading.h"

#include <string>
#include <vector>

using namespace std;

class ResumeManager {
public:
    explicit ResumeManager(const Config& cfg);

    string statePath(const string& fileName) const;
    bool loadState(const string& fileName, vector<bool>& owned);
    bool saveState(const string& fileName, const vector<bool>& owned);

private:
    const Config& cfg_;
    Mutex mu_;
};
