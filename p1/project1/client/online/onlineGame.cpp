#include "pch.hpp"
#include "onlineGame.hpp"
#include "../global.hpp"
#include "../ServerSession.hpp"
#include "../SendBuffer.hpp"
#include "../errorHandling.hpp"

extern HWND ghWnd;
extern RECT gClientRect;

namespace Online {

Game::Game() {
	// 스레드 풀 초기화
	std::cout << "----------[게임 초기화 설정]----------\n";
	std::cout << "스레드 풀에 사용할 스레드 수를 입력해 주세요.\n";
	std::cout << "컴퓨터의 물리 코어 수: " << numberOfPhysicalCores() << '\n';
	std::cout << "사용 가능한 물리 코어 수: " << numberOfPhysicalCores() - 1 << " (1개 - 메인 스레드)\n";
	std::cout << "스레드 수: ";

	std::size_t threadCnt{};
	std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);

	assetManager_.loadGFXAssets( gfx_ );
}

void Game::setupStage() {
	skybox_.setModel( assetManager_.modelCube( ) );
	skybox_.setSkyboxMaterial( assetManager_.skyboxMaterial( ) );

	player_ = std::make_shared<Object>();

	dirLight_.setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( 160.f ), mu::Degree( 0.f ) ) );
	dirLight_.color = mu::Vec3( 0.8f, 0.8f, 0.8f );
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
	camera_.setOffsetTargetPivot( mu::Vec3( 0.f, 1.f, 0.f ) );
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);
}

void Game::update(Milliseconds deltaTime) {
	processInput(deltaTime);

	// 서버로부터 받은 메시지 처리
	const auto bulkSize = 10u;
	auto messages = std::vector<Message>(bulkSize);

	auto size = messageQueue.try_dequeue_bulk(messages.data(), bulkSize);
	for (auto i = 0u; i < size; ++i) {
		const auto& msg = messages[i];

		switch (msg.type) {
		case MsgType::SetupPlayer:
			player_->setPos(msg.pos);
			player_->setOrient(msg.orient);
			player_->setScale(msg.scale);
			player_->setModel(assetManager_.modelPlayer());
			player_->enableBVRendering();
			break;

		case MsgType::SetupCube: {
			auto cube = Object{};
			cube.setId(msg.objectId);
			cube.setMaterialSetIdx(msg.materialSetIdx);
			cube.setPos(msg.pos);
			cube.setOrient(msg.orient);
			cube.setScale(msg.scale);
			cube.setModel(assetManager_.modelCube());
			cube.enableBVRendering();
			cubes_.emplace_back(std::move(cube));
			break;
		}

		case MsgType::PlayerMove: {
			auto& player = idPlayerMap_[msg.objectId];

			if (player == player_) {
				player->setServerPos(msg.pos);
			}
			else {
				player->setPos(msg.pos);
			}
			break;
		}
		}
	}

	// 물리량 갱신은 게임 갱신과 다르게 고정 주기로 수행한다.
	// 이를 통해 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	//
	// update 함수에서 physicUpdateAcc_ 변수를 통해
	// 물리량 갱신의 주기가 돌아왔는지 판단하고
	// 주기가 되었다면 물리량 갱신을 수행한다.
	physicUpdateAcc_ += deltaTime;
	moveStateSendAcc_ += deltaTime;

	if ( physicUpdateAcc_ >= physicUpdateInterval ) {
		// 물리 시뮬레이션을 위해
		// 물리 시뮬레이션의 대상이 되는 객체들을
		// 한 곳에 모아 PhysicSystem 객체에 전달한다.
		static std::vector<Object*> allObjects{};
		allObjects.resize(cubes_.size() + 1);
		std::ranges::transform(cubes_, allObjects.begin(),
			[](Object& cube) { return &cube; }
		);
		allObjects[cubes_.size()] = player_.get();

		while ( physicUpdateAcc_ >= physicUpdateInterval ) {
			physicSystem_.step( allObjects, physicUpdateInterval );
			physicUpdateAcc_ -= physicUpdateInterval;
		}

		allObjects.clear( );
	}

	if (moveStateSendAcc_ >= moveStateSendInterval_) {
		sendMoveStatePacket();
		moveStateSendAcc_ -= moveStateSendInterval_;
	}

	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / physicUpdateInterval;

	for (auto& cube : cubes_) {
		cube.update(deltaTime, tPhysicInterpolation);
	}

	// 게임 객체들 갱신
	player_->update(deltaTime, tPhysicInterpolation );
	objectsMtx_.lock( );
	for ( auto& obj : otherPlayers_ ) {
		if( obj != player_ ) {
			obj->update( deltaTime, tPhysicInterpolation );
		}
	}
	objectsMtx_.unlock( );

	camera_.update();
	dirLight_.update(deltaTime);
	dirLight_.updateShadowAuxDirectional(camera_.eye(), 100.f, -10.f, 10.f, -10.f, 10.f, 50.f, 200.f);
}

