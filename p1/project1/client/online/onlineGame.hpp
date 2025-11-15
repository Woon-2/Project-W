#ifndef __Online_game_HPP
#define __Online_game_HPP

#include "../pch.hpp"
#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../light.hpp"

namespace Online {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;

	void addPlayer( const std::shared_ptr<Object>& player ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		otherPlayers_.push_back( player );
		idPlayerMap_[ player->getId( ) ] = player;
	}

	void removePlayer( i32t playerId ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		std::erase_if( otherPlayers_, [ playerId ]( const std::shared_ptr<Object>& obj ) {
			return obj->getId( ) == playerId;
		} );
		idPlayerMap_.erase( playerId );
	}

	bool findPlayer( i32t playerId ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		return idPlayerMap_.find( playerId ) != idPlayerMap_.end( );
	}

	void createPlayer( i32t playerId, float x, float y, float z ) {
		auto newPlayer = std::make_shared<Object>( );

		newPlayer->setId( playerId );
		newPlayer->setPos( mu::Vec3( x, y, z ) );
		newPlayer->setModel( gfx_.modelPlayer( ) );
		newPlayer->setScale( 0.15f );

		addPlayer( newPlayer );
	}

	void setServerSession( const SPServerSession& serverSession ) {	serverSession_ = serverSession;	}

	const std::shared_ptr<Object>& getPlayer( ) const { return player_; }
	std::shared_ptr<Object>& getPlayerById( i32t playerId ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		return idPlayerMap_[ playerId ];
	}

private:
	void processInput(Milliseconds deltaTime);

	GFX gfx_{};
	ThreadPool threadPool_{};

	SPServerSession serverSession_{ };

	std::vector<std::vector<std::vector<Object>>> cubes_{};

	std::shared_ptr<Object> player_{};
	std::vector<std::shared_ptr<Object>> otherPlayers_{ };

	std::mutex objectsMtx_{ };
	std::unordered_map<i32t, std::shared_ptr<Object>> idPlayerMap_{ };

	Camera camera_{};
	Light dirLight_{};
};

}	// namespace Online

#endif	// __Online_game_HPP