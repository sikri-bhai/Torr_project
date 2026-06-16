#include "Peer.h"

#include "Net.h"
#include "Utilities.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

static void printHelp() {
    cout << "commands:\n";
    cout << "  share <path>       share a file from shared/\n";
    cout << "  download <name>    download a file by name\n";
    cout << "  status             show metrics\n";
    cout << "  quit               exit\n";
}

static Config parseArgs(int argc, char* argv[], string& sharePath, string& downloadName,
                        bool& runRepl) {
    Config cfg;
    sharePath.clear();
    downloadName.clear();
    runRepl = true;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) {
            cfg.peerId = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            cfg.peerPort = stoi(argv[++i]);
        } else if (arg == "--tracker" && i + 1 < argc) {
            string hostPort = argv[++i];
            auto colon = hostPort.find(':');
            if (colon != string::npos) {
                cfg.trackerHost = hostPort.substr(0, colon);
                cfg.trackerPort = stoi(hostPort.substr(colon + 1));
            } else {
                cfg.trackerHost = hostPort;
            }
        } else if (arg == "--share" && i + 1 < argc) {
            sharePath = argv[++i];
            runRepl = false;
        } else if (arg == "--download" && i + 1 < argc) {
            downloadName = argv[++i];
            runRepl = false;
        } else if (arg == "--help") {
            cout << "usage: peer [--id ID] [--port PORT] [--tracker HOST:PORT]\n";
            cout << "            [--share PATH] [--download NAME]\n";
            exit(0);
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    string sharePath;
    string downloadName;
    bool runRepl = true;
    Config cfg = parseArgs(argc, argv, sharePath, downloadName, runRepl);

    if (!Net::initWinsock()) {
        cerr << "winsock init failed\n";
        return 1;
    }

    Peer peer(cfg);
    if (!peer.start()) {
        Net::cleanupWinsock();
        return 1;
    }

    if (!sharePath.empty()) {
        peer.shareFile(sharePath);
        cout << "press enter to quit\n";
        cin.get();
        peer.stop();
        Net::cleanupWinsock();
        return 0;
    }

    if (!downloadName.empty()) {
        peer.downloadFile(downloadName);
        peer.waitForDownload();
        cout << "press enter to quit\n";
        cin.get();
        peer.stop();
        Net::cleanupWinsock();
        return 0;
    }

    printHelp();
    string line;
    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break;
        line = Util::trim(line);
        if (line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "help") {
            printHelp();
            continue;
        }
        if (cmd == "status") {
            peer.printStatus();
            continue;
        }
        if (cmd == "share") {
            string path;
            iss >> path;
            if (path.empty()) {
                cout << "usage: share <path>\n";
                continue;
            }
            peer.shareFile(path);
            continue;
        }
        if (cmd == "download") {
            string name;
            iss >> name;
            if (name.empty()) {
                cout << "usage: download <filename>\n";
                continue;
            }
            peer.downloadFile(name);
            continue;
        }
        cout << "unknown command. type help\n";
    }

    peer.stop();
    Net::cleanupWinsock();
    return 0;
}
