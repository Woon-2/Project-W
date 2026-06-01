#include "rspch.hpp"
#include "AssetManager.hpp"

void AssetManager::loadAssets() {
    modelCube_   = loadModelFromFile("../resources/models/cube/cubeServer.bin");
    modelPlayer_ = loadModelFromFile("../resources/models/player/playerServer.bin");
    modelGoblin_ = loadModelFromFile("../resources/models/goblin/goblinServer.bin");

    playerAnimations_ = loadServerAnimClipsFromFile("../resources/animations/playerAnimations.anim");
    goblinAnimations_ = loadServerAnimClipsFromFile("../resources/animations/goblinAnimations.anim");

    level_ = loadLevelFromFile("../resources/levels/level.bin", *this);
}

const ServerAnimClip* AssetManager::findClip(const std::vector<ServerAnimClip>& set,
                                              std::string_view name) {
    for (const auto& clip : set)
        if (clip.name == name) return &clip;
    return nullptr;
}
