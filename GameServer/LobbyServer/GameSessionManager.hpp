#ifndef game_session_manager_hpp
#define game_session_manager_hpp

class GameSession;

class GameSessionManager {
public:
	static void addSession(GameSession* session);
	static void removeSession(uint32 sessionId);

private:
	static std::mutex mtx_;
	static std::unordered_map<uint32, GameSession*> sessionMap_;
};

#endif  // game_session_manager_hpp