#include "rspch.hpp"
#include "RoomServer.hpp"
#include "IocpReactor.hpp"
#include "Listener.hpp"

void RoomServer::start() {
	reactor_.registerHandle(listener_.getHandle());
	listener_.startAccept();

	const int32 threadCnt = static_cast<int32>(numberOfPhysicalCores());
	workerThreads_.reserve(threadCnt);

	for (int32 i = 0; i < threadCnt; ++i) {
		workerThreads_.emplace_back([this]() {
			while (true) {
				if (!reactor_.dispatch()) {
					break;
				}
			}
		});
	}

	std::cout << "Room Server started with " << threadCnt << " worker threads.\n";

	for(auto& th : workerThreads_) {
		th.join();
	}
}
