#ifndef GLOBAL_HPP
#define GLOBAL_HPP

extern std::unique_ptr<class IGame> pGame;
extern std::atomic_bool gReady;

extern std::mutex gMtx;

#endif // GLOBAL_HPP