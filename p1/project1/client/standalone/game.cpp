#include "pch.hpp"
#include "game.hpp"

#include "../errorHandling.hpp"
#include "../binaryImport.hpp"
#include "../timer.hpp"

extern RECT gClientRect;

namespace StandAlone {

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

	assetManager_.loadGFXAssets(gfx_);
	assetManager_.loadAnimations();
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");

	// 이후 레벨에서 사용하는 스카이박스 정보들을 읽어들일 수 있지만
	// 스카이박스 재질을 하나만 사용하므로 굳이 읽지 않는다.

	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";

	dumpLog();

	skybox_.setModel(assetManager_.modelCube());
	skybox_.setSkyboxMaterial(assetManager_.skyboxMaterial());

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(160.f), mu::Degree(0.f)));
	dirLight_.color = mu::Vec3(0.8f, 0.8f, 0.8f);
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
	camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);

	for (auto& cube : cubes_) {
		cube.enableBVRendering();
	}

	player_->enableBVRendering();

	billboard_.setTexture( assetManager_.billBoard0() );

	slimeSprite_.init( assetManager_.slimeAnimation() );

	playerHpUIs_.resize( 1u );
	playerHpUIs_[0].setTexture( assetManager_.playerHpLine() );
	// playerHpUIs_[1].setTexture( assetManager_.playerHpFrame() );
	playerHpUIs_[0].setTextImage( assetManager_.textPlayerHp() );
}

void Game::importNode(std::ifstream& ifs) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	gSharedLog << "[Level Load] 레벨 노드 " << name << " 로드 완료\n";

	readHeadTag(ifs, "LocalTRS");
	const auto localT = readVec3(ifs, "Position");
	const auto localR = readVec4(ifs, "Rotation");
	const auto localS = readVec3(ifs, "Scale");
	readTailTag(ifs, "LocalTRS");

	readHeadTag(ifs, "WorldTRS");
	const auto worldT = readVec3(ifs, "Position");
	const auto worldR = readVec4(ifs, "Rotation");
	const auto worldS = readVec3(ifs, "Scale");
	readTailTag(ifs, "WorldTRS");

	Object object{};
	object.setPos(DirectX::XMLoadFloat3(&worldT));
	object.setOrient(DirectX::XMLoadFloat4(&worldR));
	object.setScale(DirectX::XMLoadFloat3(&worldS));

	if (type == "Cube") {
		auto& cube = cubes_.emplace_back(std::move(object));
		cube.setModel(assetManager_.modelCube());
		importCube(ifs, cube);
	}
	else if (type == "PlayerStart") {
		importPlayerStart(ifs, object);
	}
	else {
		// no-op
	}

	const auto childCnt = readInteger(ifs, "ChildCnt");
	readHeadTag(ifs, "Children");
	for (int i = 0; i < childCnt; ++i) {
		importNode(ifs);
	}
	readTailTag(ifs, "Children");

	readTailTag(ifs, "Node");
}

