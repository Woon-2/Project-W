#ifndef SERVER_PCH_HPP
#define SERVER_PCH_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include "windows.hpp"

#include "macro.hpp"
#include "NetAddress.hpp"
#include "SocketUtils.hpp"

#include "protocol.hpp"

#include <iostream>
#include <fstream>
#include <system_error>
#include <string>
#include <vector>
#include <array>
#include <set>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#endif // SERVER_PCH_HPP