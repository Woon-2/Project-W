#include "lspch.hpp"
#include "LobbyServer.hpp"
#include "LobbyManager.hpp"
#include "SendBuffer.hpp"
#include "networkConfig.hpp"

int main() {
	NetworkConfig networkConfig;
	std::filesystem::path networkConfigPath;
	std::string networkConfigError;
	if (!loadNetworkConfig(networkConfig, networkConfigPath, networkConfigError)) {
		std::cerr << "[NetworkConfig] " << networkConfigError << '\n';
		return 1;
	}
	std::cout << "[NetworkConfig] " << networkConfigPath.string() << '\n'
		<< "  Lobby: " << networkConfig.lobby.ip << ':' << networkConfig.lobby.port << '\n'
		<< "  Room: " << networkConfig.room.ip << ':' << networkConfig.room.port << '\n';

	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();

	LobbyManager::configureRoomEndpoint(networkConfig.room);
	LobbyServer server(networkConfig.lobby.port);
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
