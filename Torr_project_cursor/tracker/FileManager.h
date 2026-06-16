#pragma once

#include "Threading.h"
#include "Types.h"

#include <string>
#include <vector>

using namespace std;

class FileManager {
public:
    explicit FileManager(TrackerDatabase& db);

    string registerFile(const vector<string>& parts);
    string searchFile(const string& fileName);
    string getMetadata(const string& fileName);

private:
    TrackerDatabase& db_;
    Mutex mu_;
};
