#ifndef headless_bot_session_hpp
#define headless_bot_session_hpp

#include "Common.hpp"
#include "Metrics.hpp"
#include "StressConfig.hpp"
#include "BotBehavior.hpp"

#include <string>
#include <vector>

namespace hc {

// 봇 1개 = 소켓 1개 = 상태머신. connect/recv/send/update 를 담당한다.
// 모든 소켓은 non-blocking 이며, StressRunner 의 이벤트 루프가 가독/가용을 통지한다.
class BotSession {
public:
	BotSession(int id, std::string lobbyCode, const StressConfig& cfg, Metrics& metrics);

	// non-blocking 소켓 생성 후 connect 시작. 즉시 실패 시 false.
	bool startConnect();

	// 이벤트 루프 콜백.
	void onWritable();   // connect 완료 확인 + 송신 backlog flush
	void onReadable();   // 수신 드레인 + 패킷 파싱
	void onError();      // POLLERR/POLLNVAL: 상태에 따라 connectFail/disconnect 처리
	void update(TimePoint now);  // InRoom 이면 20Hz 로 C_Move 송신

	// 정상 종료(테스트 종료 시). 통계 카운터 증가 없음.
	void shutdownBot();

	SOCKET   sock()  const { return sock_; }
	BotState state() const { return state_; }
	bool     active() const { return state_ != BotState::Closed; }
	// 송신 backlog 가 있거나 connect 대기 중이면 쓰기 가능 이벤트가 필요하다.
	bool     wantsWrite() const { return state_ == BotState::Connecting || sendPending() > 0; }

private:
	void onConnected();                          // C_Enter 큐잉
	void onEnter(const SEnterPacket* pkt);        // S_Enter 수신 → InRoom
	void handlePacket(const char* data, uint16 size, PacketType type);
	void parseInbound();

	void queueSend(const char* data, int len);
	void flush();
	size_t sendPending() const { return outBuf_.size() - outPos_; }

	void failConnect();      // 연결 수립 전 실패
	void dropConnection();   // 수립된 연결이 끊김
	void rawClose();         // 소켓만 닫고 Closed 로

	int                 id_;
	std::string         lobbyCode_;
	const StressConfig& cfg_;
	Metrics&            m_;

	SOCKET   sock_  = INVALID_SOCKET;
	BotState state_ = BotState::Disconnected;

	std::vector<char> inBuf_;
	std::vector<char> outBuf_;
	size_t            outPos_ = 0;

	BotBehavior         behavior_;
	TimePoint           nextSend_{};
	std::chrono::nanoseconds sendPeriod_{};
	float               sendDtSec_ = 0.05f;
};

} // namespace hc

#endif // headless_bot_session_hpp
