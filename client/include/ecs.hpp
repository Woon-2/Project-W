#ifndef __Ecsxx_HPP
#define __Ecsxx_HPP

#include "ecsExcept.hpp"

#include <cstdint>
#include <vector>
#include <deque>
#include <thread>
#include <map>
#include <ranges>
#include <optional>
#include <memory>
#include <tuple>
#include <functional>

#include <atomic>

#include "config.hpp"
#include "enumUtil.hpp"
#include "TMP.hpp"

#define ENABLE_COMPONENT(__ComponentType)    \
    friend class ecs::Entity; \
    static constexpr ecs::Components type() NOEXCEPT {  \
        return ecs::Components::__ComponentType;    \
    }   \
    static std::weak_ptr<__ComponentType> at(ecs::Entity::ID id) {    \
        if (auto ptr = ecs::Component::at(ecs::Components::__ComponentType, id).lock()) {    \
            return std::static_pointer_cast<__ComponentType>(ptr);    \
        }   \
        throw ECS_EXCEPT( #__ComponentType " is dead");    \
    }   \
    static std::weak_ptr<const __ComponentType> atC(ecs::Entity::ID id) {    \
        if (auto ptr = ecs::Component::atC(ecs::Components::__ComponentType, id).lock()) {    \
            return std::static_pointer_cast<const __ComponentType>(ptr);    \
        }   \
        throw ECS_EXCEPT( #__ComponentType " is dead");    \
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
    PlayerController = 1,
    RigidBody,
    AssetLinker,
    Coord,
    Model,
    Size
};

void init(const InitDesc& desc);

class Component;

class Entity {
public:
    using ID = std::uint32_t;

    Entity()
        : id_(fetch(std::this_thread::get_id())) {}

    ~Entity() {
        if (id_.has_value()) {
            release(std::this_thread::get_id(), id_.value());
        }
    }

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    Entity(Entity&& other) NOEXCEPT
        : id_(std::move(other.id_)) {
        other.id_.reset();
    }

    Entity& operator=(Entity&& other) NOEXCEPT {
        if (this == &other) {
            return *this;
        }

        id_ = std::move(other.id_);
        other.id_.reset();

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
    std::weak_ptr<ConcreteComponent> get();
    template <class ConcreteComponent>
    std::weak_ptr<const ConcreteComponent> get() const;
    std::weak_ptr<Component> get(Components type);
    std::weak_ptr<const Component> get(Components type) const;

    template <class ConcreteComponent, class ... Args>
    void createComponent(Args&& ... args);

    bool valid() const NOEXCEPT {
        return id_.has_value();
    }

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

    ID fetch(std::thread::id threadId);
    std::vector<ID> fetch(std::thread::id threadId, std::size_t cnt);
    void release(std::thread::id threadId, ID id) {
        sEntityPools[poolIdx(threadId)].push_back(id);
    }
    template <std::ranges::range R>
    void release(std::thread::id threadId, const R& ids) {
        auto idx = poolIdx(threadId);
        sEntityPools[idx].insert(
            sEntityPools[idx].end(), std::begin(ids), std::end(ids)
        );
    }

    static std::vector<std::deque<ID>> sEntityPools;
    static std::map<std::thread::id, std::size_t> sThreadMap;
    static std::atomic_flag sLock;
    std::vector<void*> components_;
    std::optional<ID> id_;
};

class Component {
protected:
    friend class Entity;

    static std::weak_ptr<Component> at(Components type, Entity::ID idx) {
        if (static_cast<std::size_t>(idx) >= sComponents[etoi(type)].size()) {
            throw ECS_EXCEPT("Component index out of range");
        }

        return sComponents[etoi(type)][idx];
    }

    static std::weak_ptr<const Component> atC(Components type, Entity::ID idx) {
        if (static_cast<std::size_t>(idx) >= sComponents[etoi(type)].size()) {
            throw ECS_EXCEPT("Component index out of range");
        }

        return sComponents[etoi(type)][idx];
    }

public:
    Component(const Entity& entity)
        : entityID_(entity.id()) {}

    virtual ~Component() = default;
    Component(const Component&) = default;
    Component& operator=(const Component&) = default;
    Component(Component&&) noexcept = default;
    Component& operator=(Component&&) noexcept = default;

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

    static std::vector< std::vector<std::shared_ptr<Component> > > sComponents;
    std::optional<Entity::ID> entityID_;
};

template <class ConcreteComponent, class ... Args>
void Entity::createComponent(Args&& ... args) {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    Component::sComponents[ etoi(ConcreteComponent::type()) ][id_.value()]
        = std::make_shared<ConcreteComponent>(*this, std::forward<Args>(args)...);
}

template <class ConcreteComponent>
ConcreteComponent& Entity::as() {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    if (auto component = ConcreteComponent::at(id_.value()).lock()) {
        return *component;
    }

    throw ECS_EXCEPT("Component is dead");
}

template <class ConcreteComponent>
const ConcreteComponent& Entity::as() const {
    if (!valid()) {
        throw ECS_EXCEPT("Entity is invalid");
    }

    if (auto component = ConcreteComponent::atC(id_.value()).lock()) {
        return *component;
    }

    throw ECS_EXCEPT("Component is dead");
}

template <class ConcreteComponent>
std::weak_ptr<ConcreteComponent> Entity::get() {
    if (!valid()) {
        return {};
    }

    return ConcreteComponent::at(id_.value());
}

template <class ConcreteComponent>
std::weak_ptr<const ConcreteComponent> Entity::get() const {
    if (!valid()) {
        return {};
    }

    return ConcreteComponent::atC(id_.value());
}

inline std::weak_ptr<Component> Entity::get(Components type) {
    if (!valid()) {
        return {};
    }

    return Component::at(type, id_.value());
}

inline std::weak_ptr<const Component> Entity::get(Components type) const {
    if (!valid()) {
        return {};
    }

    return Component::atC(type, id_.value());
}

template <class ... ConcreteComponents>
class System {
protected:
    template <class ConcreteComponent>
    bool contains(Entity::ID id) {
        constexpr auto compIdx = indexOf<ConcreteComponent, ConcreteComponents...>();
        static_assert(compIdx != -1, "Component not found");

        const auto& compVec = std::get<compIdx>(componentsTuple_);

        return std::ranges::find_if( compVec, [&id](const auto& component) {
            auto compEntityID = component.lock()->entityID();
            return compEntityID.has_value() && compEntityID.value() == id;
        } ) != std::end(compVec);
    }

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

        std::apply(
            [&entity](auto& ... components) {
                (components.push_back(entity.get<ConcreteComponents>()), ...);
            },
            componentsTuple_
        );
    }

private:
    std::tuple< std::vector<std::weak_ptr<ConcreteComponents>>... > componentsTuple_;
};

}   // namespace ecs

#endif  // __Ecsxx_HPP