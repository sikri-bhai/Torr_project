#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET sock_t;
#else
typedef int sock_t;
#endif

namespace Net {

bool initWinsock();
void cleanupWinsock();

sock_t tcpListen(int port);
sock_t tcpAccept(sock_t listenSock, std::string& clientIp);
sock_t tcpConnect(const std::string& ip, int port, int timeoutSec);

bool sendAll(sock_t sock, const char* buf, int sz);
bool recvLine(sock_t sock, std::string& line);
bool recvExact(sock_t sock, char* buf, int sz);

void closeSock(sock_t sock);
std::string getPeerIp(sock_t sock);

}  // namespace Net
