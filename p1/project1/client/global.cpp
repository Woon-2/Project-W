#include "pch.hpp"
#include "global.hpp"
#include "object.hpp"

SPServerSession gServerSession = nullptr;
std::unordered_map<i32t, std::shared_ptr<class Object>> gObjects;