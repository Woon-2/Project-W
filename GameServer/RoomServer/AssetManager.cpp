#include "rspch.hpp"
#include "AssetManager.hpp"
#include "skill/skillCompiler.hpp"

void AssetManager::loadAssets() {
    modelCube_     = loadModelFromFile("../resources/models/cube/cubeServer.bin");
    modelPlayer_   = loadModelFromFile("../resources/models/player/playerServer.bin");
    modelGoblin_   = loadModelFromFile("../resources/models/goblin/goblinServer.bin");
    modelHobgoblin_ = loadModelFromFile("../resources/models/goblin/HobgoblinServer.bin");
    modelSnake_    = loadModelFromFile("../resources/models/snake/snakeServer.bin");
    modelMushroom_ = loadModelFromFile("../resources/models/mushroom/mushroomServer.bin");
    modelBomber_   = loadModelFromFile("../resources/models/bomber/bomberServer.bin");
    modelBirdy_    = loadModelFromFile("../resources/models/birdy/birdyServer.bin");
    modelSlime_    = loadModelFromFile("../resources/models/slime/slimeServer.bin");
    modelTreant_   = loadModelFromFile("../resources/models/treant/treantServer.bin");
    // Named variants: share base monster anims, different model only.
    modelGrandbaum_ = loadModelFromFile("../resources/models/treant/GrandbaumServer.bin");
    modelIsys_      = loadModelFromFile("../resources/models/birdy/IsysServer.bin");

    playerAnimations_   = loadServerAnimClipsFromFile("../resources/animations/playerAnimations.anim");
    goblinAnimations_   = loadServerAnimClipsFromFile("../resources/animations/goblinAnimations.anim");
    snakeAnimations_    = loadServerAnimClipsFromFile("../resources/animations/snakeAnimations.anim");
    mushroomAnimations_ = loadServerAnimClipsFromFile("../resources/animations/mushroomAnimations.anim");
    bomberAnimations_   = loadServerAnimClipsFromFile("../resources/animations/bomberAnimations.anim");
    birdyAnimations_    = loadServerAnimClipsFromFile("../resources/animations/birdyAnimations.anim");
    slimeAnimations_    = loadServerAnimClipsFromFile("../resources/animations/slimeAnimations.anim");
    treantAnimations_   = loadServerAnimClipsFromFile("../resources/animations/treantAnimations.anim");

    level_ = loadLevelFromFile("../resources/levels/level.bin", *this);
    level_.terrainChunks.init("../resources/terrains/");

    // 스킬 asset은 사양이 방마다 달라지지 않으므로 부팅 시 1회만 컴파일하여
    // 전 룸이 공유한다(각 Room::SkillSystem은 이 레지스트리를 참조만 한다).
    {
        ServerSkillCompiler compiler;
        skillAssets_ = compiler.compileAll("../resources/skills");
        // VFXParticle 히트박스용 게임플레이 설정(effect JSON + lua 오버라이드)을
        // 부팅 시 1회 빌드해 전 룸이 공유한다 (shared_ptr, 불변).
        buildVfxGameplayConfigs(skillAssets_, "../resources");
        skillAssets_.shrink_to_fit();
        // id가 지정되지 않은(0) asset에 1부터 순번 부여. 공유 레지스트리가
        // 바인딩 전에 id 확정 상태가 되도록 여기서 1회만 수행한다.
        for (u32t i = 0; i < static_cast<u32t>(skillAssets_.size()); ++i)
            if (skillAssets_[i].id == 0)
                skillAssets_[i].id = i + 1;
        std::cout << "[AssetManager] Loaded " << skillAssets_.size() << " skill(s)\n";
    }

    // 무기별 스킬 로드아웃(기본공격 + 3슬롯 + 코스트)을 스킬 메타에서 1회 빌드.
    loadout_ = SkillLoadout::build(skillAssets_);

    // 스택 충전 경제 튜닝(몬스터별 charge, 콤보, 소프트캡)을 부팅 시 1회 로드.
    chargeConfig_.load("../resources/data/chargeConfig.lua");
}

const ServerAnimClip* AssetManager::findClip(const std::vector<ServerAnimClip>& set,
                                              std::string_view name) {
    for (const auto& clip : set)
        if (clip.name == name) return &clip;
    return nullptr;
}
