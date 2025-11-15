#ifndef SERVER_SESSION_HPP
#define SERVER_SESSION_HPP

#include "Session.hpp"
#include "object.hpp"

class ServerSession : public PacketSession {
public:
	ServerSession( ) : player_( nullptr ) {}
	virtual ~ServerSession( ) { }

	virtual void onConnected( ) override;
	virtual void onDisconnected( ) override;
	virtual int32 onRecvPacket( uint8* buffer, int32 len ) override;
	virtual void onSend( int32 len ) override {	/*std::cout << "[Client] Sent " << len << " bytes to server.\n";*/ }

	void setPlayer( const std::shared_ptr<Object>& player ) {
		player_ = player;

		/*std::lock_guard<std::mutex> lock( gMtx );
		gObjects[ player->getId( ) ] = player;*/
	}

	std::shared_ptr<Object> getPlayer( ) const { return player_; }

private:
	std::shared_ptr<Object> player_;
};

#endif // SERVER_SESSION_HPP