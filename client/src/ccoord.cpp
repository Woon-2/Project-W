#include "ccoord.hpp"

void CoordRoot::addEntity(ecs::Entity& entity) {
    ecs::System<Coord>::addEntity(entity);
    auto weakCoord = entity.get<Coord>();
    auto coord = weakCoord.lock();
    if (!coord) {
        throw ECS_EXCEPT("Entity does not have a Coord component");
    }
    
    coord->get().setParent(&rootCoordSys_);
}