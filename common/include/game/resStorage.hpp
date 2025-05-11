#ifndef __resStorage_HPP
#define __resStorage_HPP

#include "stdafx.hpp"

#include "TMP.hpp"

class ResourceStorage {
public:
    enum class ResType {
        Texture,
        TexArray,
        TexCube,
        RefModel,
        Skeleton,
        BVHPath,
        AnimClip,
        Unknown,
    };

    using SlotID = std::string;
    using ResID = std::string;

    template <class K, class V>
    using ContainerType = ccMap<K, V>;

    class Slot;

    template <class T>
        requires std::is_default_constructible_v<T>
    Slot& addSlot(SlotID id, ResType resType) {
        slots_[id].init<T>(resType);

        return it->second;
    }

    Slot& slot(SlotID id) {
        return slots_.at(id);
    }

    const Slot& slot(SlotID id) const {
        return slots_.at(id);
    }

    bool hasSlot(SlotID id) const {
        return slots_.find(id) != slots_.end();
    }

private:
    // each slot is a container of resources
    ccMap<SlotID, Slot> slots_;
};

template <class T>
struct ResTypeMapperT2E {
    static constexpr auto resType = ResourceStorage::ResType::Unknown;
};

template <ResourceStorage::ResType E>
struct ResTypeMapperE2T {
    using type = void;
};

class ResourceStorage::Slot {
public:
    template <class T>
        requires std::is_default_constructible_v<T>
    void init(ResType type) {
        valueType_ = type;
        container_ = ContainerType<ResID, T>();
    }

    bool contains(ResType type) const {
        return valueType_ == type;
    }

    template <class T>
        requires std::is_default_constructible_v<T>
    bool contains(const ResID& resID) const {
        auto pContainer = any_cast<const ContainerType<T>>(&container_);
        return pContainer->find(resID) != pContainer->end();
    }

    template <class T, class Loader, class ... Args>
        requires (std::is_default_constructible_v<T> && std::invocable<Loader, Args...>)
    T& load(const ResID& resID, Loader&& loader, Args&& ... args) {
        if (valueType_ != ResTypeMapperT2E<T>::resType) [[unlikely]] {
            throw std::runtime_error("[Description] ResourceStorage::Slot::load: ResType mismatch detected.\n"
                "Tried to load an instance of wrong resource type on ResourceStorage."
            );
        }

        auto pContainer = any_cast<const ContainerType<T>>(&container_);
        return (*pContainer)[resID] = std::invoke(loader, std::forward<Args>(args)...);
    }

    template <class T, class ... Args>
    T& load(const ResID& resID, Args&& ... args) {
        if (valueType_ != ResTypeMapperT2E<T>::resType) [[unlikely]] {
            throw std::runtime_error("[Description] ResourceStorage::Slot::load: ResType mismatch detected.\n"
                "Tried to load an instance of wrong resource type on ResourceStorage."
            );
        }

        auto pContainer = any_cast<const ContainerType<T>>(&container_);
        return (*pContainer)[resID] = T(std::forward<Args>(args)...);
    }

    template <class T>
    const T* get(const ResID& resID) const {
        if (valueType_ != ResTypeMapperT2E<T>::resType) [[unlikely]] {
            throw std::runtime_error("[Description] ResourceStorage::Slot::get: ResType mismatch detected.\n"
                "Tried to get an instance of wrong resource type from ResourceStorage."
            );
        }

        return &any_cast<const ContainerType<T>>(&container_)->at(resID);
    }

    template <class T>
    T* get(const ResID& resID) {
        if (valueType_ != ResTypeMapperT2E<T>::resType) [[unlikely]] {
            throw std::runtime_error("[Description] ResourceStorage::Slot::get: ResType mismatch detected.\n"
                "Tried to get an instance of wrong resource type from ResourceStorage."
            );
        }

        return &any_cast<ContainerType<T>>(&container_)->at(resID);
    }

private:
    AnyMoveOnly container_;
    ResType valueType_;
};

#endif    // __resStorage_HPP