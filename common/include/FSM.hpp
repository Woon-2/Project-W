#ifndef __FSM_HPP
#define __FSM_HPP

#include <concepts>
#include <coroutine>
#include <iostream>
#include <unordered_map>
#include <deque>
#include <vector>
#include <list>
#include <any>
#include <string>
#include <ranges>
#include <algorithm>
#include <string_view>
#include <functional>
#include <limits>
#include <chrono>
#include <numeric>


inline void hash_combine(std::size_t& seed, std::size_t h) {
    seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
    template <>
    struct hash<std::pair<std::string, std::string>> {
        std::size_t operator()(const std::pair<std::string, std::string>& p) const {
            std::size_t seed = 0;
            hash_combine(seed, std::hash<std::string>{}(p.first));
            hash_combine(seed, std::hash<std::string>{}(p.second));
            return seed;
        }
    };
}

namespace fsm {

using Milliseconds = std::chrono::duration<float, std::milli>;
using Clock = std::chrono::high_resolution_clock;

// integer max value as event type is reserved for state transition event.
class Event {
public:
    static constexpr int evTypeTransition = std::numeric_limits<int>::max();

    Event() = default;

    template <class T, class ... Args>
    static Event create(int eventType, Args&& ... args) {
        return Event(eventType, T(std::forward<Args>(args)...));
    }

    static Event create(int eventType) {
        return Event(eventType, std::any{});
    }

    template <class StrLike1, class StrLike2>
    static Event transition(StrLike1&& fromStateKey, StrLike2&& toStateKey) {
        return Event(evTypeTransition, std::pair<std::string, std::string>(
            std::forward<StrLike1>(fromStateKey),
            std::forward<StrLike2>(toStateKey)
        ));
    }

    template <class T>
    T& get() {
        return std::any_cast<T&>(any_);
    }

    template <class T>
    const T& get() const {
        return std::any_cast<const T&>(any_);
    }

    int evType() const {
        return evType_;
    }

private:
    Event(int evType, const std::any& any)
        : any_(any), evType_(evType) {
    }
    Event(int evType, std::any&& any)
        : any_(std::move(any)), evType_(evType) {
    }

    std::any any_;
    int evType_;
};

struct Promise;

struct State : std::coroutine_handle<Promise> {
    using promise_type = Promise;
    std::string key;
};

struct Promise {
    State get_return_object() { return State(std::coroutine_handle<Promise>::from_promise(*this)); }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() { throw; }
    void return_void() noexcept {}
};

class FSM {
public:
    FSM(const std::string& key)
        : fsmKey_(key), evQ_(), deferredEvQ_(), states_(), stateMap_(), curState_(nullptr) {
        sFSMMap_.try_emplace(key, this);
    }

    auto getEvents() {
        struct Awaitable {
            bool await_ready() const { return false; }
            bool await_suspend(std::coroutine_handle<> h) {
                return pFSM->curState_->address() != h.address();
            }
            auto await_resume() {
                class Popper {
                public:
                    Popper(FSM* pFSM)
                        : pFSM_(pFSM), bg_(pFSM_->evQ_.begin()),
                        ed_(pFSM_->evQ_.end()), cur_(bg_) {
                    }

                    ~Popper() {
                        if (pFSM_) {
                            pFSM_->evQ_.erase(bg_, ed_);
                            // pFSM_->evQ_.clear();
                        }
                    }

                    Event* pop() {
                        if (cur_ == ed_) {
                            return nullptr;
                        }

                        if (cur_->evType() != Event::evTypeTransition) {
                            return &*(cur_++);
                        }

                        do {
                            auto& transition = cur_->get<std::pair<std::string, std::string>>();
                            if (transition.first == pFSM_->curState_->key) {
                                pFSM_->transitionMap_.at(transition).operator()();
                                pFSM_->curState_ = pFSM_->stateMap_.at(transition.second);
                            }

                            if ((++cur_) == ed_) {
                                ed_ = cur_;
                                return nullptr;
                            }
                        } while (cur_->evType() == Event::evTypeTransition);

                        ed_ = cur_;
                        return nullptr;
                    }

                    operator bool() const noexcept { return bg_ != ed_; }

                private:
                    FSM* pFSM_;
                    std::deque<Event>::iterator bg_;
                    std::deque<Event>::iterator ed_;
                    std::deque<Event>::iterator cur_;
                };

                return Popper(pFSM);
            }

            FSM* pFSM;
        };

        return Awaitable{ this };
    }

