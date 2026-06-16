#include "lspch.hpp"
#include "LobbyServer.hpp"
#include "SendBuffer.hpp"

int main() {
	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();

	LobbyServer server;
	server.start();

	const auto coreCnt = numberOfPhysicalCores() - 1;
	std::vector<std::thread> threads( coreCnt );

	for ( size_t i = 0; i < coreCnt; ++i ) {
		threads[ i ] = std::thread( [&server] { 
			while ( true ) {
				server.reactor().dispatch();
			}
		} );
	}

	while ( true ) {
		server.reactor().dispatch();
	}

	for( auto& t : threads ) {
		t.join();
	}

	// 종료 순서 규약(shutdownSequence.md): 워커 join 후 SendBufferManager → MemoryManager → SocketUtils.
	// 현재는 위 루프가 무한이라 도달하지 않지만, 정상 종료 도입 시 이 순서를 유지할 것.
	SendBufferManager::release();
	MemoryManager::release();
	SocketUtils::release();
}
