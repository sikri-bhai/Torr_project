#pragma once

#include <string>

struct Config {
    int pieceSize = 1048576;
    int heartbeatInterval = 5;
    int dashboardRefresh = 1;
    int maxUploadThreads = 10;
    int connectTimeout = 5;
    int trackerPort = 8080;
    int peerPort = 5001;
    std::string trackerHost = "127.0.0.1";
    std::string peerId = "peer1";
    std::string sharedDir = "shared";
    std::string downloadDir = "downloads";
};
