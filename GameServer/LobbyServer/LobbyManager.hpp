#ifndef lobby_manager_hpp
#define lobby_manager_hpp

#include <shared_mutex>
#include <random>

class LobbyRoom;

class LobbyManager {
public:
	static std::shared_ptr<LobbyRoom> createRoom();
	static std::shared_ptr<LobbyRoom> findRoom( const std::string& code );
	static void removeRoom( const std::string& code );

	static std::string generateCode() {
		static constexpr char pool[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		static std::mt19937 rng{ std::random_device{}() };
		static std::uniform_int_distribution<int32> dist( 0, 35 );

		std::string code( 6, '\0' );
		for ( auto& c : code ) {
			c = pool[ dist( rng ) ];
		}

		return code;
	}

private:
	static std::shared_mutex mutex_;
	static std::unordered_map<std::string, std::shared_ptr<LobbyRoom>> rooms_;
};

#endif // lobby_manager_hpp
