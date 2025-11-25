#ifndef GAME_SESSION_MANAGER_HPP
#define GAME_SESSION_MANAGER_HPP

class GameSessionManager {
public:
	static void add( const SPGameSession& session );
	static void remove( const SPGameSession& session );
	static void broadcast( const SPSendBuffer& sendBuffer );

	static SPGameSession findGameSession(int32 sessionId) {
		return sessions_[sessionId];
	}

private:
	static std::mutex mtx_;
	static std::unordered_map<int32, SPGameSession> sessions_;
};

#endif // GAME_SESSION_MANAGER_HPP