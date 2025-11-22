#include "pch.hpp"
#include "Service.hpp"
#include "IocpCore.hpp"
#include "Session.hpp"
#include "SendBuffer.hpp"
#include "GameSession.hpp"
#include "GameSessionManager.hpp"

#include "assetManager.hpp"
#include "RoomManager.hpp"

#include <chrono>
using namespace std::chrono_literals;

int main( )
{
	pushLoggerA("standard", &std::cout);
	pushLoggerW("standard", &std::wcout);

	AssetManager assetManager{};
	assetManager.loadAssets();

	RoomManager::setLevelData(assetManager.level());

	auto characterModel = loadModelFromFile("../resources/models/vanguardServer.bin");
	auto cubeModel = loadModelFromFile("../resources/models/cubeServer.bin");
	dumpLog();

	SocketUtils::init( );

	auto service = std::make_shared<ServerService>(
		NetAddress( serverIp, serverPort ),
		std::make_shared<IocpCore>( ), nullptr, 100
	);

	// temporary
	service->setSessionFactory( []( ) {
		return std::make_shared<GameSession>( );
	} );

	ASSERT_CRASH( service->start( ) );

	std::vector<std::thread> threads;
	for ( auto i = 0; i < 4; ++i ) {	// TODO: 쓰레드 개수 조정해야 됨
		threads.emplace_back( [ &service ]( ) {
			while ( true ) {
				service->getIocpCore( )->dispatch( );
			}
		} );
	}

	//// temporary
	//char sendData[ ] = "Hello, World!";
	//while ( true ) {
	//	auto sendBuffer = std::make_shared<SendBuffer>( 100 );
	//	sendBuffer->header.size = ( sizeof( sendData ) + sizeof( PacketHeader ) );
	//	sendBuffer->header.id = 1; // temporary
	//	sendBuffer->copyData( sendData, sizeof( sendData ) );
	//	GameSessionManager::broadcast( sendBuffer );

	//	std::this_thread::sleep_for( 250ms );
	//}

	for ( auto& thread : threads ) {
		thread.join( );
	}

	SocketUtils::rel( );
}