void Game::importCube(std::ifstream& ifs, Object& cube) {
	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void Game::importPlayerStart(std::ifstream& ifs, Object& player) {
	if (playerSpawned_) {
		return;
	}
	playerSpawned_ = true;

	player_ = std::make_shared<Object>(std::move(player));
	player_->setModel(assetManager_.modelPlayer());
	player_->setAnimBlender(animSystem_, assetManager_);

	Equipment rifle{};
	rifle.socketType = Bone::SocketType::RightHand;
	rifle.object = std::make_unique<Object>();
	rifle.object->setModel(assetManager_.modelRifle());
	rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

	player_->equip(std::move(rifle));
}

// 게임의 업데이트는 다음 순서대로 이루어진다.
// 입력 처리
// 이벤트 처리
// 물리 업데이트 루틴
// 객체별 업데이트 루틴
// 애니메이션 업데이트
void Game::update(Milliseconds deltaTime) {
	// 입력 처리
	processInput(deltaTime);

	// 이벤트 처리
	for (auto pEvRaw : eventList_) {
		auto pEv = reinterpret_cast<BasicEvent*>(pEvRaw);
		switch (pEv->type) {
		case EventType::Fire:
			player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
			for (auto& ui : playerHpUIs_) {
				ui.eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, &ui);
			}
			break;

		case EventType::Hit:
			player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
			for (auto& ui : playerHpUIs_) {
				ui.eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, &ui);
			}
			break;

		case EventType::MuzzleFlash: {
			auto& muzzleFlash = muzzleFlashes_.emplace_back();
			muzzleFlash.init(assetManager_.muzzleFlashAnimation());
			muzzleFlash.setTint(mu::Vec3(0.8f, 0.4f, 0.1f));
			break;
		}

		case EventType::Blood: {
			auto& bloodSplash = bloodSplashes_.emplace_back();
			bloodSplash.init(assetManager_.muzzleFlashAnimation());
			bloodSplash.setSpeed(0.75f);
			bloodSplash.setTint(mu::Vec3(0.92f, 0.04f, 0.01f));
			break;
		}

		case EventType::Reloading:
			reloading_ = true;
			break;

		case EventType::ReloadComplete:
			reloading_ = false;
			for (auto& ui : playerHpUIs_) {
				ui.eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, &ui);
			}

		default:
			break;
		}
	}

	// 물리 업데이트 루틴
	// 
	// 물리량 갱신은 게임 갱신과 다르게 고정 주기로 수행한다.
	// 이를 통해 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	//
	// update 함수에서 physicUpdateAcc_ 변수를 통해
	// 물리량 갱신의 주기가 돌아왔는지 판단하고
	// 주기가 되었다면 물리량 갱신을 수행한다.
	physicUpdateAcc_ += deltaTime;

	if (physicUpdateAcc_ >= physicUpdateInterval) {
		// 물리 시뮬레이션을 위해
		// 물리 시뮬레이션의 대상이 되는 객체들을
		// 한 곳에 모아 PhysicSystem 객체에 전달한다.
		static std::vector<Object*> allObjects{};
		allObjects.resize(cubes_.size() + 1u);
		std::ranges::transform(cubes_, allObjects.begin(),
			[](Object& cube) { return &cube; }	
		);
		allObjects[cubes_.size()] = player_.get();

		while (physicUpdateAcc_ >= physicUpdateInterval) {
			physicSystem_.step(allObjects, physicUpdateInterval);
			physicUpdateAcc_ -= physicUpdateInterval;
		}

		allObjects.clear();
	}

	// 객체별 업데이트 루틴
	//
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / physicUpdateInterval;


	for ( auto& cube : cubes_ ) {
		cube.update(deltaTime, tPhysicInterpolation);
	}
	player_->update(deltaTime, tPhysicInterpolation);
	camera_.update();
	dirLight_.update(deltaTime);
	dirLight_.updateShadowAuxDirectional(camera_.eye(), 100.f, -10.f, 10.f, -10.f, 10.f, 50.f, 200.f);

	// 애니메이션 업데이트
	slimeSprite_.update( deltaTime );
	for ( auto& hpUI : playerHpUIs_ ) {
		hpUI.update( deltaTime, gfx_, nullptr );
	}

	for (auto& muzzleFlash : muzzleFlashes_) {
		// 총구 화염 스프라이트 애니메이션
		// 플레이어에 대해서 오프셋 지정
		muzzleFlash.setPos( player_->pos()
			+ player_->up() * 1.3f
			+ player_->forward() * 0.85f
			+ player_->right() * 0.15f
		);
		muzzleFlash.update(deltaTime);
	}

	std::erase_if(muzzleFlashes_, [](const SpriteAnimation& anim) { return anim.done(); });

	for (auto& bloodSplash : bloodSplashes_) {
		bloodSplash.setPos( player_->pos()
			+ player_->up() * 1.25f
			- player_->forward() * 0.1f
		);
		bloodSplash.update(deltaTime);
	}

	std::erase_if(bloodSplashes_, [](const SpriteAnimation& anim) { return anim.done(); });

	animSystem_.update(0.016s);

	// 총 발사 쿨타임 계산
	if (fireCooldown_ > 0ms) {
		fireCooldown_ -= deltaTime;
	}

	clearEvents(eventList_);
}

