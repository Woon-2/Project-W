#ifndef __Client_Coord_HPP
#define __Client_Coord_HPP

#include "ecs.hpp"

#include "coord.hpp"

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

#endif  // __Client_Coord_HPP