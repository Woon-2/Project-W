#include "pch.hpp"
#include "global.hpp"
#include "online/onlineGame.hpp"
#include "ServerSession.hpp"
#include "object.hpp"

GFX gGfx{};

std::unique_ptr<IGame> pGame = nullptr;
std::atomic_bool gReady = false;

std::shared_ptr<Object> gPlayer = nullptr;
std::unordered_map<i32t, std::shared_ptr<Object>> gObjects;
std::mutex gMtx;