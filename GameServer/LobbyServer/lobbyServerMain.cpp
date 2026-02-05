#include "pch.hpp"
#include "Memory.hpp"
#include "Server.hpp"
#include "IdPool.hpp"
#include "SendBuffer.hpp"

int main()
{
	SocketUtils::init();
	Memory::init();
	IdPool::init();

	Server::start();

	// 물리 코어 수 - 1(main 쓰래드) 개 만큼 쓰래드 생성
	auto coreCnt = numberOfPhysicalCores() - 1;
	auto threads = std::vector<std::thread>(coreCnt);
	for (size_t i = 0; i < coreCnt; ++i) {
		threads[i] = std::thread([]() {
			while (true) {
				Server::iocpCore().dispatch();
			}
		});
	}

	for (auto& th : threads) {
		th.join();
	}

	Server::stop();

	SendBufferManager::clear();
	Memory::release();
	SocketUtils::release();
}