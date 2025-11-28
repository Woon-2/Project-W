#ifndef GAME_SESSION_MANAGER_HPP
#define GAME_SESSION_MANAGER_HPP

class GameSessionManager {
public:
	static void add( const SPGameSession& session );
	static void remove( const SPGameSession& session );
	static void broadcast( const SPSendBuffer& sendBuffer );

	static SPGameSession findGameSession(int32 sessionId) {
		if (sessions_.find(sessionId) != sessions_.end()) {
			return sessions_[sessionId];
		}
		else {
			return nullptr;
		}
	}

private:
	static std::mutex mtx_;
	static std::unordered_map<int32, SPGameSession> sessions_;
};

#endif // GAME_SESSION_MANAGER_HPP