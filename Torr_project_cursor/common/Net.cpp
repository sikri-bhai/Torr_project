#include "Net.h"

#include <ws2tcpip.h>

using namespace std;

namespace Net {

bool initWinsock() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void cleanupWinsock() {
    WSACleanup();
}

sock_t tcpListen(int port) {
    sock_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closeSock(sock);
        return INVALID_SOCKET;
    }
    if (listen(sock, SOMAXCONN) != 0) {
        closeSock(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

sock_t tcpAccept(sock_t listenSock, string& clientIp) {
    sockaddr_in addr{};
    int len = sizeof(addr);
    sock_t client = accept(listenSock, (sockaddr*)&addr, &len);
    if (client == INVALID_SOCKET) return INVALID_SOCKET;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    clientIp = ip;
    return client;
}

sock_t tcpConnect(const string& ip, int port, int timeoutSec) {
    sock_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    connect(sock, (sockaddr*)&addr, sizeof(addr));

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    timeval tv{};
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;

    int sel = select(0, nullptr, &wfds, nullptr, &tv);
    if (sel <= 0) {
        closeSock(sock);
        return INVALID_SOCKET;
    }

    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
    return sock;
}

bool sendAll(sock_t sock, const char* buf, int sz) {
    int sent = 0;
    while (sent < sz) {
        int n = send(sock, buf + sent, sz - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool recvLine(sock_t sock, string& line) {
    line.clear();
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') break;
        if (c != '\r') line += c;
    }
    return true;
}

bool recvExact(sock_t sock, char* buf, int sz) {
    int got = 0;
    while (got < sz) {
        int n = recv(sock, buf + got, sz - got, 0);
        if (n <= 0) return false;
        got += n;
    }
    return true;
}

void closeSock(sock_t sock) {
    if (sock != INVALID_SOCKET) closesocket(sock);
}

string getPeerIp(sock_t sock) {
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getpeername(sock, (sockaddr*)&addr, &len) != 0) return "";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return ip;
}

}  // namespace Net
