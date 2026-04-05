#include "rspch.hpp"
#include "RoomServer.hpp"
#include "IocpReactor.hpp"
#include "Listener.hpp"
#include "JobQueuePool.hpp"
#include "JobTimer.hpp"

void DoReservedJob() {
	const uint64 now = GetTickCount64();
	JobTimer::distribute(now);
}

void DoJob() {
	while (true) {
		const uint64 now = GetTickCount64();
		if (now >= LEndTick) {
			break;
		}

		auto jobQueue = JobQueuePool::pop();
		if (jobQueue == nullptr) {
			break;
		}

		jobQueue->execute();
	}
}

void DoWork(IocpReactor& reactor) {
	while (true) {
		LEndTick = GetTickCount64() + 64;	// 64ms동안 작업 수행 / TODO: 작업을 수행해야 하는 시간을 유동적으로 조절하도록 구현하면 좋을 것 같다

		// IOCP 이벤트 처리
		reactor.dispatch(10);

		// 예약된 작업 처리
		DoReservedJob();

		// 작업 큐 처리
		DoJob();
	}
}

void RoomServer::start() {
	reactor_.registerHandle(listener_.getHandle());
	listener_.startAccept();

	const int32 threadCnt = static_cast<int32>(numberOfPhysicalCores());
	workerThreads_.reserve(threadCnt);

	for (int32 i = 0; i < threadCnt - 1; ++i) {
		workerThreads_.emplace_back([this]() {
			DoWork(reactor_);
		});
	}

	// 메인 스레드도 작업을 수행하도록 한다.
	DoWork(reactor_);

	std::cout << "Room Server started with " << threadCnt << " worker threads.\n";

	for(auto& th : workerThreads_) {
		th.join();
	}
}
