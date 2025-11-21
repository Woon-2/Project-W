#include "game.hpp"

#include "../errorHandling.hpp"
#include "../binaryImport.hpp"

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
	// gfx_.setThreadPool(&threadPool_);

	assetManager_.loadGFXAssets(gfx_);
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

	slimeSprite_.setTexture( assetManager_.slimeSprites() );
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

	player_ = std::make_shared<Object>(std::move(player));
	player_->setModel(assetManager_.modelPlayer());
}

void Game::update(Milliseconds deltaTime) {
	processInput(deltaTime);

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
	//  billboard_->update( deltaTime );
	slimeSprite_.update( deltaTime );
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

	case WM_SIZE:
		return DefWindowProcA(hWnd, msg, wParam, lParam);

	default:
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
};

void Game::processInput(Milliseconds deltaTime) {
	DISPLAY_ERROR_GLE( GetKeyboardState(keyboardState_.data()), false );

	if ( keyboardState_['W'] & 0x80 ) {
		player_->setPos( player_->pos( ) + player_->forward() * 0.01f );
	}
	if ( keyboardState_['A'] & 0x80 ) {
		player_->setPos( player_->pos( ) - player_->right() * 0.01f );
	}
	if ( keyboardState_['S'] & 0x80 ) {
		player_->setPos( player_->pos( ) - player_->forward() * 0.01f );
	}
	if ( keyboardState_['D'] & 0x80 ) {
		player_->setPos( player_->pos( ) + player_->right() * 0.01f );
	}
	if ( keyboardState_['1'] & 0x80 ) {
		camera_.setOffsetFromTargetPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 2.f, 0.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 2.f, 8.f));
		cameraMode_ = CameraMode::FirstPerson;
	} 
	if ( keyboardState_['3'] & 0x80 ) {
		camera_.setXXPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
		cameraMode_ = CameraMode::ThirdPerson;
	} 

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
}

}	// namespace StandAlone