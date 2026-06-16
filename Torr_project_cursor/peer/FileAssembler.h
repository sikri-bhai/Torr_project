#pragma once

#include "Config.h"

#include <string>

using namespace std;

class FileAssembler {
public:
    explicit FileAssembler(const Config& cfg);

    bool assemble(const string& fileName, int pieceCnt, long long expectedSz);
    string outputPath(const string& fileName) const;

private:
    const Config& cfg_;
};
