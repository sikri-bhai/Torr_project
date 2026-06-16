#pragma once

#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace Protocol {

inline const char* REGISTER_PEER = "REGISTER_PEER";
inline const char* HEARTBEAT = "HEARTBEAT";
inline const char* REGISTER_FILE = "REGISTER_FILE";
inline const char* UPDATE_PIECES = "UPDATE_PIECES";
inline const char* SEARCH_FILE = "SEARCH_FILE";
inline const char* FILE_FOUND = "FILE_FOUND";
inline const char* GET_PEERS = "GET_PEERS";
inline const char* PEER_LIST = "PEER_LIST";
inline const char* GET_METADATA = "GET_METADATA";
inline const char* FILE_METADATA = "FILE_METADATA";
inline const char* GET_AVAILABILITY = "GET_AVAILABILITY";
inline const char* AVAILABILITY = "AVAILABILITY";
inline const char* DOWNLOAD_COMPLETE = "DOWNLOAD_COMPLETE";
inline const char* GOODBYE = "GOODBYE";
inline const char* ERROR_MSG = "ERROR";

inline const char* HELLO = "HELLO";
inline const char* REQUEST_PIECE = "REQUEST_PIECE";
inline const char* SEND_PIECE = "SEND_PIECE";
inline const char* PIECE_NOT_FOUND = "PIECE_NOT_FOUND";

inline vector<string> split(const string& line, char delim = '|') {
    vector<string> parts;
    string cur;
    for (char c : line) {
        if (c == delim) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    return parts;
}

inline string joinInts(const vector<int>& vals) {
    ostringstream oss;
    for (size_t i = 0; i < vals.size(); ++i) {
        if (i > 0) oss << ',';
        oss << vals[i];
    }
    return oss.str();
}

inline string joinInts(const set<int>& vals) {
    ostringstream oss;
    bool first = true;
    for (int v : vals) {
        if (!first) oss << ',';
        oss << v;
        first = false;
    }
    return oss.str();
}

inline vector<int> parseIntCsv(const string& csv) {
    vector<int> out;
    if (csv.empty()) return out;
    istringstream iss(csv);
    string tok;
    while (getline(iss, tok, ',')) {
        if (!tok.empty()) out.push_back(stoi(tok));
    }
    return out;
}

inline string buildMsg(const string& cmd) {
    return cmd + "\n";
}

inline string buildMsg(const string& cmd, const vector<string>& fields) {
    ostringstream oss;
    oss << cmd;
    for (const auto& f : fields) oss << '|' << f;
    oss << '\n';
    return oss.str();
}

}  // namespace Protocol
