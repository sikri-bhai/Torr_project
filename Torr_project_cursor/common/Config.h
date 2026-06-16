#pragma once

#include <string>

using namespace std;

struct Config {
    int pieceSize = 1048576;
    int heartbeatInterval = 5;
    int dashboardRefresh = 1;
    int maxUploadThreads = 10;
    int connectTimeout = 5;
    int trackerPort = 8080;
    int peerPort = 5001;
    string trackerHost = "127.0.0.1";
    string peerId = "peer1";
    string sharedDir = "shared";
    string downloadDir = "downloads";
};
