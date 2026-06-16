#include "HashManager.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

static uint64_t fnv64(const char* data, size_t sz, uint64_t seed) {
    uint64_t h = seed;
    for (size_t i = 0; i < sz; ++i) {
        h ^= (uint64_t)(unsigned char)data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static string toHex16(uint64_t v) {
    ostringstream oss;
    oss << hex << nouppercase << setfill('0') << setw(16) << v;
    return oss.str();
}

string HashManager::hashPiece(const char* data, size_t sz) {
    uint64_t h1 = fnv64(data, sz, 0xcbf29ce484222325ULL);
    uint64_t h2 = fnv64(data, sz, 0x84222325cbf29ce4ULL);
    uint64_t h3 = fnv64(data, sz, h1 ^ h2);
    uint64_t h4 = fnv64(data, sz, h3 ^ 0xdeadbeefcafebabeULL);
    return toHex16(h1) + toHex16(h2) + toHex16(h3) + toHex16(h4);
}

bool HashManager::verify(const vector<char>& data, const string& expectedHex) {
    return hashPiece(data.data(), data.size()) == expectedHex;
}

vector<string> HashManager::hashFile(const string& path, int pieceSz) {
    vector<string> hashes;
    ifstream f(path, ios::binary);
    if (!f) return hashes;

    vector<char> buf(pieceSz);
    while (true) {
        f.read(buf.data(), pieceSz);
        streamsize n = f.gcount();
        if (n <= 0) break;
        hashes.push_back(hashPiece(buf.data(), (size_t)n));
        if (n < pieceSz) break;
    }
    return hashes;
}
