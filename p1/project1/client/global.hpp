#ifndef GLOBAL_HPP
#define GLOBAL_HPP

extern class GFX gGfx;

extern std::unique_ptr<class IGame> pGame;
extern std::atomic_bool gReady;

extern std::shared_ptr<class Object> gPlayer;
extern std::unordered_map<int, std::shared_ptr<class Object>> gObjects;
extern std::mutex gMtx;

#endif // GLOBAL_HPP