#ifndef __Online_game_HPP
#define __Online_game_HPP

#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../skybox.hpp"
#include "../light.hpp"
#include "../AssetManager.hpp"
#include "../standalone/physics.hpp"
#include "../animation.hpp"
#include "../basicPlayerHpUI.hpp"
#include "../event.hpp"
#include "../spriteAnimation.hpp"
#include "../crosshair.hpp"

class Timer;

namespace Online {

enum class MsgType : u8t {
	None,
	SetupPlayer,
	SetupCube,
	PlayerMouseMove,
	PlayerRollback,
	PlayerMove,
	Fire,
	Reload,
	HitResult,
	Death
};

struct Message {
	MsgType type{MsgType::None};
	i32t objectId;
	i32t targetId;
	u32t materialSetIdx;
	mu::Vec3 pos;
	mu::Vec3 evVelocity;
	mu::NQuat orient;
	mu::Vec3 scale;
	float cameraPitch{0.f};
	i32t currHp{0};
	i32t bulletCnt{0};
};

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();

	GameType type() const override { return GameType::Online; }

	void setTimer(Timer* pTimer) { pTimer_ = pTimer; }
	// 객체들을 생성한다.
	void setupStage();

	// 게임의 업데이트는 다음 순서대로 이루어진다.
	// 입력 처리
	// 네트워크 패킷 처리
	// 이벤트 처리
	// 물리 업데이트 루틴
	// 객체별 업데이트 루틴
	// 애니메이션 업데이트
	void update(Milliseconds deltaTime) override;
	void render() override;
	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	LRESULT receiveWndMsg( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

	void addClientPlayer( i32t playerId, float x, float y, float z ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );

		player_ = std::make_shared<Object>( );

		player_->setId( playerId );
		idPlayerMap_[ player_->getId( ) ] = player_;
	}

	void addOtherPlayer( const std::shared_ptr<Object>& player ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		player->setAnimBlender(animSystem_, assetManager_);
		otherPlayers_.push_back( player );
		idPlayerMap_[ player->getId( ) ] = player;
		auto [pPair, _] = otherPlayerHpUIs_.try_emplace(player->getId());
		auto& ui = pPair->second;
		ui.setTexture(assetManager_.playerHpLine());
	}

	void removePlayer( i32t playerId );

	bool findPlayer( i32t playerId ) {
		std::lock_guard<std::mutex> lock( objectsMtx_ );
		return idPlayerMap_.find( playerId ) != idPlayerMap_.end( );
	}

	void createOtherPlayer( i32t playerId, float x, float y, float z ) {
		auto newPlayer = std::make_shared<Object>( );

		newPlayer->setId( playerId );
		newPlayer->setPos( mu::Vec3( x, y, z ) );
		newPlayer->setModel( assetManager_.modelPlayer( ) );
		newPlayer->setScale( 1.f );
		newPlayer->enableBVRendering();
		// 임시
		newPlayer->setHp(100);
		// newPlayer->setAmmo(30);

		Equipment rifle{};
		rifle.socketType = Bone::SocketType::RightHand;
		rifle.object = std::make_unique<Object>();
		// rifle.object->setModel(assetManager_.modelRifle());
		rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

		newPlayer->equip(std::move(rifle));

		addOtherPlayer( newPlayer );
	}

	//void setServerSession( const SPServerSession& serverSession ) {	serverSession_ = serverSession;	}

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

	struct SpriteAnimationOwned {
		SpriteAnimation anim;
		std::shared_ptr<Object> pOwner;
	};

	void sendMouseMovePacket();
	void sendMoveStatePacket();
	void sendEnterRoomPacket(i32t roomId);
	void processInput(Milliseconds deltaTime);
	void processInputLobby(Milliseconds deltaTime);
	void processInputGame(Milliseconds deltaTime);

	// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
	// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
	void captureCursor();
	// 커서 캡처가 설정되어있다면 해제한다.
	// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
	void releaseCursor();
	void hideCursor();
	void showCursor();

	AssetManager assetManager_{};

	PhysicSystem physicSystem_{};
	Seconds physicUpdateAcc_{ 0s };	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{ 1s / 60.f };	// 60fps로 물리 업데이트

	Seconds moveStateSendAcc_{0s};
	Seconds moveStateSendInterval_{1s / 20.f};	// 20fps로 이동 상태 패킷 전송
	
	AnimSystem animSystem_{};

	GFX gfx_{};
	ThreadPool threadPool_{};

	EventList eventList_{};
	Timer* pTimer_ = nullptr;

	//SPServerSession serverSession_{ };

	std::vector<Object> cubes_{};

	std::shared_ptr<Object> player_{};
	std::vector<std::shared_ptr<Object>> otherPlayers_{ };

	SkyboxObject skybox_{};

	std::mutex objectsMtx_{ };
	std::unordered_map<i32t, std::shared_ptr<Object>> idPlayerMap_{ };

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	// 카메라 yaw는 기본적으로 플레이어에 대한 오프셋으로만 작동하지만,
	// 플레이어 사망 이후에는 이 변수로 작동한다.
	mu::Radian cameraYaw_ = 0.f;
	CameraMode cameraMode_ = CameraMode::ThirdPerson;

	Light dirLight_{};
	AssetConfigs assetConfigs_{};

	SpriteAnimation slimeSprite_{};

	bool playerDead_{};

	BasicPlayerHpUI playerHpUI_{};
	std::unordered_map<i32t, BasicPlayerHpUI> otherPlayerHpUIs_{};
	Crosshair crosshair_{};

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};

	bool inRoom_ = false;

	mu::Vec3 prevVelocity_{};
	mu::Vec3 currVelocity_{};
};

}	// namespace Online

#endif	// __Online_game_HPP