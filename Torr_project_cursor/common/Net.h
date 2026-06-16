#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET sock_t;
#else
typedef int sock_t;
#endif

using namespace std;

namespace Net {

bool initWinsock();
void cleanupWinsock();

sock_t tcpListen(int port);
sock_t tcpAccept(sock_t listenSock, string& clientIp);
sock_t tcpConnect(const string& ip, int port, int timeoutSec);

bool sendAll(sock_t sock, const char* buf, int sz);
bool recvLine(sock_t sock, string& line);
bool recvExact(sock_t sock, char* buf, int sz);

void closeSock(sock_t sock);
string getPeerIp(sock_t sock);

}  // namespace Net
