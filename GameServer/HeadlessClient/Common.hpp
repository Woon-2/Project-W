#ifndef headless_common_hpp
#define headless_common_hpp

// Winsock 헤더는 windows.h 보다 먼저 와야 한다. NOMINMAX/LEAN 으로 매크로 충돌 방지.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// 서버/클라이언트와 동일한 패킷 정의를 그대로 재사용한다(어긋나면 안 됨).
#include "protocol.hpp"

#include <chrono>

namespace hc {

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = SteadyClock::time_point;

// 봇 1개의 수명 상태머신.
enum class BotState {
	Disconnected,
	Connecting,    // non-blocking connect 진행 중
	Connected,     // TCP 연결 완료(직후 C_Enter 송신)
	EnteringRoom,  // C_Enter 송신 후 S_Enter 대기
	InRoom,        // S_Enter 수신, 이후 20Hz C_Move 송신
	Closed,
};

inline bool isLive(BotState s) {
	return s == BotState::Connected || s == BotState::EnteringRoom || s == BotState::InRoom;
}

} // namespace hc

#endif // headless_common_hpp