void Game::render() {
	for (auto& cube : cubes_) {
		cube.render(gfx_);
	}

	player_->render(gfx_);
	skybox_.render( gfx_ );
	objectsMtx_.lock( );
	for ( auto& obj : otherPlayers_ ) {
		if( obj != player_ ) {
			obj->render( gfx_ );
		}
	}
	objectsMtx_.unlock( );

	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	auto frameData = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameData );

	gfx_.render();
}

// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
LRESULT Game::receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_INPUT 메시지
		// 마우스 움직임의 경우 WM_MOUSEMOVE 메시지 대신 이 메시지로 처리하는 것이
		// 정확하고 안정적인 처리가 가능하다.
		// 윈도우 경계로부터 영향을 받지 않고, 가상 커서 속도/가속도 설정을 무시하며,
		// Alt-Tab / 창 이동 후에도 상태 복구가 명확하다.
		// (DPI 스케일링 환경에서도 입력이 왜곡되지 않는다고 한다.)
	case WM_INPUT: {
		static auto sRawInputBuffer = std::vector<std::uint8_t>(256);
		UINT rawInputSize{};
		UINT rawInputResult{};

		// 입력 구조체 크기 수신
		rawInputResult = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult != -1, true);

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, true);

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// 마우스에 대한 입력 내용이 상대 좌표여야 한다.
			DISPLAY_ERROR_STR(!(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
				"[Input Error] Game::receiveWndMsg: 마우스 입력 장치가 게임과 호환되지 않습니다.\n"
				"RAWMOUSE의 플래그 중 MOUSE_MOVE_ABSOLUTE가 활성화되어있습니다.",
				true
			);

			// 마우스 이동량 기록
			mouseDeltaX_ += ri->data.mouse.lLastX;
			mouseDeltaY_ += ri->data.mouse.lLastY;
		}
		return 0;
	}

	case WM_SIZE:
		return DefWindowProcA(hWnd, msg, wParam, lParam);

	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}

void Game::sendMoveInputPacket(i32t moveXSign, i32t moveZSign) {
	auto moveStartPacket = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(CSMoveInputPacket),
			.id = static_cast<std::uint16_t>(PacketType::csMoveInput)
		},
		.csMoveInput = {
			.moveXSign = static_cast<std::int16_t>(moveXSign),
			.moveZSign = static_cast<std::int16_t>(moveZSign),
			.timeStamp = static_cast<u32t>(HighResolutionClock::now().time_since_epoch().count())
		}
	};

	i32t packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&moveStartPacket, packetSize);
	serverSession_->send(sendBuffer);
}

void Game::sendMouseMovePacket() {
	const auto forward = player_->forward();
	const auto yaw = std::atan2(forward.x(), forward.z());

	auto mouseMovePacket = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(CSMouseMovePacket),
			.id = static_cast<std::uint16_t>(PacketType::csMouseMove)
		},
		.csMouseMove = {
			.playerYawRadian = yaw,
			.cameraPitchRadian = cameraPitch_,
			.timeStamp = static_cast<u32t>(HighResolutionClock::now().time_since_epoch().count())
		}
	};

	i32t packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&mouseMovePacket, packetSize);
	serverSession_->send(sendBuffer);
}

void Game::sendMoveStatePacket() {
	auto moveStatePacket = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(CSMoveStatePacket),
			.id = static_cast<std::uint16_t>(PacketType::csMoveState)
		},
		.csMoveState = {
			.velocity = player_->physicState().velocity.getXmf(),
			.forward = player_->forward().getXmf(),
			.timeStamp = static_cast<u32t>(HighResolutionClock::now().time_since_epoch().count())
		}
	};

	i32t packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&moveStatePacket, packetSize);
	serverSession_->send(sendBuffer);
}

void Game::sendEnterRoomPacket(i32t roomId) {
	auto packet = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(CSFindRoomPacket),
			.id = static_cast<std::uint16_t>(PacketType::csFindRoom)
		},
		.csFindRoom = {
			.roomId = roomId
		}
	};

	i32t packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&packet, packetSize);
	serverSession_->send(sendBuffer);
	inRoom_ = true;
}

