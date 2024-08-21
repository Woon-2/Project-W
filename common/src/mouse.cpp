#include "mouse.hpp"

namespace ic {

const std::optional<Mouse::Event> Mouse::read() NOEXCEPT {
    if (auto ret = peek()) {
        buf_.pop();
        return ret;
    }

    return {};
}

void Mouse::onMouseWheel(Point pos, int wheelDelta) {
    wheelDeltaCarry_ += wheelDelta;
    
    auto nWheelStep = static_cast<int>(
        wheelDeltaCarry_ / wheelThreshold()
    );

    wheelDeltaCarry_ -= nWheelStep * wheelThreshold();

    while (nWheelStep > 0) {
        buf_.emplace( Event::Type::WheelUp, pos );
        --nWheelStep;
    }

    while (nWheelStep < 0) {
        buf_.emplace( Event::Type::WheelDown, pos );
        ++nWheelStep;
    }

    trimBuf();
}

void Mouse::onEnter(Point pos) {
    buf_.emplace( Event::Type::Enter, pos );
    trimBuf();
    bInWindow_ = true;
}

void Mouse::onLeave(Point pos) {
    buf_.emplace( Event::Type::Leave, pos );
    trimBuf();
    bInWindow_ = false;
}

void Mouse::trimBuf() NOEXCEPT {
    while (buf_.size() > bufSize()) {
        buf_.pop();
    }
}

}   // namespace ic