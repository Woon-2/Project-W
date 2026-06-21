#ifndef headless_metrics_hpp
#define headless_metrics_hpp

#include <atomic>
#include <cstdint>

namespace hc {

// thread-safe 누적 통계. 단일 스레드 이벤트 루프이지만, 향후 IO 워커 샤딩을 대비해 atomic.
struct Metrics {
	std::atomic<uint64_t> sendPackets{0};
	std::atomic<uint64_t> recvPackets{0};
	std::atomic<uint64_t> sendBytes{0};      // 송신을 위해 큐잉한(=발생시킨) 바이트
	std::atomic<uint64_t> recvBytes{0};      // 소켓에서 실제로 읽은 바이트
	std::atomic<uint64_t> connectFail{0};
	std::atomic<uint64_t> disconnect{0};
	std::atomic<uint64_t> parseError{0};
	std::atomic<uint64_t> npcBatchRecv{0};   // 후속 FPS 측정용: S_NpcMoveBatch 수신 수
	std::atomic<uint64_t> maxSendQueue{0};   // 봇별 송신 backlog 최대치(도구측 병목 신호)

	void noteSendQueue(uint64_t pending) {
		uint64_t cur = maxSendQueue.load(std::memory_order_relaxed);
		while (pending > cur &&
		       !maxSendQueue.compare_exchange_weak(cur, pending, std::memory_order_relaxed)) {
		}
	}

	// 1초 delta 계산을 위한 스냅샷.
	struct Snapshot {
		uint64_t sendPackets, recvPackets, sendBytes, recvBytes;
		uint64_t connectFail, disconnect, parseError, npcBatchRecv;
	};

	Snapshot snapshot() const {
		return Snapshot{
			sendPackets.load(std::memory_order_relaxed),
			recvPackets.load(std::memory_order_relaxed),
			sendBytes.load(std::memory_order_relaxed),
			recvBytes.load(std::memory_order_relaxed),
			connectFail.load(std::memory_order_relaxed),
			disconnect.load(std::memory_order_relaxed),
			parseError.load(std::memory_order_relaxed),
			npcBatchRecv.load(std::memory_order_relaxed),
		};
	}
};

} // namespace hc

#endif // headless_metrics_hpp
