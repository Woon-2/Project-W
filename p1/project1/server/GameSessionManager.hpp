#ifndef GAME_SESSION_MANAGER_HPP
#define GAME_SESSION_MANAGER_HPP

class GameSessionManager {
public:
	static void add( const SPGameSession& session );
	static void remove( const SPGameSession& session );
	static void broadcast( const SPSendBuffer& sendBuffer );

	static const std::set<SPGameSession>& getSessions( ) { return sessions_; }

private:
	static std::mutex mtx_;
	static std::set<SPGameSession> sessions_;
};

#endif // GAME_SESSION_MANAGER_HPP