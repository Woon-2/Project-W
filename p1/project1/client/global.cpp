#include "pch.hpp"
#include "global.hpp"
#include "ServerSession.hpp"
#include "object.hpp"

std::unique_ptr<IGame> pGame = nullptr;
std::atomic_bool gReady = false;

moodycamel::ConcurrentQueue<Online::Message> messageQueue{};

std::mutex gMtx;