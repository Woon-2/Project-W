#include "pch.hpp"
#include "Service.hpp"
#include "IocpCore.hpp"
#include "Session.hpp"
#include "SendBuffer.hpp"
#include "GameSession.hpp"
#include "GameSessionManager.hpp"
#include "GameLogicManager.hpp"

#include "assetManager.hpp"
#include "RoomManager.hpp"

#include <chrono>
using namespace std::chrono_literals;

// [상훈이가 구현해야 할 것]
// - Room::enter에서 Room::cubes 정보도 패킷에 담아 보내기
// - GameSession의 플레이어 객체 정보 Room에 Object화 해놓기
//   - GameSession이 소유해야 할지 Room이 소유해야 할지 결정
// - Room별 물리 시뮬레이션 구현 (고정 주기: 60 fps 또는 30 fps)
//   - 충돌 피드백에 의해 움직인 큐브도 Move 패킷을 보내야 한다
//
// 현재 구조
// AssetManager::loadAssets에 의해 모든 모델과 레벨이 로드됨
// 레벨 정보를 RoomManager에 RoomManager::setLevelData로 등록
// RoomManager::CreateRoom 시 Room::init을 레벨 정보를 인자로 호출
// Room::init은 레벨 정보를 바탕으로 해당 룸의 객체들 초기화
//
// Model은 모델 이름과 로컬 바운딩 볼륨 정보만을 보관함
// Object에 setModel을 하면 Model에 있는 로컬 바운딩 볼륨 정보를 기반으로
// 월드 공간 바운딩 볼륨이 생성됨
// 그 이후 Object의 setPos, setScale등은 월드 공간 바운딩 볼륨을 갱신함
// 물리 시뮬레이션은 이 월드 공간 바운딩 볼륨간에 이루어져야 함

int main( )
{
	pushLoggerA("standard", &std::cout);
	pushLoggerW("standard", &std::wcout);

	AssetManager assetManager{};
	assetManager.loadAssets();

	RoomManager::setLevelData(assetManager.level());
	RoomManager::setPlayerModelData(assetManager.modelPlayer());

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

	GameLogicManager::init(2);
	GameLogicManager::startAll();

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

	GameLogicManager::stopAll();
	GameLogicManager::release();
	SocketUtils::rel( );
}