#include "rspch.hpp"
#include "RoomServer.hpp"
#include "IocpReactor.hpp"
#include "Listener.hpp"
#include "JobQueuePool.hpp"
#include "JobTimer.hpp"
#include "globalTLS.hpp"

void DoReservedJob() {
	JobTimer::distribute();
}

void DoJob() {
	static constexpr Milliseconds timeOut = 64ms;

	while (true) {
		const auto now = HighResolutionClock::now();
		if (now - LWorkStartTime >= timeOut) {
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
		LWorkStartTime = HighResolutionClock::now();

		// IOCP 이벤트 처리
		reactor.dispatch(10);

		// 예약된 작업 처리
		JobTimer::distribute();

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
