#include "pch.hpp"
#include "global.hpp"
#include "online/onlineGame.hpp"
#include "ServerSession.hpp"
#include "object.hpp"

std::unique_ptr<IGame> pGame = nullptr;
std::atomic_bool gReady = false;

std::mutex gMtx;