void Game::render() {
	for ( auto& cube : cubes_ ) {
		cube.render( gfx_ );
	}
	player_->render(gfx_);
	skybox_.render(gfx_);
	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);
	// billboard_.render( gfx_ );
	slimeSprite_.render( gfx_); 
	for (const auto& muzzleFlash : muzzleFlashes_) {
		muzzleFlash.render(gfx_);
	}
	for (const auto& bloodSplash : bloodSplashes_) {
		bloodSplash.render(gfx_);
	}

	for ( auto& hpUI : playerHpUIs_ ) {
		hpUI.render( gfx_ );
	}

	auto frameDataPBR = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBR );
	auto frameDataPBRSkinned = PBRSkinnedPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBRSkinned );

	auto frameData1 = UIPipeline::FrameData{
		.screenWidth = static_cast<float>( gClientRect.right - gClientRect.left ),
		.screenHeight = static_cast<float>( gClientRect.bottom - gClientRect.top )
	};
	gfx_.addFrameData( frameData1 );

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
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult != -1, true);

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, true);

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// 마우스에 대한 입력 내용이 상대 좌표여야 한다.
			DISPLAY_ERROR_STR( !(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
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

	// Alt+Tab 등으로 윈도우가 포커스를 잃었다가 되찾은 경우,
	// 커서와 관련된 플래그들을 읽어 커서 캡처, 커서 숨기기 등을 다시 수행한다.
	case WM_SETFOCUS:
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		if (!cursorShowEnabled_) {
			hideCursor();
		}
		break;

	// Alt+Tab 등으로 윈도우가 포커스를 잃은 경우
	// 커서와 관련된 플래그들을 읽어 커서 캡처 해제, 커서 보이기 등을 수행한다.
	// 다른 윈도우로 전환되었는데 커서가 보이지 않거나 안 움직여지면 곤란할 것이다.
	case WM_KILLFOCUS:
		if (cursorCaptureEnabled_) {
			releaseCursor();
		}
		if (cursorShowEnabled_) {
			showCursor();
		}
		break;

	case WM_SIZE:
		break;

	default:
		break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
};

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

	// 카메라 1인칭 모드 설정
	if ( keyboardStateCurr_['1'] & 0x80 ) {
		camera_.setOffsetFromTargetPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 2.f, 0.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 2.f, 8.f));
		cameraMode_ = CameraMode::FirstPerson;
	} 
	// 카메라 3인칭 모드 설정
	if ( keyboardStateCurr_['3'] & 0x80 ) {
		camera_.setXXPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
		cameraMode_ = CameraMode::ThirdPerson;
	}

	// 총 장전
	if ( keyboardStateCurr_['R'] & 0x80 ) {
		if (!reloading_) {
			holdEvent(eventList_, EvReloading{});
			pTimer_->enqueueJob( DelayedJob{
				.job = [this](){ holdEvent(eventList_, EvReloadComplete{}); },
				.executeAt = pTimer_->lastTp() + 2s
			} );
		}
	}

	// 총 발사: 총구 화염 애니메이션 재생
	if ( (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80)
	) {
		if (!reloading_) {
			fireCooldown_ = 200ms;
			holdEvent(eventList_, EvFire{});
		}
	}

	// 임시: 피격, 피격 애니메이션 및 이펙트 재생
	if ( (keyboardStateCurr_[VK_RBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_RBUTTON] & 0x80)
	) {
		holdEvent(eventList_, EvHit{});
	}

	// 마우스 민감도를 기반으로 1인칭 카메라 모드와 3인칭 카메라 모드일 때
	// 각각의 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
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

		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );

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

		camera_.setXXPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );

		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		break;
	}
	}

	// Enter 키를 누르면 커서 캡처 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_RETURN] & 0x80) && !(keyboardStatePrev_[VK_RETURN] & 0x80) ) {
		cursorCaptureEnabled_ = !cursorCaptureEnabled_;
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		else {
			releaseCursor();
		}
	}

	// Space 키를 누르면 커서 보이기 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_SPACE] & 0x80) && !(keyboardStatePrev_[VK_SPACE] & 0x80) ) {
		cursorShowEnabled_ = !cursorShowEnabled_;
		if (cursorShowEnabled_) {
			showCursor();
		}
		else {
			hideCursor();
		}
	}
}

// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
void Game::captureCursor() {
    auto ul = POINT{ gClientRect.left, gClientRect.top };
    auto lr = POINT{ gClientRect.right, gClientRect.bottom };

    // 클라이언트의 외곽 좌표를 윈도우 좌표로 변환
    MapWindowPoints(ghWnd, nullptr, &ul, 1);
    MapWindowPoints(ghWnd, nullptr, &lr, 1);

    auto clipRect = RECT{ ul.x, ul.y, lr.x, lr.y };

    ClipCursor(&clipRect);
}

// 커서 캡처가 설정되어있다면 해제한다.
// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
void Game::releaseCursor() {
	ClipCursor(nullptr);
}

void Game::hideCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(false) >= 0) {}
}

void Game::showCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(true) < 0) {}
}

}	// namespace StandAlone