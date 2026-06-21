#include "BotSession.hpp"
#include "PacketIO.hpp"

#include <cstring>

namespace hc {

BotSession::BotSession(int id, std::string lobbyCode, const StressConfig& cfg, Metrics& metrics)
	: id_(id)
	, lobbyCode_(std::move(lobbyCode))
	, cfg_(cfg)
	, m_(metrics) {
	const double hz = cfg_.movePacketHz > 0 ? cfg_.movePacketHz : 20;
	sendDtSec_  = static_cast<float>(1.0 / hz);
	sendPeriod_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::duration<double>(1.0 / hz));
}

bool BotSession::startConnect() {
	sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_ == INVALID_SOCKET) {
		failConnect();
		return false;
	}

	// non-blocking + Nagle 끄기(실제 서버 Listener 와 동일).
	u_long mode = 1;
	::ioctlsocket(sock_, FIONBIO, &mode);
	int one = 1;
	::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY,
		reinterpret_cast<const char*>(&one), sizeof(one));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port   = ::htons(cfg_.port);
	if (::inet_pton(AF_INET, cfg_.ip.c_str(), &addr.sin_addr) != 1) {
		failConnect();
		return false;
	}

	state_ = BotState::Connecting;
	const int r = ::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	if (r == 0) {
		// 루프백에서 즉시 성공하는 경우.
		state_ = BotState::Connected;
		onConnected();
		return true;
	}

	const int err = ::WSAGetLastError();
	if (err != WSAEWOULDBLOCK) {
		failConnect();
		return false;
	}
	return true;  // 진행 중: 이후 onWritable 에서 완료 처리.
}

void BotSession::onWritable() {
	if (state_ == BotState::Connecting) {
		int err = 0;
		int len = sizeof(err);
		if (::getsockopt(sock_, SOL_SOCKET, SO_ERROR,
				reinterpret_cast<char*>(&err), &len) != 0 || err != 0) {
			failConnect();
			return;
		}
		state_ = BotState::Connected;
		onConnected();
	}
	flush();
}

void BotSession::onConnected() {
	std::vector<char> pkt;
	buildEnter(pkt, lobbyCode_.c_str());
	queueSend(pkt.data(), static_cast<int>(pkt.size()));
	state_ = BotState::EnteringRoom;
}

void BotSession::onReadable() {
	if (sock_ == INVALID_SOCKET) {
		return;
	}

	char tmp[16 * 1024];
	for (;;) {
		const int n = ::recv(sock_, tmp, sizeof(tmp), 0);
		if (n > 0) {
			m_.recvBytes.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
			inBuf_.insert(inBuf_.end(), tmp, tmp + n);
			if (n < static_cast<int>(sizeof(tmp))) {
				break;  // 더 읽을 게 없을 가능성이 높음 → 다음 루프 WOULDBLOCK 회피.
			}
		} else if (n == 0) {
			dropConnection();  // 서버가 정상 종료.
			return;
		} else {
			const int err = ::WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				break;
			}
			dropConnection();
			return;
		}
	}

	parseInbound();
}

void BotSession::parseInbound() {
	size_t off = 0;
	const size_t total = inBuf_.size();

	while (total - off >= sizeof(PacketHeader)) {
		PacketHeader hdr{};
		std::memcpy(&hdr, inBuf_.data() + off, sizeof(hdr));

		if (hdr.size < sizeof(PacketHeader) || hdr.size > kMaxPacketSize) {
			// 스트림이 깨졌다고 보고 연결을 끊는다(재동기 불가).
			m_.parseError.fetch_add(1, std::memory_order_relaxed);
			dropConnection();
			return;
		}
		if (total - off < hdr.size) {
			break;  // 패킷이 아직 다 안 옴.
		}

		handlePacket(inBuf_.data() + off, hdr.size, hdr.type);
		m_.recvPackets.fetch_add(1, std::memory_order_relaxed);
		off += hdr.size;
	}

	if (off > 0) {
		inBuf_.erase(inBuf_.begin(), inBuf_.begin() + off);
	}
}

