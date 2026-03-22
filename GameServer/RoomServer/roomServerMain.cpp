#include "rspch.hpp"
#include "RoomServer.hpp"
#include "SendBuffer.hpp"

int main()
{
	
	SocketUtils::init();
	MemoryManager::init();
	IdPool::init();
	RoomIdPool::init();

	RoomServer server;
	server.start();

	SendBufferManager::clear();
	MemoryManager::release();
	SocketUtils::release();
}