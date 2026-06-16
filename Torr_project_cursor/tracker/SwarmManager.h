#pragma once

#include "Threading.h"
#include "Types.h"

#include <string>
#include <vector>

using namespace std;

class SwarmManager {
public:
    explicit SwarmManager(TrackerDatabase& db);

    string updatePieces(const vector<string>& parts);
    string getPeerList(const string& fileName);
    string getAvailability(const string& fileName);

private:
    TrackerDatabase& db_;
    Mutex mu_;
};