void BotSession::handlePacket(const char* data, uint16 size, PacketType type) {
	switch (type) {
	case PacketType::S_Enter:
		if (state_ == BotState::EnteringRoom && size >= sizeof(SEnterPacket)) {
			onEnter(reinterpret_cast<const SEnterPacket*>(data));
		}
		break;
	case PacketType::S_NpcMoveBatch:
		// 후속 FPS 측정 신호: Room 당 매 tick 1회 브로드캐스트됨.
		m_.npcBatchRecv.fetch_add(1, std::memory_order_relaxed);
		break;
	default:
		// 그 외 패킷은 MVP 에서 통계(recvPackets/Bytes)만 반영.
		break;
	}
}

void BotSession::onEnter(const SEnterPacket* pkt) {
	// 입장 성공. 스폰 위치를 원형 이동 중심으로 삼는다.
	const DirectX::XMFLOAT3 center = pkt->myInfo.pos;
	const float phase = static_cast<float>(id_) * 0.6180339887f;  // 봇마다 다른 위상(황금각 근사).
	behavior_.init(center, cfg_.moveRadius, cfg_.moveAngularSpeed, phase);

	state_    = BotState::InRoom;
	nextSend_ = SteadyClock::now();  // 곧바로 첫 C_Move 송신.
}

void BotSession::update(TimePoint now) {
	if (state_ != BotState::InRoom) {
		return;
	}
	if (now < nextSend_) {
		return;
	}

	const MoveSample mv = behavior_.step(sendDtSec_);
	std::vector<char> pkt;
	buildMove(pkt, mv.pos, mv.velocity);
	queueSend(pkt.data(), static_cast<int>(pkt.size()));

	nextSend_ += sendPeriod_;
	if (nextSend_ < now) {
		// 크게 밀렸으면 몰아치기 대신 현재 기준으로 재설정.
		nextSend_ = now + sendPeriod_;
	}
}

void BotSession::queueSend(const char* data, int len) {
	outBuf_.insert(outBuf_.end(), data, data + len);
	// "발생시킨 부하" 기준으로 카운트(루프백에서 실제 전송과 거의 동일).
	m_.sendPackets.fetch_add(1, std::memory_order_relaxed);
	m_.sendBytes.fetch_add(static_cast<uint64_t>(len), std::memory_order_relaxed);
	flush();
}

void BotSession::flush() {
	if (sock_ == INVALID_SOCKET) {
		return;
	}

	while (outPos_ < outBuf_.size()) {
		const int n = ::send(sock_, outBuf_.data() + outPos_,
			static_cast<int>(outBuf_.size() - outPos_), 0);
		if (n > 0) {
			outPos_ += static_cast<size_t>(n);
		} else {
			const int err = ::WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				break;  // 커널 송신버퍼 가득 → backlog 유지, 다음 onWritable 에서 재시도.
			}
			dropConnection();
			return;
		}
	}

	if (outPos_ >= outBuf_.size()) {
		outBuf_.clear();
		outPos_ = 0;
	} else {
		// 소비한 prefix 제거(보통 backlog 0 이라 비용 없음).
		outBuf_.erase(outBuf_.begin(), outBuf_.begin() + outPos_);
		outPos_ = 0;
		m_.noteSendQueue(outBuf_.size());
	}
}

void BotSession::onError() {
	if (state_ == BotState::Connecting) {
		failConnect();
	} else {
		dropConnection();
	}
}

void BotSession::failConnect() {
	m_.connectFail.fetch_add(1, std::memory_order_relaxed);
	rawClose();
}

void BotSession::dropConnection() {
	if (state_ != BotState::Closed) {
		m_.disconnect.fetch_add(1, std::memory_order_relaxed);
	}
	rawClose();
}

void BotSession::rawClose() {
	if (sock_ != INVALID_SOCKET) {
		::closesocket(sock_);
		sock_ = INVALID_SOCKET;
	}
	state_ = BotState::Closed;
}

void BotSession::shutdownBot() {
	if (sock_ != INVALID_SOCKET) {
		// 서버가 disconnect 를 감지하도록 정상 종료.
		::shutdown(sock_, SD_BOTH);
		::closesocket(sock_);
		sock_ = INVALID_SOCKET;
	}
	state_ = BotState::Closed;
}

} // namespace hc
