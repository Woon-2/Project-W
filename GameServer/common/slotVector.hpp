#ifndef common_slot_vector_hpp
#define common_slot_vector_hpp

#include <vector>
#include <cstddef>
#include <utility>

// Stable-handle slot container with free-list reuse.
//
// Generalizes the "register / unregister with reusable slot index" pattern used
// for terrain (and now scatter) colliders in PhysicsWorld. A removed slot leaves
// an inactive tombstone (reset element) reused by the next add(). Handles stay
// valid across unrelated add/remove calls.
//
// T must be default-constructible, move-assignable, and contextually convertible
// to bool such that a default-constructed value tests false and an active value
// tests true (e.g. std::unique_ptr<U> or std::optional<U>).
template <class T>
class SlotVector {
public:
    using Handle = std::size_t;
    static constexpr Handle kInvalid = static_cast<std::size_t>(-1);

    template <class U>
    Handle add(U&& value) {
        Handle h;
        if (!free_.empty()) {
            h = free_.back();
            free_.pop_back();
            slots_[h] = std::forward<U>(value);
        } else {
            h = slots_.size();
            slots_.push_back(std::forward<U>(value));
        }
        return h;
    }

    void remove(Handle h) {
        if (h == kInvalid || h >= slots_.size()) return;
        if (!slots_[h]) return;            // already inactive
        slots_[h] = T{};                   // reset tombstone
        free_.push_back(h);
    }

    bool empty() const {
        for (const auto& s : slots_) if (s) return false;
        return true;
    }

    // Invokes f(*slot) for every active slot. f receives a reference to the
    // pointed-to / contained value.
    template <class F>
    void forEach(F&& f) const {
        for (const auto& s : slots_)
            if (s) f(*s);
    }

private:
    std::vector<T>           slots_;
    std::vector<std::size_t> free_;
};

#endif // common_slot_vector_hpp
