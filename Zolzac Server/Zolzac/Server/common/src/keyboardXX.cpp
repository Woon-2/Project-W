#include "keyboardXX.hpp"


#include <ranges>
#include <algorithm>

namespace ic {

void KeyInputTracker::update(Keyboard& keyboard) {
    auto now = Clock::now();
    snapshots_.push_back( FrameSnapshot{ std::move(keyboard.states()), now } );
    lastTime_ = now;

    auto expiredRange = snapshots_ | std::views::take_while( [this](const auto& snapshot) {
        return std::chrono::duration_cast<Milliseconds>(lastTime_ - snapshot.time) > trackDuration_;
    } );

    snapshots_.erase( expiredRange.begin(), expiredRange.end().base() );
	std::erase_if( triggersTracing_, [](const auto& trigger) { return trigger.done(); } );
	std::ranges::for_each( triggersTracing_, [](auto& trigger) { trigger.resume(); } );	
}

bool KeyInputTracker::isTap(char key) const NOEXCEPT {
    return std::ranges::any_of( snapshots_ | std::views::reverse,
        [key](const auto& snapshot) {
            return snapshot.states[key] & Keyboard::press;
        }
    );
}

bool KeyInputTracker::isHold(char key) const NOEXCEPT {
    return std::ranges::all_of( snapshots_ | std::views::reverse
        | std::views::take_while( [this](const auto& snapshot) {
            return std::chrono::duration_cast<Milliseconds>(
                lastTime_ - snapshot.time
            ) < holdRequired_;
        } ),
        [key](const auto& snapshot) {
            return snapshot.states[key] & Keyboard::press;
        }
    );
}

bool KeyInputTracker::isAway(char key) const NOEXCEPT {
    return isTap(key) && !isHold(key);
}

bool KeyInputTracker::isNone(char key) const NOEXCEPT {
    return !isTap(key);
}

namespace {
bool keyFlagSatisfied(const KeyInputTracker& kit, const auto& waitFor) {
	return std::ranges::all_of( waitFor, [&kit](const auto& keyFlagPair) {
		if ( (keyFlagPair.flags & KeyState::Hold) && !kit.isHold(keyFlagPair.key) ) {
			return false;
		}
		if ( (keyFlagPair.flags & KeyState::Away) && !kit.isAway(keyFlagPair.key) ) {
			return false;
		}
		if ( (keyFlagPair.flags & KeyState::Tap) && !kit.isTap(keyFlagPair.key) ) {
			return false;
		}
		if ( (keyFlagPair.flags & KeyState::None) && !kit.isNone(keyFlagPair.key) ) {
			return false;
		}
		return true;
	} );
}
}	// namespace ic::{anonymous}

void KeyInputTracker::checkTrigger(const KeyInputTrigger& trigger) {
	auto& firstDesc = trigger.descs().front();

	if ( keyFlagSatisfied(*this, firstDesc.waitFor) ) {
		auto tracker = trackTrigger(trigger);
		tracker.resume();
		triggersTracing_.push_back( std::move(tracker) );
	}
}

// TODO: add flag for accumulated endurance
// TODO: track mouse input together
Resumable KeyInputTracker::trackTrigger(const KeyInputTrigger& trigger) {
	auto accTp = Clock::now();

	while (Clock::now() - accTp < trigger.descs().front().keepMinimum) {
		co_await std::suspend_always{};
	}
	
	for (const auto& desc : trigger.descs() | std::views::drop(1)) {
		while ( !keyFlagSatisfied(*this, desc.waitFor) ) {
			if (Clock::now() - accTp > desc.endurance) {
				co_return;
			}
			co_await std::suspend_always{};
		}

		while (Clock::now() - accTp < desc.keepMinimum) {
			co_await std::suspend_always{};
		}

		accTp = Clock::now();
	}

	trigger.executeCallback();
}

}   // namespace ic