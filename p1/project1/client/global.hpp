#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include "online/onlineGame.hpp"

extern std::unique_ptr<class IGame> pGame;
extern std::atomic_bool gReady;

// 일단은 현재 네트워크 쓰레드에서 move 메시지를 받기 위한 큐
extern moodycamel::ConcurrentQueue<struct Online::Message> messageQueue;

extern std::mutex gMtx;

#endif // GLOBAL_HPP