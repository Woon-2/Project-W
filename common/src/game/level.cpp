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

void MU_CALLCONV Coord::accTranslation(mu::Vec3 deltaPos) {
    // 2-3: x, 4-5: y, 6-7: z, precision: 0.0003m
    static constexpr auto precision = 0.0003f;
    const auto oldV = compressedDeltaPos_.load();

    auto dx = static_cast<i16t>(oldV >> 32);
    // convert cm to m then devide by precision
    dx += static_cast<i16t>(deltaPos.x() / precision);

    auto dy = static_cast<i16t>(oldV >> 16);
    dy += static_cast<i16t>(deltaPos.y() / precision);

    auto dz = static_cast<i16t>(oldV);
    dz += static_cast<i16t>(deltaPos.z() / precision);

    const auto newV = static_cast<u64t>(dx) << 32 | static_cast<u64t>(dy) << 16 | static_cast<u64t>(dz);

    compressedDeltaPos_.store(newV);
}

void MU_CALLCONV Coord::accRotation(mu::NQuat deltaRot) {
    // 0-1: vx, 2-3: vy, 4-5: vz, 6-7: w, precision: 0.0001rad
    static constexpr auto precision = 0.0001f;
    const auto oldV = compressedDeltaRot_.load();

    const auto x = static_cast<float>(
        static_cast<i16t>((oldV >> 48) & 0xFFFF)
        * precision   
    );

    const auto y = static_cast<float>(
        static_cast<i16t>((oldV >> 32) & 0xFFFF)
        * precision
    );

    const auto z = static_cast<float>(
        static_cast<i16t>((oldV >> 16) & 0xFFFF)
        * precision
    );

    const auto w = static_cast<float>(
        static_cast<i16t>(oldV & 0xFFFF)
        * precision
    );

    auto quat = mu::NQuat(x, y, z, w, mu::NQuat::NoNormalize_t{});
    quat *= deltaRot;

    const auto newV = static_cast<u64t>(static_cast<i16t>(quat.x() / precision)) << 48
        | static_cast<u64t>(static_cast<i16t>(quat.y() / precision)) << 32
        | static_cast<u64t>(static_cast<i16t>(quat.z() / precision)) << 16
        | static_cast<u64t>(static_cast<i16t>(quat.w() / precision));

    compressedDeltaRot_.store(newV);
}

void Coord::resetDeltaPos() {
    compressedDeltaPos_.store(0);
}
void Coord::resetDeltaRot() {
    // 0-1: vx, 2-3: vy, 4-5: vz, 6-7: w, precision: 0.0001rad
    static constexpr auto precision = 0.0001f;

    const auto identity = mu::NQuat();

    const auto newV = static_cast<u64t>(static_cast<i16t>(identity.x() / precision)) << 48
        | static_cast<u64t>(static_cast<i16t>(identity.y() / precision)) << 32
        | static_cast<u64t>(static_cast<i16t>(identity.z() / precision)) << 16
        | static_cast<u64t>(static_cast<i16t>(identity.w() / precision));

    compressedDeltaRot_.store(newV);
}

mu::Vec3 MU_CALLCONV Coord::decodeDeltaPos(u64t compressedDeltaPos) {
    // 2-3: x, 4-5: y, 6-7: z, precision: 0.03cm
    static constexpr auto precision = 0.03f;
    auto dx = static_cast<i16t>((compressedDeltaPos >> 32) & 0xFFFF);
    auto dy = static_cast<i16t>((compressedDeltaPos >> 16) & 0xFFFF);
    auto dz = static_cast<i16t>(compressedDeltaPos & 0xFFFF);

    return mu::Vec3(dx * precision, dy * precision, dz * precision);
}

mu::NQuat MU_CALLCONV Coord::decodeDeltaRot(u64t compressedDeltaRot) {
    // 0-1: vx, 2-3: vy, 4-5: vz, 6-7: w, precision: 0.0001rad
    static constexpr auto precision = 0.0001f;
    auto x = static_cast<i16t>((compressedDeltaRot >> 48) & 0xFFFF);
    auto y = static_cast<i16t>((compressedDeltaRot >> 32) & 0xFFFF);
    auto z = static_cast<i16t>((compressedDeltaRot >> 16) & 0xFFFF);
    auto w = static_cast<i16t>(compressedDeltaRot & 0xFFFF);

    return mu::NQuat(x * precision, y * precision, z * precision, w * precision, mu::NQuat::NoNormalize_t{});
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