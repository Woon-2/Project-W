#ifndef SERVER_PCH_HPP
#define SERVER_PCH_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "Ws2_32.lib")

#include "macro.hpp"

#endif // SERVER_PCH_HPP