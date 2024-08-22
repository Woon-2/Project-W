#ifndef __KEYBOARDXX_HPP
#define __KEYBOARDXX_HPP

#include <algorithm>

#include <fstream>
#include <map>
#include <ranges>
#include <vector>

#include <deque>
#include <vector>
#include <limits>
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>

#include "resumable.hpp"
#include "enumUtil.hpp"

#include "config.hpp"

namespace ic {

enum class KeyState : std::uint8_t
{
	None = 0x01,
	Tap = 0x02,
	Hold = 0x04,
	Away = 0x08,
};

DEFINE_ENUM_LOGICAL_OP_ALL(KeyState);

using KeyStateFlag = std::underlying_type_t<KeyState>;
using namespace std::literals;

class Keyboard {
public:
    static constexpr auto press = 0x80u;
    static constexpr auto toggle = 0x01u;

    Keyboard()
        : states_( 0x100u, 0 ) {}

    virtual void patchKeyState() = 0;

    bool pressed(char key) const NOEXCEPT {
        return states_[key] & press;
    }

    auto& states() NOEXCEPT { return states_; }
    const auto& states() const NOEXCEPT { return states_; }

private:
    std::vector<std::uint8_t> states_;
};

struct KeyFlagPair {
    char key;
    KeyStateFlag flags;
};

class KeyInputTrigger {
public:
    friend class KeyInputTracker;

    using Milliseconds = std::chrono::duration<float, std::milli>;

    struct Desc {
        std::vector<KeyFlagPair> waitFor;
        Milliseconds keepMinimum;
        Milliseconds endurance;
    };

    KeyInputTrigger(std::function<void()> callback)
        : descs_(), callback_(callback) {}

    KeyInputTrigger(std::initializer_list<Desc> descs, std::function<void()> callback)
        : descs_(descs), callback_(callback) {}

    void addDesc(const Desc& desc) {
        descs_.push_back(desc);
    }

    void addDesc(Desc&& desc) {
        descs_.push_back(std::move(desc));
    }

private:
    auto& descs() NOEXCEPT { return descs_; }
    const auto& descs() const NOEXCEPT { return descs_; }
    void executeCallback() const { callback_(); }

    std::vector<Desc> descs_;
    std::function<void()> callback_;
};

class KeyInputTracker {
public:
    using Clock = std::chrono::steady_clock;
    using Milliseconds = std::chrono::duration<float, std::milli>;
    using TimePoint = std::chrono::time_point<Clock, Milliseconds>;

private:
    struct FrameSnapshot {
        std::vector<std::uint8_t> states;
        TimePoint time;
    };

public:
    KeyInputTracker(Milliseconds trackDuration = 600ms, Milliseconds holdRequired = 36ms)
        : snapshots_(), lastTime_(Clock::now()), trackDuration_(trackDuration) {}

    void update(Keyboard& keyboard);

    void addTrigger(const std::string& name, const KeyInputTrigger& trigger) {
        if (triggerMap_.contains(name)) {
            throw std::runtime_error("KeyInputTracker::addTrigger: name already exists");
        }

        triggerMap_.emplace(name, trigger);
    }

    // TODO: must check onboarding triggers
    void popTrigger(const std::string& name) {
        triggerMap_.erase(name);
    }

    Milliseconds trackDuration() const NOEXCEPT {
        return trackDuration_;
    }

    void setTrackDuration(Milliseconds duration) NOEXCEPT {
        trackDuration_ = duration;
    }

    bool isTap(char key) const NOEXCEPT;
    bool isHold(char key) const NOEXCEPT;
    bool isAway(char key) const NOEXCEPT;
    bool isNone(char key) const NOEXCEPT;

private:
    void checkTrigger(const KeyInputTrigger& trigger);
    Resumable trackTrigger(const KeyInputTrigger& trigger);

    std::map<std::string, KeyInputTrigger> triggerMap_;
    std::deque<FrameSnapshot> snapshots_;
    std::list<Resumable> triggersTracing_;
    TimePoint lastTime_;
    Milliseconds trackDuration_;
    Milliseconds holdRequired_;
};

// tap : holdRequired 내에 한 번이라도 눌려있어야 tap
// hold : holdRequired 내내 눌려있어야 hold
// away : holdRequired 내에 한 번이라도 눌려있고, 마지막이 안 눌려있어야 away
// none : holdRequired 내에 한 번도 눌리지 않았어야 none

// trackDuration에 다 눌려있어야 hold?

// {wait for: {'A' flags: 'Tab'}, {'B' flags: 'None'}, keep: 0ms, endurance: 10ms}
// -> {wait for: {'B' flags: 'Tab'}, keep: 0ms, endurance: 10ms}

}   // namespace ic

#endif // KEYBOARDXX_HPP
