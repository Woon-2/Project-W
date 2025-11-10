#ifndef TYPES_HPP
#define TYPES_HPP

#include "concurrentqueue.h"

#include <cstdint>
#include <memory>

using int8 = std::int8_t;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;

using SPIocpCore = std::shared_ptr<class IocpCore>;
using SPIocpObject = std::shared_ptr<class IocpObject>;
using SPListener = std::shared_ptr<class Listener>;
using SPSession = std::shared_ptr<class Session>;
using SPGameSession = std::shared_ptr<class GameSession>;
using SPPacketSession = std::shared_ptr<class PacketSession>;
using SPClientService = std::shared_ptr<class ClientService>;
using SPServerService = std::shared_ptr<class ServerService>;
using SPSendBuffer = std::shared_ptr<class SendBuffer>;
using SPRoom = std::shared_ptr<class Room>;

template<class T>
using CCQueue = moodycamel::ConcurrentQueue<T>;

#endif // TYPES_HPP