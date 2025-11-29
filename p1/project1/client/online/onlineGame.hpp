#ifndef __Online_game_HPP
#define __Online_game_HPP

#include "../pch.hpp"
#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../skybox.hpp"
#include "../light.hpp"
#include "../AssetManager.hpp"
#include "../standalone/physics.hpp"

namespace Online {

enum class MsgType : u8t {
	SetupPlayer,
	SetupCube,
	PlayerMove,
};

struct Message {
	MsgType type;
	i32t objectId;
	mu::Vec3 pos;
	mu::NQuat orient;
	mu::Vec3 scale;
};

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;
	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	LRESULT receiveWndMsg( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

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
		newPlayer->setModel( assetManager_.modelPlayer( ) );
		newPlayer->setScale( 1.f );

		addPlayer( newPlayer );
	}

	void setServerSession( const SPServerSession& serverSession ) {	serverSession_ = serverSession;	}

	const std::shared_ptr<Object>& getPlayer( ) const { return player_; }
	std::shared_ptr<Object>& getPlayerById( i32t playerId ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		return idPlayerMap_[ playerId ];
	}


private:
	enum class CameraMode {
		FirstPerson,
		ThirdPerson
	};

	void updateMoveState(int vk, Direction dir);
	void processInput(Milliseconds deltaTime);

	AssetManager assetManager_{};

	PhysicSystem physicSystem_{};
	Seconds physicUpdateAcc_{ 0s };	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{ 1s / 60.f };	// 60fps로 물리 업데이트
	
	GFX gfx_{};
	ThreadPool threadPool_{};

	SPServerSession serverSession_{ };

	std::vector<Object> cubes_{};

	std::shared_ptr<Object> player_{};
	std::vector<std::shared_ptr<Object>> otherPlayers_{ };

	SkyboxObject skybox_{};

	std::mutex objectsMtx_{ };
	std::unordered_map<i32t, std::shared_ptr<Object>> idPlayerMap_{ };

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	CameraMode cameraMode_ = CameraMode::ThirdPerson;

	Light dirLight_{};

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};

	bool inRoom_ = false;
};

}	// namespace Online

#endif	// __Online_game_HPP