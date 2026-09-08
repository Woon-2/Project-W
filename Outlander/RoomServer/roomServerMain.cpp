#include "rspch.hpp"
#include "RoomServer.hpp"
#include "SendBuffer.hpp"
#include "AssetManager.hpp"
#include "RoomManager.hpp"
#include "JobTimer.hpp"
#include "networkConfig.hpp"
#include "securityConfig.hpp"
#include "dbConfig.hpp"
#include "DBExecutor.hpp"
#include "EntryTicket.hpp"

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

	// DB는 리스너가 accept를 시작하기 전에 준비돼야 한다.
	// DBExecutor::init은 내부에서 onew를 쓰므로 MemoryManager::init() 이후여야 한다.
	DbConfig dbConfig;
	std::filesystem::path dbConfigPath;
	std::string dbConfigError;
	if (!loadDbConfig(dbConfig, dbConfigPath, dbConfigError)) {
		std::cerr << "[DbConfig] " << dbConfigError << '\n';
		return 1;
	}
	std::cout << "[DbConfig] " << dbConfigPath.string() << '\n';

	if (!DBExecutor::init(dbConfig.connectionCount, dbConfig.connectionString.c_str())) {
		// 연결 실패 원인(SQLSTATE)은 DB 레이어가 이미 출력했다.
		std::cerr << "[DbConfig] failed to connect database\n";
		return 1;
	}

	// 입장 티켓 검증 키. 로비서버와 같은 파일을 공유한다. 키가 없으면 모든 입장을 거부하게 되므로
	// 리스너가 뜨기 전에 치명적으로 실패시킨다.
	SecurityConfig securityConfig;
	std::filesystem::path securityConfigPath;
	std::string securityConfigError;
	if (!loadSecurityConfig(securityConfig, securityConfigPath, securityConfigError)) {
		std::cerr << "[SecurityConfig] " << securityConfigError << '\n';
		return 1;
	}
	std::cout << "[SecurityConfig] " << securityConfigPath.string() << '\n';

	EntryTicketAuthority::init(
		securityConfig.entryTicketSecret.data(),
		static_cast<int32>(securityConfig.entryTicketSecret.size()),
		securityConfig.entryTicketTtlSeconds);

	RoomServer server(networkConfig.room.port);
	server.start();

	JobTimer::clear();
	// 종료 순서 규약(client/docs/shutdownSequence.md): 워커 join 후 DBExecutor → SendBufferManager → MemoryManager → SocketUtils.
	// DBExecutor::shutdown은 내부 풀이 odelete를 쓰므로 MemoryManager::release() 앞이어야 한다.
	// 현재는 start()가 반환하지 않아 도달하지 않지만, 정상 종료 도입 시 이 순서를 유지할 것.
	DBExecutor::shutdown();
	SendBufferManager::release();
	MemoryManager::release();
	SocketUtils::release();
}