void Game::processInput(Milliseconds deltaTime) {
	if (GetForegroundWindow() != ghWnd) {
		return;
	}

	keyboardStatePrev_ = keyboardStateCurr_;
	DISPLAY_ERROR_GLE( GetKeyboardState(keyboardStateCurr_.data()), false );

	const auto maxSpeed = 10.f;	// 10m/s
	const Seconds zeroToMax = 0.5s;
	const Seconds maxToZero = 0.2s;

	// 서로 상쇄되는 입력들을 감안해서,
	// 현재 이동 입력이 있으면 플레이어 객체의 속도를 변화시킨다.

	const auto moveXSign = (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80);
	const auto moveZSign = (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80);
	const auto moveThreshold = 0.1f;

	if (moveXSign || moveZSign) {
		// 'W'/'S' 입력으로 판정된 Z 부호는 플레이어의 forward 벡터,
		// 'D'/'A' 입력으로 판정된 X 부호는 플레이어의 right 벡터와 곱해 속도의 방향을 정한다.
		const auto moveDirection = mu::NVec3(
			static_cast<float>(moveXSign) * player_->right() + static_cast<float>(moveZSign) * player_->forward()
		);

		// 플레이어 객체의 속력을 증가시킨다.
		const auto moveAmount = Seconds(deltaTime).count() * maxSpeed / zeroToMax.count();
		player_->physicState().velocity += mu::Vec3(moveDirection) * moveAmount;

		// 플레이어 객체의 속력이 최대 속력을 넘지 못하게 한다.
		if (player_->physicState().velocity.len2() > maxSpeed * maxSpeed) {
			player_->physicState().velocity *= maxSpeed / player_->physicState().velocity.len();
		}
	}
	// 이동 입력이 없으면 플레이어 객체의 속력을 감소시킨다. (마찰)
	// 속력이 moveThreshold보다 작다면, 플레이어 객체를 멈춘다.
	else if (player_->physicState().velocity.len2() > moveThreshold * moveThreshold) {
		const auto moveAmount = Seconds(deltaTime).count() * maxSpeed / maxToZero.count();

		// 속력 감소량이 현재 플레이어의 속력보다 크게 계산됐다면,
		// 플레이어의 속력을 0으로 만든다.
		if (moveAmount * moveAmount > player_->physicState().velocity.len2()) {
			player_->physicState().velocity = mu::Vec3();
		}
		// 그렇지 않다면 플레이어가 움직이고 있는 반대 방향의 속도를 더해
		// 플레이어의 속력을 감소시킨다.
		else {
			const auto moveDirection = mu::NVec3(-player_->physicState().velocity);
			player_->physicState().velocity += mu::Vec3(moveDirection) * moveAmount;
		}
	}
	// 플레이어 객체를 멈춘다.
	else {
		player_->physicState().velocity = mu::Vec3();
	}

	// 이동 입력 패킷 전송
	sendMoveInputPacket(moveXSign, moveZSign);

	if (inRoom_) {
		// 1인칭 카메라 시점 설정
		if (keyboardStateCurr_['Q'] & 0x80) {
			camera_.setOffsetFromTargetPreRotation(mu::NQuat{});
			camera_.setOffsetFromTarget(mu::Vec3(0.f, 2.f, 0.5f));
			camera_.setOffsetTargetPivot(mu::Vec3(0.f, 2.f, 8.f));
			cameraMode_ = CameraMode::FirstPerson;
		}
		// 3인칭 카메라 시점 설정
		if (keyboardStateCurr_['E'] & 0x80) {
			camera_.setXXPreRotation(mu::NQuat{});
			camera_.setOffsetFromTarget(mu::Vec3(0.f, 1.8f, -2.5f));
			camera_.setOffsetTargetPivot(mu::Vec3(0.f, 1.f, 0.f));
			cameraMode_ = CameraMode::ThirdPerson;
		}

		// 카메라 움직임 처리
		const auto mouseSensitivity = mu::pi * 2.f;

		switch (cameraMode_) {
		case CameraMode::ThirdPerson: {
			auto yaw = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f),
				mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left))
			);
			
			player_->setOrient(player_->orient() * yaw);

			cameraPitch_ = std::clamp(
				static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
				-mu::pi * 0.16f,
				mu::pi * 0.3f
			);

			camera_.setOffsetFromTargetPreRotation(mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)));

			sendMouseMovePacket();

			mouseDeltaX_ = 0;
			mouseDeltaY_ = 0;
			break;
		}
		case CameraMode::FirstPerson: {
			auto yaw = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f),
				mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left))
			);

			player_->setOrient(player_->orient() * yaw);

			cameraPitch_ = std::clamp(
				static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
				-mu::pi * 0.16f,
				mu::pi * 0.3f
			);

			camera_.setXXPreRotation(mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)));

			mouseDeltaX_ = 0;
			mouseDeltaY_ = 0;
			break;
		}
		}

		if (keyboardStateCurr_[VK_LBUTTON] & 0x80) {

		}
	}
	else {
		// 방에 들어가지 않은 상태일 때
		// 방 입장 키 처리

		if (keyboardStateCurr_['1'] & 0x80) {
			sendEnterRoomPacket(1);
		}
		if (keyboardStateCurr_['2'] & 0x80) {
			sendEnterRoomPacket(2);
		}
		if (keyboardStateCurr_['3'] & 0x80) {
			sendEnterRoomPacket(3);
		}
		if (keyboardStateCurr_['4'] & 0x80) {
			sendEnterRoomPacket(4);
		}
	}
}

}	// namespace Online