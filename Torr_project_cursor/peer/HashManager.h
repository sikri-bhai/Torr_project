#pragma once

#include <string>
#include <vector>

using namespace std;

// Simple custom piece hash (replace with SHA-256 later).
class HashManager {
public:
    static string hashPiece(const char* data, size_t sz);
    static bool verify(const vector<char>& data, const string& expectedHex);
    static vector<string> hashFile(const string& path, int pieceSz);
};
