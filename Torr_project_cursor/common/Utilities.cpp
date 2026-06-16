#include "Utilities.h"

#include <direct.h>
#include <errno.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <fstream>

using namespace std;

namespace Util {

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

string fileNameFromPath(const string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos) return path;
    return path.substr(pos + 1);
}

bool ensureDir(const string& dir) {
    return _mkdir(dir.c_str()) == 0 || errno == EEXIST;
}

bool readFile(const string& path, vector<char>& data) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f) return false;
    auto sz = f.tellg();
    if (sz < 0) return false;
    f.seekg(0);
    data.resize((size_t)sz);
    if (!f.read(data.data(), sz)) return false;
    return true;
}

bool writeFile(const string& path, const char* data, size_t sz) {
    ofstream f(path, ios::binary | ios::trunc);
    if (!f) return false;
    f.write(data, (streamsize)sz);
    return f.good();
}

bool atomicWrite(const string& path, const string& content) {
    string tmp = path + ".tmp";
    {
        ofstream f(tmp, ios::trunc);
        if (!f) return false;
        f << content;
        if (!f.good()) return false;
    }
    if (remove(path.c_str()) != 0 && errno != ENOENT) {
    }
    return rename(tmp.c_str(), path.c_str()) == 0;
}

long long fileSize(const string& path) {
    struct _stat st;
    if (_stat(path.c_str(), &st) != 0) return -1;
    return st.st_size;
}

int calcPieceCount(long long fileSz, int pieceSz) {
    if (fileSz <= 0) return 0;
    return (int)((fileSz + pieceSz - 1) / pieceSz);
}

}  // namespace Util
