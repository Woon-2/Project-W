#include "rspch.hpp"
#include "RoomServer.hpp"
#include "IocpReactor.hpp"
#include "Listener.hpp"
#include "JobQueuePool.hpp"

void DoJob() {
	while (true) {
		auto jobQueue = JobQueuePool::pop();
		if (jobQueue == nullptr) {
			break;
		}

		jobQueue->execute();
	}
}

void RoomServer::start() {
	reactor_.registerHandle(listener_.getHandle());
	listener_.startAccept();

	const int32 threadCnt = static_cast<int32>(numberOfPhysicalCores());
	workerThreads_.reserve(threadCnt);

	for (int32 i = 0; i < threadCnt; ++i) {
		workerThreads_.emplace_back([this]() {
			while (true) {
				reactor_.dispatch(10);
				DoJob();
			}
		});
	}

	std::cout << "Room Server started with " << threadCnt << " worker threads.\n";

	for(auto& th : workerThreads_) {
		th.join();
	}
}
