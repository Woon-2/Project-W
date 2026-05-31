#include "lspch.hpp"
#include "LobbyServer.hpp"

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

	MemoryManager::release();
	SocketUtils::release();
}
