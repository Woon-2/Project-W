#ifndef __LEVEL_HPP
#define __LEVEL_HPP

#define ECS_SERVER

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
        : ecs::Component(entity) {}

    gfx::coord::System& get() NOEXCEPT { return coordSys_; }
    const gfx::coord::System& get() const NOEXCEPT { return coordSys_; }

private:
gfx::coord::System coordSys_;
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