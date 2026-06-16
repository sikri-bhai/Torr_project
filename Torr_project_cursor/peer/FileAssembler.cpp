#include "FileAssembler.h"

#include "Utilities.h"

#include <fstream>
#include <iostream>

using namespace std;

FileAssembler::FileAssembler(const Config& cfg) : cfg_(cfg) {}

string FileAssembler::outputPath(const string& fileName) const {
    return cfg_.downloadDir + "/" + fileName;
}

bool FileAssembler::assemble(const string& fileName, int pieceCnt, long long expectedSz) {
    string outPath = outputPath(fileName);
    string pieceDir = cfg_.downloadDir + "/" + fileName;
    ofstream out(outPath, ios::binary | ios::trunc);
    if (!out) return false;

    for (int i = 0; i < pieceCnt; ++i) {
        string piecePath = pieceDir + "/piece_" + to_string(i) + ".bin";
        vector<char> data;
        if (!Util::readFile(piecePath, data)) return false;
        out.write(data.data(), (streamsize)data.size());
    }
    out.flush();
    if (!out.good()) return false;

    long long sz = Util::fileSize(outPath);
    if (sz != expectedSz) {
        cerr << "assemble: size mismatch expected " << expectedSz << " got " << sz << "\n";
        return false;
    }
    return true;
}
