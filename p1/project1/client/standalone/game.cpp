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
	gfx_.setThreadPool(&threadPool_);

	assetManager_.loadModels(gfx_);
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");
	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(60.f), mu::Degree(15.f)));
	dirLight_.color = mu::Vec3(0.8f, 0.8f, 0.8f);
	dirLight_.intensity = 1.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 3.2f, -3.5f ) );
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);
}

void Game::importNode(std::ifstream& ifs) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	std::cout << "[Level Load] 레벨 노드 " << name << " 로드\n";

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

	for ( auto& cube : cubes_ ) {
		cube.update( deltaTime );
	}
	player_->update(deltaTime);
	camera_.update();
	dirLight_.update(deltaTime);
}

void Game::render() {
	for ( auto& cube : cubes_ ) {
		cube.render( gfx_ );
	}
	player_->render(gfx_);
	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	auto frameData = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameData );

	gfx_.render();
}

void Game::processInput(Milliseconds deltaTime) {
	if ( GetAsyncKeyState('W') & 0x8000 ) {
		player_->setPos( player_->pos( ) + player_->forward() * 0.01f );
	}
	if ( GetAsyncKeyState('A') & 0x8000 ) {
		player_->setPos( player_->pos( ) - player_->right() * 0.01f );
	}
	if ( GetAsyncKeyState('S') & 0x8000 ) {
		player_->setPos( player_->pos( ) - player_->forward() * 0.01f );
	}
	if ( GetAsyncKeyState('D') & 0x8000 ) {
		player_->setPos( player_->pos( ) + player_->right() * 0.01f );
	}
}

}	// namespace StandAlone