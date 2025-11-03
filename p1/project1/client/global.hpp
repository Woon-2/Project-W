#ifndef GLOBAL_HPP
#define GLOBAL_HPP

using SPServerSession = std::shared_ptr<class ServerSession>;
extern SPServerSession gServerSession;

extern std::unordered_map<int, std::shared_ptr<class Object>> gObjects;
extern std::mutex gMtx;

#endif // GLOBAL_HPP