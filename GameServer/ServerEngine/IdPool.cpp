#include "sepch.hpp"
#include "IdPool.hpp"

ccqueue<uint32> IdPool::pool_;
ccqueue<uint32> RoomIdPool::pool_;
std::atomic_int32_t RoomIdPool::currId_{-1};
