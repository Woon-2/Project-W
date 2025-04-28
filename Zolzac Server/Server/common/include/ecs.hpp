#ifndef __Ecs_HPP
#define __Ecs_HPP

#include "ecsException.hpp"
#include "config.hpp"
#include "enumUtil.hpp"

#include <cstdint>
#include <vector>
#include <deque>
#include <thread>
#include <map>
#include <ranges>
#include <algorithm>
#include <optional>
#include <memory>
#include <tuple>
#include <functional>
#include <type_traits>

#include <atomic>

#define ENABLE_COMPONENT(__ComponentType)    \
    friend class ecs::Entity; \
    static constexpr ecs::Components type() NOEXCEPT {  \
        return ecs::Components::__ComponentType;    \
    }   \
    static __ComponentType* at(ecs::Entity::ID id) {    \
        auto ptr = ecs::Component::at(ecs::Components::__ComponentType, id);    \
        return static_cast<__ComponentType*>(ptr);    \
    }   \
    static const __ComponentType* atC(ecs::Entity::ID id) {    \
        auto ptr = ecs::Component::atC(ecs::Components::__ComponentType, id);   \
        return static_cast<const __ComponentType*>(ptr);    \
    }

// Entity를 할당하는 스레드와 반납하는 스레드가 다르면,
// Entity 풀들의 크기가 서로 달라지게 된다.
// 문제가 되면 고치자.

namespace ecs {

struct InitDesc {
    std::size_t threadCnt;
    std::size_t entityPoolSize;
};

enum class Components {
#ifdef ECS_CLIENT
    PlayerController,
    AssetLinker,
    Model,
    Camera,
    Light,
    LevelChunkModel,
#endif
#ifdef ECS_SERVER
    DummyModel,
#endif
    RigidBody,
    Coord,
    NetEx,
    BoundingVolume,
    AnimController,
    Size,
};

void init(const InitDesc& desc);

class Component;

class Entity {
public:
    using ID = std::uint32_t;

    Entity()
        : id_(fetch(std::this_thread::get_id())) {}

    ~Entity() {
        release(std::this_thread::get_id());
    }

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    Entity(Entity&& other) NOEXCEPT
        : id_(std::exchange(other.id_, std::nullopt)) {}

    Entity& operator=(Entity&& other) NOEXCEPT {
        if (this == &other) {
            return *this;
        }

        id_ = std::exchange(other.id_, std::nullopt);

        return *this;
    }

    const std::optional<ID>& id() const NOEXCEPT {
        return id_;
    }

    template <class ConcreteComponent>
    ConcreteComponent& as();
    template <class ConcreteComponent>
    const ConcreteComponent& as() const;

    template <class ConcreteComponent>
    ConcreteComponent* get();
    template <class ConcreteComponent>
    const ConcreteComponent* getC() const {
        return get<ConcreteComponent>();
    }
    template <class ConcreteComponent>
    const ConcreteComponent* get() const;
    Component* get(Components type);
    const Component* getC(Components type) {
        return get(type);
    }
    const Component* get(Components type) const;

    template <class ConcreteComponent, class ... Args>
    void createComponent(Args&& ... args);

    bool valid() const NOEXCEPT {
        return id_.has_value();
    }

    void release(std::thread::id threadId);

    auto operator<=>(const Entity& other) const NOEXCEPT = default;

private:
    friend void init(const InitDesc& desc);

    static void init(std::size_t threadCnt, std::size_t entityPoolSize);
    static std::size_t poolIdx(std::thread::id threadId);

    bool available(std::thread::id threadId) const NOEXCEPT {
        return !sEntityPools[poolIdx(threadId)].empty();
    }
    bool available(std::thread::id threadId, std::size_t cnt) const NOEXCEPT {
        return sEntityPools[poolIdx(threadId)].size() >= cnt;
    }

    static ID fetch(std::thread::id threadId);
    static std::vector<ID> fetch(std::thread::id threadId, std::size_t cnt);

    static std::vector<std::deque<ID>> sEntityPools;
    static std::map<std::thread::id, std::size_t> sThreadMap;
    static std::atomic_flag sLock;
    std::optional<ID> id_;
};

class Component {
protected:
    friend class Entity;

