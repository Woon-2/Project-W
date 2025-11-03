#include "pch.hpp"
#include "global.hpp"
#include "object.hpp"

SPServerSession gServerSession = nullptr;

std::shared_ptr<class Object> gPlayer = nullptr;
std::unordered_map<i32t, std::shared_ptr<class Object>> gObjects;
std::mutex gMtx;