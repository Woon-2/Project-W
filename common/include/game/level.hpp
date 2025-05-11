#ifndef __LEVEL_HPP
#define __LEVEL_HPP

#include "stdafx.hpp"

#include "ecs.hpp"

#include "coord.hpp"

#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace gameEngine {

class ObjectDisposition {
public:
    ObjectDisposition() = default;
    ObjectDisposition(std::ifstream& is);

    mu::Mat4x4 xform_;
    std::string name_;
    std::string prefabName_;
    std::vector<ObjectDisposition> children_;
};

class Coord : public ecs::Component {
public:
    ENABLE_COMPONENT(Coord);

    Coord(const ecs::Entity& entity) NOEXCEPT
        : ecs::Component(entity), coordSys_(), compressedDeltaPos_(), compressedDeltaRot_()
        {
        resetDeltaPos();
        resetDeltaRot();
    }

    gfx::coord::System& get() NOEXCEPT { return coordSys_; }
    const gfx::coord::System& get() const NOEXCEPT { return coordSys_; }

    // be careful with overflow
    void MU_CALLCONV accTranslation(mu::Vec3 deltaPos);
    // be careful with overflow
    void MU_CALLCONV accRotation(mu::NQuat deltaRot);

    u64t compressedDeltaPos() const NOEXCEPT {
        return compressedDeltaPos_.load();
    }

    u64t compressedDeltaRot() const NOEXCEPT {
        return compressedDeltaRot_.load();
    }

    void resetDeltaPos();
    void resetDeltaRot();

    static mu::Vec3 MU_CALLCONV decodeDeltaPos(u64t compressedDeltaPos);
    static mu::NQuat MU_CALLCONV decodeDeltaRot(u64t compressedDeltaRot);

private:
    gfx::coord::System coordSys_;
    // 2-3: x, 4-5: y, 6-7: z, precision: 0.0003m
    au64t compressedDeltaPos_;
    // 0-1: vx, 2-3: vy, 4-5: vz, 6-7: w, precision: 0.0001rad
    au64t compressedDeltaRot_;
};

class CoordRoot : public ecs::System<Coord> {
public:
    void addEntity(ecs::Entity& entity);
    void update() {
        rootCoordSys_.traverse();
    }

    gfx::coord::System& get() NOEXCEPT { return rootCoordSys_; }
    const gfx::coord::System& get() const NOEXCEPT { return rootCoordSys_; }

private:
    gfx::coord::System rootCoordSys_;
};

class LevelRegion : public ecs::Entity {
public:
    LevelRegion(const std::filesystem::path& path);

    std::vector<ecs::Entity> instantiateAllObjects(gfx::coord::System& coordRoot);

    ObjectDisposition& dispositionRoot() NOEXCEPT { return dispositionRoot_; }
    const ObjectDisposition& dispositionRoot() const NOEXCEPT { return dispositionRoot_; }

private:
    void instantiateObjectHierarchy( std::optional<std::size_t> parentIdx,
        const ObjectDisposition& disposition,
        gfx::coord::System& coordRoot, std::vector<ecs::Entity>& out
    );

    std::unique_ptr< std::ifstream > pStream_;
    ObjectDisposition dispositionRoot_;
};

}  // namespace gameEngine

#endif  // __LEVEL_HPP