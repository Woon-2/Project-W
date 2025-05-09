#include "ecs.hpp"

#include <iostream>
#include <ranges>
#include <algorithm>
#include <numeric>

namespace ecs {

void init(const InitDesc& desc) {
    Entity::init(desc.entityPoolSize);
    Component::init(desc.threadCnt * desc.entityPoolSize);
}

void Entity::init(std::size_t entityPoolSize) {
	auto nums = std::vector<ID>( entityPoolSize );
    std::iota( nums.begin(), nums.end(), 0u );

    auto ret = idPool_.try_enqueue_bulk( nums.begin( ), nums.size( ) );
    if ( !ret ) {
		std::cerr << "Failed to initialize entity pool\n";
        system("pause");
        exit( -1 );
    }
}

Entity::ID Entity::fetch() {
    // lock free queue¿¡¼­ id pop
    ID id;
    if ( !idPool_.try_dequeue( id ) ) {
		std::cerr << "Failed to fetch entity id\n";
        system("pause");
        exit( -1 );
    }
    return id;
}

void Entity::release() {
    const auto oldId = id_.load( );
    if ( oldId == -1u ) {
        return;
    }

    id_.store( -1u );
	idPool_.enqueue( oldId );

    for ( auto& pComponents : Component::sComponents ) {
        pComponents[ oldId ].reset( );
    }
}

moodycamel::ConcurrentQueue<Entity::ID> Entity::idPool_;

Component::Component(Component&& other) noexcept
    : entityID_(std::exchange(other.entityID_, std::nullopt)) {}

Component& Component::operator=(Component&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    entityID_ = std::exchange(other.entityID_, std::nullopt);
    return *this;
}

std::vector< std::vector<std::unique_ptr<Component> > > Component::sComponents;

}   // namespace ecs