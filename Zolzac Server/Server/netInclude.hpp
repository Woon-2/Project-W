#ifndef __NET_INCLUDE_HPP
#define __NET_INCLUDE_HPP

#include "protocol.hpp"

#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <cstdint>
#include <array>

#pragma comment(lib, "WS2_32.LIB")
#pragma comment(lib, "MSWSock.LIB")

void errorDisplay( std::string_view, int );

#endif	// __NET_INCLUDE_HPP