    // initializing =============
    template <class FnState, class ... Args>
    [[maybe_unused]] State& addState(const std::string& stateKey, FnState&& fnState, Args&& ... args) {
        auto [it, _] = stateMap_.try_emplace(
            stateKey,
            &states_.emplace_back(
                std::invoke(fnState, *this, std::forward<Args>(args)...)
            )
        );
        it->second->key = stateKey;
        return *it->second;
    }

    template <class FnTransition, class ... Args>
    void addTransition(const std::string& fromStateKey, const std::string& toStateKey,
        FnTransition&& fnTransition, Args&& ... args
    ) {
        transitionMap_.try_emplace(
            std::pair<std::string, std::string>(fromStateKey, toStateKey),
            [fnTransition = std::forward<FnTransition>(fnTransition),
            args = std::forward_as_tuple(std::forward<Args>(args)...)
            ]() mutable {
                std::apply(fnTransition, std::move(args));
            }
        );
    }

    void start(const std::string& initialStateKey) {
        (curState_ = stateMap_.at(initialStateKey))->resume();
    }
    // ==========================


    // adding events ============
    // these functions would be asynchronous later on.
    // (returning awaitable types.)
    void transfer(const std::string& stateKey) {
        pushEvent(Event::transition(curState_->key, stateKey));
    }

    void pushEvent(const Event& ev) {
        evQ_.push_back(ev);
    }

    void pushEvent(Event&& ev) {
        evQ_.push_back(std::move(ev));
    }

    static void sendEvent(const Event& ev, const std::string& FSMKey) {
        sFSMMap_.at(FSMKey)->pushEvent(ev);
    }

    static void sendEvent(Event&& ev, const std::string& FSMKey) {
        sFSMMap_.at(FSMKey)->pushEvent(std::move(ev));
    }

    static void sendEvent(const Event& ev, FSM* pFSM) {
        pFSM->pushEvent(ev);
    }

    static void sendEvent(Event&& ev, FSM* pFSM) {
        pFSM->pushEvent(std::move(ev));
    }

    void pushDeferredEvent(const Event& ev) {
        deferredEvQ_.push_back(ev);
    }

    void pushDeferredEvent(Event&& ev) {
        deferredEvQ_.push_back(std::move(ev));
    }

    static void sendDeferredEvent(const Event& ev, const std::string& FSMKey) {
        sFSMMap_.at(FSMKey)->pushDeferredEvent(ev);
    }

    static void sendDeferredEvent(Event&& ev, const std::string& FSMKey) {
        sFSMMap_.at(FSMKey)->pushDeferredEvent(std::move(ev));
    }

    static void sendDeferredEvent(const Event& ev, FSM* pFSM) {
        pFSM->pushDeferredEvent(ev);
    }

    static void sendDeferredEvent(Event&& ev, FSM* pFSM) {
        pFSM->pushDeferredEvent(std::move(ev));
    }
    // ========================


    // update =================
    void update() {
        while (!evQ_.empty()) {
            curState_->resume();
        }

        std::ranges::move(deferredEvQ_, std::back_inserter(evQ_));
        deferredEvQ_.clear();
    }

    std::suspend_always completeStateUpdate() noexcept { return {}; }
    // ========================

    const std::string& key() const { return fsmKey_; }

private:
    static std::unordered_map<std::string, FSM*> sFSMMap_;

    std::string fsmKey_;
    std::deque<Event> evQ_;
    std::deque<Event> deferredEvQ_;
    std::list<State> states_;
    std::unordered_map<std::string, State*> stateMap_;
    std::unordered_map<
        std::pair<std::string, std::string>, std::function<void()>
    > transitionMap_;
    State* curState_;
};

}   // namespace fsm

#endif  // __FSM_HPP