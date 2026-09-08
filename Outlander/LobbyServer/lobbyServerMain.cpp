#include "lspch.hpp"
#include "LobbyServer.hpp"
#include "LobbyManager.hpp"
#include "SendBuffer.hpp"
#include "networkConfig.hpp"
#include "dbConfig.hpp"
#include "DBExecutor.hpp"
#include "securityConfig.hpp"
#include "EntryTicket.hpp"

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

	// 입장 티켓 서명 키. 룸서버와 같은 파일을 공유하며, 없으면 티켓을 만들 수 없으므로
	// DB와 마찬가지로 리스너가 뜨기 전에 치명적으로 실패시킨다.
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

	// 종료 순서 규약(shutdownSequence.md): 워커 join 후 DBExecutor → SendBufferManager → MemoryManager → SocketUtils.
	// DBExecutor::shutdown은 내부 풀이 odelete를 쓰므로 MemoryManager::release() 앞이어야 한다.
	// 현재는 위 루프가 무한이라 도달하지 않지만, 정상 종료 도입 시 이 순서를 유지할 것.
	DBExecutor::shutdown();
	SendBufferManager::release();
	MemoryManager::release();
	SocketUtils::release();
}
