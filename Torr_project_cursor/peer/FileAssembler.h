#pragma once

#include "Config.h"

#include <string>

class FileAssembler {
public:
    explicit FileAssembler(const Config& cfg);

    bool assemble(const std::string& fileName, int pieceCnt, long long expectedSz);
    std::string outputPath(const std::string& fileName) const;

private:
    const Config& cfg_;
};
