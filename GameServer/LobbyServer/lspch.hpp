#pragma once
#ifndef lobby_server_pch_hpp
#define lobby_server_pch_hpp

#ifdef _DEBUG
#pragma comment(lib, "Debug/ServerEngine.lib")
#else
#pragma comment(lib, "Release/ServerEngine.lib")
#endif

// ServerEngine is a static library, so the ODBC imports must be resolved here in the EXE.
#pragma comment(lib, "odbc32.lib")

#include "sepch.hpp"

#endif // lobby_server_pch_hpp
