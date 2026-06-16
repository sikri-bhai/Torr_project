#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace Util {

std::vector<std::string> splitPath(const std::string& path);
std::string fileNameFromPath(const std::string& path);
bool ensureDir(const std::string& dir);
bool readFile(const std::string& path, std::vector<char>& data);
bool writeFile(const std::string& path, const char* data, size_t sz);
bool atomicWrite(const std::string& path, const std::string& content);
long long fileSize(const std::string& path);
int calcPieceCount(long long fileSz, int pieceSz);
std::string trim(const std::string& s);

}  // namespace Util
