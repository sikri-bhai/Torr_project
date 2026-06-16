#include "TrackerServer.h"

#include "Net.h"

#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc >= 2) port = stoi(argv[1]);

    if (!Net::initWinsock()) {
        cerr << "winsock init failed\n";
        return 1;
    }

    TrackerServer server(port);
    if (!server.start()) {
        Net::cleanupWinsock();
        return 1;
    }

    cout << "press enter to stop tracker\n";
    cin.get();

    server.stop();
    Net::cleanupWinsock();
    return 0;
}
