#include "rspch.hpp"
#include "RoomServer.hpp"
#include "SendBuffer.hpp"
#include "AssetManager.hpp"
#include "RoomManager.hpp"
#include "JobTimer.hpp"

int main()
{
	pushLoggerA("standard", &std::cout);
	pushLoggerW("standard", &std::wcout);
	
	AssetManager assetManager;
	assetManager.loadAssets();

	RoomManager::setLevelData(assetManager.level());
	RoomManager::setPlayerModelData(assetManager.modelPlayer());

	dumpLog();

	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();
	RoomIdPool::init();

	RoomServer server;
	server.start();

	JobTimer::clear();
	MemoryManager::release();
	SocketUtils::release();
}