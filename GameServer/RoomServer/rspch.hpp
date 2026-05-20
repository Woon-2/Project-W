#ifndef room_server_pch_hpp
#define room_server_pch_hpp

#ifdef _DEBUG
#pragma comment(lib, "Debug/ServerEngine.lib")
#pragma comment(lib, "Debug/lua54.lib")
#else
#pragma comment(lib, "Release/ServerEngine.lib")
#pragma comment(lib, "Release/lua54.lib")
#endif

#include "sepch.hpp"

#include <fstream>
#include <filesystem>
#include <variant>
#include <random>

#include "log.hpp"
#include "errorHandling.hpp"

using XMFLOAT2 = DirectX::XMFLOAT2;
using XMFLOAT3 = DirectX::XMFLOAT3;
using XMFLOAT4 = DirectX::XMFLOAT4;
using XMUINT2 = DirectX::XMUINT2;
using XMUINT3 = DirectX::XMUINT3;
using XMUINT4 = DirectX::XMUINT4;
using XMINT2 = DirectX::XMINT2;
using XMINT3 = DirectX::XMINT3;
using XMINT4 = DirectX::XMINT4;
using XMFLOAT3X3 = DirectX::XMFLOAT3X3;
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;

#endif // room_server_pch_hpp