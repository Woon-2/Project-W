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
	threadCnt = 4u;
	// std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);

	assetManager_.loadGFXAssets(gfx_);
	assetManager_.loadAnimations();

	emitterConfig_.pClip       = assetManager_.flameAnimation();
	emitterConfig_.position    = { 0.f, 1.f, -3.f };
	emitterConfig_.direction   = { 0.f, 1.f, 0.f };
	emitterConfig_.spread      = 0.0f;
	emitterConfig_.speedMin    = 2.f;
	emitterConfig_.speedMax    = 5.f;
	emitterConfig_.lifetimeMin = 0.5f;
	emitterConfig_.lifetimeMax = 1.5f;
	emitterConfig_.sizeEnd = 0.f;
	emitterConfig_.tintBegin = { 1.f, 0.4f, 0.0f };
	emitterConfig_.tintEnd = { 0.3f, 0.1f, 0.0f };
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path, std::ios::binary);
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

	//for (auto& cube : cubes_) {
	//	cube.enableBVRendering();
	//}

	player_->enableBVRendering();
	goblin_->enableBVRendering();
	anubis_->enableBVRendering();
	bat_->enableBVRendering();
	bomber_->enableBVRendering();
	demon_->enableBVRendering();
	dragon_->enableBVRendering();
	eyeball_->enableBVRendering();
	fishman_->enableBVRendering();
	gargoyle_->enableBVRendering();

	//playerHpUI_.setTexture( assetManager_.playerHpLine() );
	//playerHpUI_.setTextImage( assetManager_.textPlayerHp() );
	//playerHpUI_.setHp( player_->hp() );
	//playerHpUI_.setAmmo( player_->ammo() );
	playerHpUI_.setPivot( mu::Vec2(512.f, 768.f - 40.f) );
	playerHpUI_.setScale( mu::Vec2(1024.f, 64.f) );

	// 전투 시스템에 참가자 등록
	// 플레이어: 공격 hitbox 및 데미지 설정 (AI 쿨타임은 사용하지 않음)
	combatSystem_.registerCombatant(player_.get(),   { {1.5f, 1.5f, 1.5f}, 1.0f, 30, 500ms  });
	// 몬스터: 종류별로 공격 범위·데미지·쿨타임 차등 적용
	combatSystem_.registerCombatant(goblin_.get(),   { {1.2f, 1.5f, 1.2f}, 0.8f, 15, 2000ms });
	combatSystem_.registerCombatant(anubis_.get(),   { {1.5f, 2.0f, 1.5f}, 1.0f, 25, 3000ms });
	combatSystem_.registerCombatant(bat_.get(),      { {0.8f, 0.8f, 0.8f}, 0.6f, 10, 1500ms });
	combatSystem_.registerCombatant(bomber_.get(),   { {1.5f, 1.5f, 1.5f}, 1.2f, 30, 4000ms });
	combatSystem_.registerCombatant(demon_.get(),    { {1.5f, 2.0f, 1.5f}, 1.0f, 20, 2500ms });
	combatSystem_.registerCombatant(dragon_.get(),   { {2.5f, 2.0f, 2.5f}, 1.5f, 40, 5000ms });
	combatSystem_.registerCombatant(eyeball_.get(),  { {1.2f, 1.2f, 1.2f}, 1.0f, 15, 2000ms });
	combatSystem_.registerCombatant(fishman_.get(),  { {1.2f, 1.8f, 1.2f}, 0.9f, 20, 2500ms });
	combatSystem_.registerCombatant(gargoyle_.get(), { {1.5f, 2.0f, 1.5f}, 1.0f, 25, 3000ms });
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
		auto tmp = std::make_shared<Cube>(std::move(object));
		tmp->setModel(assetManager_.modelCube());
		importCube(ifs, *tmp);
	}
	else if (type == "PlayerStart") {
		if (!playerSpawned_) {
			player_ = std::make_shared<Player>(std::move(object));
			playerSpawned_ = true;
			importPlayerStart(ifs, *player_);
		}
	}
	else if (type == "GoblinSpawner") {
		goblin_ = std::make_shared<Goblin>(std::move(object));
		importGoblinSpawner(ifs, *goblin_);
	}
	else if (type == "AnubisSpawner") {
		anubis_ = std::make_shared<Anubis>(std::move(object));
		importAnubisSpawner(ifs, *anubis_);
	}
	else if (type == "BatSpawner") {
		bat_ = std::make_shared<Bat>(std::move(object));
		importBatSpawner(ifs, *bat_);
	}
	else if (type == "BomberSpawner") {
		bomber_ = std::make_shared<Bomber>(std::move(object));
		importBomberSpawner(ifs, *bomber_);
	}
	else if (type == "DemonSpawner") {
		demon_ = std::make_shared<Demon>(std::move(object));
		importDemonSpawner(ifs, *demon_);
	}
	else if (type == "DragonSpawner") {
		dragon_ = std::make_shared<Dragon>(std::move(object));
		importDragonSpawner(ifs, *dragon_);
	}
	else if (type == "EyeballSpawner") {
		eyeball_ = std::make_shared<Eyeball>(std::move(object));
		importEyeballSpawner(ifs, *eyeball_);
	}
	else if (type == "FishmanSpawner") {
		fishman_ = std::make_shared<Fishman>(std::move(object));
		importFishmanSpawner(ifs, *fishman_);
	}
	else if (type == "GargoyleSpawner") {
		gargoyle_ = std::make_shared<Gargoyle>(std::move(object));
		importGargoyleSpawner(ifs, *gargoyle_);
	}
	else if (type == "Terrain") {
		terrain_ = std::make_shared<TerrainObject>(std::move(object));
		importTerrain(ifs, *terrain_);
		terrain_->update(0ms, 1.f);
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

void Game::importCube(std::ifstream& ifs, Cube& cube) {
	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void Game::importPlayerStart(std::ifstream& ifs, Player& player) {
	player.setModel(assetManager_.modelPlayer());
	player.setAnimBlender(animSystem_, assetManager_);
	player.setHp(100);

	//Equipment rifle{};
	//rifle.socketType = Bone::SocketType::RightHand;
	//rifle.object = std::make_unique<Object>();
	//rifle.object->setModel(assetManager_.modelRifle());
	//rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

	//player.equip(std::move(rifle));
}

void Game::importGoblinSpawner(std::ifstream& ifs, Goblin& goblin) {
	goblin.setModel(assetManager_.modelGoblin());
	goblin.setAnimBlender(animSystem_, assetManager_);
	goblin.setHp(90);
	goblin.setId(1);
}

void Game::importAnubisSpawner(std::ifstream& ifs, Anubis& anubis) {
	anubis.setModel(assetManager_.modelAnubis());
	anubis.setAnimBlender(animSystem_, assetManager_);
	anubis.setHp(110);
	anubis.setId(2);
}

void Game::importBatSpawner(std::ifstream& ifs, Bat& bat) {
	bat.setModel(assetManager_.modelBat());
	bat.setAnimBlender(animSystem_, assetManager_);
	bat.setHp(40);
	bat.setId(3);
}

void Game::importBomberSpawner(std::ifstream& ifs, Bomber& bomber) {
	bomber.setModel(assetManager_.modelBomber());
	bomber.setAnimBlender(animSystem_, assetManager_);
	bomber.setHp(60);
	bomber.setId(4);
}

void Game::importDemonSpawner(std::ifstream& ifs, Demon& demon) {
	demon.setModel(assetManager_.modelDemon());
	demon.setAnimBlender(animSystem_, assetManager_);
	demon.setHp(140);
	demon.setId(5);
}

void Game::importDragonSpawner(std::ifstream& ifs, Dragon& dragon) {
	dragon.setModel(assetManager_.modelDragon());
	dragon.setAnimBlender(animSystem_, assetManager_);
	dragon.setHp(250);
	dragon.setId(6);
}

void Game::importEyeballSpawner(std::ifstream& ifs, Eyeball& eyeball) {
	eyeball.setModel(assetManager_.modelEyeball());
	eyeball.setAnimBlender(animSystem_, assetManager_);
	eyeball.setHp(120);
	eyeball.setId(7);
}

void Game::importFishmanSpawner(std::ifstream& ifs, Fishman& fishman) {
	fishman.setModel(assetManager_.modelFishman());
	fishman.setAnimBlender(animSystem_, assetManager_);
	fishman.setHp(80);
	fishman.setId(8);
}

void Game::importGargoyleSpawner(std::ifstream& ifs, Gargoyle& gargoyle) {
	gargoyle.setModel(assetManager_.modelGargoyle());
	gargoyle.setAnimBlender(animSystem_, assetManager_);
	gargoyle.setHp(160);
	gargoyle.setId(9);
}

void Game::importTerrain(std::ifstream& ifs, TerrainObject& terrain) {
	const auto manifestPath = readText(ifs, "ManifestPath");
	terrain.setTerrainData(assetManager_.terrain());
}

// 게임의 업데이트는 다음 순서대로 이루어진다.
// 입력 처리
// 이벤트 처리
// 물리 업데이트 루틴
// 객체별 업데이트 루틴
// 애니메이션 업데이트
void Game::update(Milliseconds deltaTime) {
	// 평가 물리량 초기화
	player_->physicState().evVelocity = mu::Vec3();
	player_->physicState().evOmega = mu::Vec3();
	goblin_->physicState().evVelocity = mu::Vec3();
	goblin_->physicState().evOmega = mu::Vec3();
	anubis_->physicState().evVelocity = mu::Vec3();
	anubis_->physicState().evOmega = mu::Vec3();
	bat_->physicState().evVelocity = mu::Vec3();
	bat_->physicState().evOmega = mu::Vec3();
	bomber_->physicState().evVelocity = mu::Vec3();
	bomber_->physicState().evOmega = mu::Vec3();
	demon_->physicState().evVelocity = mu::Vec3();
	demon_->physicState().evOmega = mu::Vec3();
	dragon_->physicState().evVelocity = mu::Vec3();
	dragon_->physicState().evOmega = mu::Vec3();
	eyeball_->physicState().evVelocity = mu::Vec3();
	eyeball_->physicState().evOmega = mu::Vec3();
	fishman_->physicState().evVelocity = mu::Vec3();
	fishman_->physicState().evOmega = mu::Vec3();
	gargoyle_->physicState().evVelocity = mu::Vec3();
	gargoyle_->physicState().evOmega = mu::Vec3();

	// 입력 처리
	processInput(deltaTime);

	// 평가 물리량 갱신
	player_->physicState().evVelocity += player_->physicState().velocity;
	player_->physicState().evOmega += player_->physicState().omega;

	// debug BV 갱신 (TTL 감소 + 소멸 조건 평가)
	debugBVView_.update(deltaTime);

	// 몬스터 AI 공격 처리
	// 쿨타임 감소 후 플레이어와 AABB 교차 시 EvAttack + EvHit 발생
	if (!playerDead_) {
		combatSystem_.update(deltaTime, player_->getId(), eventList_);
	}

	// 이벤트 처리
	for (auto pEvRaw : eventList_) {
		auto pEv = reinterpret_cast<BasicEvent*>(pEvRaw);
		switch (pEv->type) {
		case EventType::Hit:
			if (static_cast<EvHit*>(pEv)->targetId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
				if (goblin_->hp() == 0) {
					holdEvent( eventList_, EvDeath(goblin_->getId()) );
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
				if (anubis_->hp() == 0) {
					holdEvent( eventList_, EvDeath(anubis_->getId()) );
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
				if (bat_->hp() == 0) {
					holdEvent(eventList_, EvDeath(bat_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
				if (bomber_->hp() == 0) {
					holdEvent(eventList_, EvDeath(bomber_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
				if (demon_->hp() == 0) {
					holdEvent(eventList_, EvDeath(demon_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
				if (dragon_->hp() == 0) {
					holdEvent(eventList_, EvDeath(dragon_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
				if (eyeball_->hp() == 0) {
					holdEvent(eventList_, EvDeath(eyeball_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
				if (fishman_->hp() == 0) {
					holdEvent( eventList_, EvDeath(fishman_->getId()) );
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
				if (gargoyle_->hp() == 0) {
					holdEvent(eventList_, EvDeath(gargoyle_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
				if (player_->hp() <= 0 && !playerDead_) {
					playerDead_ = true;
					holdEvent(eventList_, EvDeath(player_->getId()));
				}
			}
			break;

		case EventType::Attack: {
			auto* attack = static_cast<EvAttack*>(pEv);
			if (attack->attackerId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}
			else if (attack->attackerId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
			}
			else if (attack->attackerId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
			}
			else if (attack->attackerId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
			}
			else if (attack->attackerId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
			}
			else if (attack->attackerId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
			}
			else if (attack->attackerId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
			}
			else if (attack->attackerId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
			}
			else if (attack->attackerId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
			}

			// 공격 발동 시 attack hitbox를 1500ms 동안 live 추적으로 렌더링
			if (auto spec = combatSystem_.queryAttackSpec(attack->attackerId)) {
				debugBVView_.pushLive(spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms);
			}
		}
		break;

		case EventType::Death: {
			auto* death = static_cast<EvDeath*>(pEv);

			if (death->victimId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}
			else if (death->victimId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
			}
			else if (death->victimId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
			}
			else if (death->victimId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
			}
			else if (death->victimId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
			}
			else if (death->victimId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
			}
			else if (death->victimId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
			}
			else if (death->victimId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
			}
			else if (death->victimId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
			}
			else if (death->victimId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
			}
			break;
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
		static std::vector<Object*> targetObjects{};
		targetObjects.resize(10u);
		targetObjects[0] = player_.get();
		targetObjects[1] = goblin_.get();
		targetObjects[2] = anubis_.get();
		targetObjects[3] = bat_.get();
		targetObjects[4] = bomber_.get();
		targetObjects[5] = demon_.get();
		targetObjects[6] = dragon_.get();
		targetObjects[7] = eyeball_.get();
		targetObjects[8] = fishman_.get();
		targetObjects[9] = gargoyle_.get();

		while (physicUpdateAcc_ >= physicUpdateInterval) {
			physicSystem_.step(targetObjects, physicUpdateInterval);
			physicUpdateAcc_ -= physicUpdateInterval;
		}

		targetObjects.clear();
	}

	// 객체별 업데이트 루틴
	//
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / physicUpdateInterval;

	player_->update(deltaTime, tPhysicInterpolation);
	goblin_->update(deltaTime, tPhysicInterpolation);
	anubis_->update(deltaTime, tPhysicInterpolation);
	bat_->update(deltaTime, tPhysicInterpolation);
	bomber_->update(deltaTime, tPhysicInterpolation);
	demon_->update(deltaTime, tPhysicInterpolation);
	dragon_->update(deltaTime, tPhysicInterpolation);
	eyeball_->update(deltaTime, tPhysicInterpolation);
	fishman_->update(deltaTime, tPhysicInterpolation);
	gargoyle_->update(deltaTime, tPhysicInterpolation);
	camera_.update();
	dirLight_.update(deltaTime);
	dirLight_.updateShadowAuxDirectional(camera_.eye(), 400.f, -300.f, 300.f, -300.f, 300.f, 50.f, 800.f);

	// playerHpUI_.update( deltaTime, gfx_, nullptr );

	// 애니메이션 업데이트
	animSystem_.update(0.016s);

	// 파티클
	particleSystem_.update( deltaTime );

	// UI 동기화
	// playerHpUI_.setHp(player_->hp());

	clearEvents(eventList_);
}

void Game::render() {
	debugBVView_.render(gfx_);
	player_->render(gfx_);
	goblin_->render(gfx_);
	anubis_->render(gfx_);
	bat_->render(gfx_);
	bomber_->render(gfx_);
	demon_->render(gfx_);
	dragon_->render(gfx_);
	eyeball_->render(gfx_);
	fishman_->render(gfx_);
	gargoyle_->render(gfx_);
	skybox_.render(gfx_);
	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	particleSystem_.render( gfx_ );

	playerHpUI_.render( gfx_ );

	auto frameDataPBR = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBR );
	auto frameDataPBRSkinned = PBRSkinnedPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBRSkinned );

	if (terrain_) {
		terrain_->render(gfx_);
		gfx_.addCameraData(TerrainPipeline::CameraData{
			.view = camera_.view(),
			.proj = camera_.proj(),
			.pos  = camera_.eye()
		});
		gfx_.addLightData(TerrainPipeline::LightData{
			.dir       = mu::Vec3(dirLight_.dir()),
			.color     = dirLight_.color,
			.intensity = dirLight_.intensity,
			.view      = dirLight_.shadowView(),
			.proj      = dirLight_.shadowProj()
		});
		auto terrainIdxShadowMap = BindlessIndex{};
		if (SharedResources::ShadowMap::shadowMapData.contains("ShadowMap")) {
			terrainIdxShadowMap = SharedResources::ShadowMap::shadowMapData.at("ShadowMap").tex.idxSrv;
		}
		gfx_.addFrameData(TerrainPipeline::FrameData{
			.globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f),
			.idxShadowMap  = terrainIdxShadowMap
		});
	}

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
	// + 플레이어가 죽으면 움직이지 않는다.

	const auto moveXSign = !playerDead_ * ( (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80) );
	const auto moveZSign = !playerDead_ * ( (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80) );
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

	// 플레이어 공격: LButton 클릭 시 forward 방향 hitbox와 몬스터 AABB 교차 검사
	if ( !playerDead_
		&& (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80)
	) {
		combatSystem_.onPlayerAttack(player_->getId(), eventList_);
		// 공격 발동 시 플레이어 attack hitbox를 1500ms 동안 live 추적으로 렌더링
		if (auto spec = combatSystem_.queryAttackSpec(player_->getId())) {
			debugBVView_.pushLive(spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms);
		}
	}

	// 마우스 민감도를 기반으로 1인칭 카메라 모드와 3인칭 카메라 모드일 때
	// 각각의 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
    const auto mouseSensitivity = mu::pi * 2.f;

	switch (cameraMode_) {
	case CameraMode::ThirdPerson: {
		const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
		auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

		cameraPitch_ = std::clamp(
			static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
			-mu::pi * 0.16f,
			mu::pi * 0.3f
		);

		if (!playerDead_) {
			player_->setOrient(player_->orient() * yawRotation);
			camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
		}
		else {
			cameraYaw_ += yaw;
			camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
		}

		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		break;
	}
	case CameraMode::FirstPerson: {
		const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
		auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

		if (!playerDead_) {
			player_->setOrient(player_->orient() * yawRotation);
		}

		cameraPitch_ = std::clamp(
			static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
			-mu::pi * 0.16f,
			mu::pi * 0.3f
		);

		if (!playerDead_) {
			player_->setOrient(player_->orient() * yawRotation);
			camera_.setXXPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
		}
		else {
			cameraYaw_ += yaw;
			camera_.setXXPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
		}

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

	// F key: emit particles for testing
	if ( (keyboardStateCurr_['F'] & 0x80) && !(keyboardStatePrev_['F'] & 0x80) ) {
		emitterConfig_.position = player_->pos()
		                        + player_->right()   * 1.0f
		                        + player_->forward() * 1.5f;
		particleSystem_.emit(emitterConfig_, 5);
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