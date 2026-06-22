#include "rspch.hpp"
#include "RoomServer.hpp"
#include "SendBuffer.hpp"
#include "AssetManager.hpp"
#include "RoomManager.hpp"
#include "JobTimer.hpp"
#include "networkConfig.hpp"

int main()
{
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

	pushLoggerA("standard", &std::cout);
	pushLoggerW("standard", &std::wcout);
	
	AssetManager assetManager;
	assetManager.loadAssets();

	RoomManager::setLevelData(assetManager.level());
	RoomManager::setPlayerModelData(assetManager.modelPlayer());
	RoomManager::setPlayerAnimations(&assetManager.playerAnimations());

	dumpLog();

	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();
	RoomIdPool::init();

	RoomServer server(networkConfig.room.port);
	server.start();

	JobTimer::clear();
	// 종료 순서 규약(client/docs/shutdownSequence.md): 워커 join 후 SendBufferManager → MemoryManager → SocketUtils.
	// 현재는 start()가 반환하지 않아 도달하지 않지만, 정상 종료 도입 시 이 순서를 유지할 것.
	SendBufferManager::release();
	MemoryManager::release();
	SocketUtils::release();
}
