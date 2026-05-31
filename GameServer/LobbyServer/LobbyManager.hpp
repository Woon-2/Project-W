#ifndef lobby_manager_hpp
#define lobby_manager_hpp

class LobbyRoom;

class LobbyManager {
public:
	static LobbyRoom* createRoom();
	static LobbyRoom* findRoom(std::string_view code);
	static void removeRoom(std::string_view code);
};

#endif // lobby_manager_hpp
