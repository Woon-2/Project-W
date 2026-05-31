#include "lspch.hpp"
#include "LobbyServer.hpp"

int main() {
	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();

	LobbyServer server;
	server.start();

	const int32 coreCnt = numberOfPhysicalCores() - 1;
	std::vector<std::thread> threads( coreCnt );

	for ( int32 i = 0; i < coreCnt; ++i ) {
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
