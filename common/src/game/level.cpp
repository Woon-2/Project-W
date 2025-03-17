#include "game/level.hpp"

#include <cstring>
#include <cstdint>

namespace gameEngine {

ObjectDisposition::ObjectDisposition(std::ifstream& is)
    : xform_(), name_(), prefabName_(), children_() {
    char pstrToken[64] = { '\0' };
    std::uint8_t nStrLength = 0;
    dx::XMFLOAT4X4 xform{};
    bool isInstance = false;
    int childCnt = 0;

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(std::uint8_t));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Node:>")) {
        throw std::runtime_error("Node token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(std::uint8_t));
    name_.resize(nStrLength);
    is.read(reinterpret_cast<char*>(name_.data()), nStrLength);

    is.read(reinterpret_cast<char*>(&xform), sizeof(dx::XMFLOAT4X4));
    xform_ = mu::Mat4x4( dx::XMLoadFloat4x4(&xform) );

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(std::uint8_t));
    prefabName_.resize(nStrLength);
    is.read(reinterpret_cast<char*>(prefabName_.data()), nStrLength);

    is.read(reinterpret_cast<char*>(&isInstance), sizeof(bool));
    if (!isInstance) {
        prefabName_.clear();
    }

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(std::uint8_t));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Children:>")) {
        throw std::runtime_error("Children token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&childCnt), sizeof(int));

    for (int i = 0; i < childCnt; ++i) {
        children_.emplace_back(is);
    }

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(std::uint8_t));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "</Node>")) {
        throw std::runtime_error("Node end token expected but got: " + std::string(pstrToken));
    }
}

void CoordRoot::addEntity(ecs::Entity& entity) {
    ecs::System<Coord>::addEntity(entity);
    auto pCoord = entity.get<Coord>();
    if (!pCoord) {
        throw ECS_EXCEPT("Entity does not have a Coord component");
    }
    
    pCoord->get().setParent(&rootCoordSys_);
}


LevelRegion::LevelRegion(const std::filesystem::path& path)
    : ecs::Entity(), pStream_(std::make_unique<std::ifstream>(path, std::ios::binary)), dispositionRoot_(*pStream_) {}

std::vector<ecs::Entity> LevelRegion::instantiateAllObjects(gfx::coord::System& coordRoot) {
    auto ret = std::vector<ecs::Entity>();

    instantiateObjectHierarchy(std::nullopt, dispositionRoot_, coordRoot, ret);

    return ret;
}

void LevelRegion::instantiateObjectHierarchy( std::optional<std::size_t> parentIdx,
    const ObjectDisposition& disposition, gfx::coord::System& coordRoot,
    std::vector<ecs::Entity>& out
) {
    if (disposition.prefabName_.empty()) {
        for (auto& child : disposition.children_) {
            instantiateObjectHierarchy(parentIdx, child, coordRoot, out);
        }
        return;
    }

    const auto modelKey = disposition.prefabName_.substr(2);

    auto obj = ecs::Entity();

    obj.createComponent<Coord>();
    obj.as<Coord>().get().setLocalXform(disposition.xform_);
    obj.as<Coord>().get() << mu::translate(0.f, -25.f, 0.f);

    if (parentIdx.has_value()) {
        obj.as<Coord>().get().setParent(&out[parentIdx.value()].as<Coord>().get());
    }
    else {
        obj.as<Coord>().get().setParent(&coordRoot);
    }

    out.push_back(std::move(obj));
    std::size_t myIdx = out.size() - 1;

    for (auto& child : disposition.children_) {
        instantiateObjectHierarchy(myIdx, child, coordRoot, out);
    }
}

} // namespace gameEngine