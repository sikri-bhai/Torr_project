#pragma once

#include <fstream>
#include <string>
#include <vector>

using namespace std;

namespace Util {

vector<string> splitPath(const string& path);
string fileNameFromPath(const string& path);
bool ensureDir(const string& dir);
bool readFile(const string& path, vector<char>& data);
bool writeFile(const string& path, const char* data, size_t sz);
bool atomicWrite(const string& path, const string& content);
long long fileSize(const string& path);
int calcPieceCount(long long fileSz, int pieceSz);
string trim(const string& s);

}  // namespace Util