    static Component* at(Components type, Entity::ID idx) {
        if (static_cast<std::size_t>(idx) >= sComponents[etoi(type)].size()) {
            throw ECS_EXCEPT("Component index out of range");
        }

        return sComponents[etoi(type)][idx].get();
    }

    static const Component* atC(Components type, Entity::ID idx) {
        if (static_cast<std::size_t>(idx) >= sComponents[etoi(type)].size()) {
            throw ECS_EXCEPT("Component index out of range");
        }

        return sComponents[etoi(type)][idx].get();
    }

public:
    Component(const Entity& entity)
        : entityID_(entity.id()) {}

    virtual ~Component() = default;
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) noexcept;
    Component& operator=(Component&&) noexcept;

    bool valid() const NOEXCEPT {
        return entityID_.has_value();
    }

    const std::optional<Entity::ID>& entityID() const NOEXCEPT {
        return entityID_;
    }

    auto operator<=>(const Component& other) const NOEXCEPT = default;

private:
    friend void init(const InitDesc& desc);
    static void init(std::size_t entityCnt) {
        sComponents.clear();
        sComponents.resize(etoi(Components::Size));

        for (auto& component : sComponents) {
            component.resize(entityCnt);
        }
    }

    static std::vector< std::vector<std::unique_ptr<Component>> > sComponents;
    std::optional<Entity::ID> entityID_;
};

template <class ConcreteComponent, class ... Args>
void Entity::createComponent(Args&& ... args) {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    Component::sComponents[ etoi(ConcreteComponent::type()) ][id_.value()]
        = std::make_unique<ConcreteComponent>(*this, std::forward<Args>(args)...);
}

template <class ConcreteComponent>
ConcreteComponent& Entity::as() {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    if (auto component = ConcreteComponent::at(id_.value())) {
        return *component;
    }

    throw ECS_EXCEPT("Component is dead");
}

template <class ConcreteComponent>
const ConcreteComponent& Entity::as() const {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    if (auto component = ConcreteComponent::atC(id_.value())) {
        return *component;
    }

    throw ECS_EXCEPT("Component is dead");
}

template <class ConcreteComponent>
ConcreteComponent* Entity::get() {
    if (!valid()) {
        return nullptr;
    }

    return ConcreteComponent::at(id_.value());
}

template <class ConcreteComponent>
const ConcreteComponent* Entity::get() const {
    if (!valid()) {
        return nullptr;
    }

    return ConcreteComponent::atC(id_.value());
}

inline Component* Entity::get(Components type) {
    if (!valid()) {
        return nullptr;
    }

    return Component::at(type, id_.value());
}

inline const Component* Entity::get(Components type) const {
    if (!valid()) {
        return nullptr;
    }

    return Component::atC(type, id_.value());
}

template <class T>
using SysCompCont = std::vector<T>;

// TODO: Remove expired components
template <class ... ConcreteComponents>
class System {
protected:
    template <class ConcreteComponent>
    auto& components() NOEXCEPT {
        constexpr auto compIdx = indexOf<ConcreteComponent, ConcreteComponents...>();
        static_assert(compIdx != -1, "Component not found");

        return std::get<compIdx>(componentsTuple_);
    }

    template <class ConcreteComponent>
    const auto& components() const NOEXCEPT {
        constexpr auto compIdx = indexOf<ConcreteComponent, ConcreteComponents...>();
        static_assert(compIdx != -1, "Component not found");

        return std::get<compIdx>(componentsTuple_);
    }

public:
    System() = default;
    virtual ~System() = default;
    System(const System&) = default;
    System& operator=(const System&) = default;
    System(System&&) noexcept = default;
    System& operator=(System&&) noexcept = default;

    void addEntity(Entity& entity) {
        if (!entity.valid()) {
            throw ECS_EXCEPT("Entity is invalid");
        }

        auto pushIfNotNull = [&entity](auto& components) {
            if (auto component = entity.get<std::remove_pointer_t< std::ranges::range_value_t<decltype(components)> >>()) {
                components.push_back(component);
            }
        };

        std::apply(
            [&pushIfNotNull](auto& ... components) {
                (pushIfNotNull(components), ...);
            },
            componentsTuple_
        );
    }

private:
    std::tuple< SysCompCont<ConcreteComponents*>... > componentsTuple_;
};

}   // namespace ecs

#endif  // __Ecs_HPP