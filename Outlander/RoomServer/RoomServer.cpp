#include "rspch.hpp"
#include "RoomServer.hpp"
#include "IocpReactor.hpp"
#include "Listener.hpp"
#include "JobQueuePool.hpp"
#include "JobTimer.hpp"
#include "RoomManager.hpp"

void DoIocp(IocpReactor& reactor) {
	while (true) {
		reactor.dispatch();
	}
}

void DoJobTimer() {
	auto nextSweep = HighResolutionClock::now();
	while (true) {
		JobTimer::distribute();

		// 지연 파괴 reaper: 맵에서 빠진 방을 JobQueue 유휴 확인 후 풀로 반납한다
		// (docs/serverHandoff.md §4-P0 D). 루프가 hot-spin이라 매 스핀마다 rmMtx_를
		// 잡지 않도록 ~50ms 간격으로만 돈다.
		const auto now = HighResolutionClock::now();
		if (now >= nextSweep) {
			RoomManager::sweepPendingRooms();
			nextSweep = now + std::chrono::milliseconds(50);
		}
	}
}

void DoJob() {
	while (true) {
		auto jq = JobQueuePool::pop();
		if (!jq) continue;

		jq->execute();
	}
}

void RoomServer::start() {
	reactor_.registerHandle(listener_->getHandle());
	listener_->startAccept();

	const int32 coreCnt			= static_cast<int32>(numberOfPhysicalCores());
	const int32 iocpThreadCnt	= 2;
	const int32 jobThreadCnt	= std::max(1, coreCnt - iocpThreadCnt - 1);

	iocpThreads_.reserve( iocpThreadCnt );
	for (int32 i = 0; i < iocpThreadCnt - 1; ++i) {
		iocpThreads_.emplace_back( [this]() { DoIocp( reactor_ ); } );
	}

	jobTimerThread_ = std::thread( DoJobTimer );

	jobThreads_.reserve( jobThreadCnt );
	for (int32 i = 0; i < jobThreadCnt; ++i) {
		jobThreads_.emplace_back( DoJob );
	}

	std::cout << "Room Server started. IOCP: " << iocpThreadCnt
		<< ", Room Update: 1, Execute Job: " << jobThreadCnt
		<< ", Port: " << listener_->listenPort() << '\n';

	// 메인	스레드는 IOCP를 담당
	DoIocp(reactor_);

	for ( auto& t : jobThreads_ ) t.join();
	jobTimerThread_.join();
	for ( auto& t : iocpThreads_ ) t.join();
}
