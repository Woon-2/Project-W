#ifndef __Mouse
#define __Mouse

#include <optional>
#include <queue>
#include <vector>

#include <cstdint>

#include "config.hpp"

namespace ic {

class Mouse {
private:
    struct Data2D {
        int x;
        int y;
    };

public:
    friend class MouseMsgAPI;

    using Point = Data2D;
    using RawDelta = Point;

    class Event {
    public:
        enum class Type {
            LPress,
            LRelease,
            RPress,
            RRelease,
            MPress,
            MRelease,
            WheelUp,
            WheelDown,
            Move,
            RawDelta,
            Enter,
            Leave
        };

        Event() NOEXCEPT
            : type_(), data_{0, 0} {}

        Event(Type typeVal, const Data2D& val) NOEXCEPT
            : type_(typeVal), data_(val) {}

        bool moved() const NOEXCEPT {
            return valid()
                && type_.value() == Type::Move;
        }

        bool leftPressed() const NOEXCEPT {
            return valid()
                && type_.value() == Type::LPress;
        }

        bool leftReleased() const NOEXCEPT {
            return valid()
                && type_.value() == Type::LRelease;
        }

        bool rightPressed() const NOEXCEPT {
            return valid()
                && type_.value() == Type::RPress;
        }

        bool rightReleased() const NOEXCEPT {
            return valid()
                && type_.value() == Type::RRelease;
        }

        bool midPressed() const NOEXCEPT {
            return valid()
                && type_.value() == Type::MPress;
        }

        bool midReleased() const NOEXCEPT {
            return valid()
                && type_.value() == Type::MRelease;
        }

        bool WheelUped() const NOEXCEPT {
            return valid()
                && type_.value() == Type::WheelUp;
        }

        bool WheelDowned() const NOEXCEPT {
            return valid()
                && type_.value() == Type::WheelDown;
        }

        bool entered() const NOEXCEPT {
            return valid()
                && type_.value() == Type::Enter;
        }

        bool leaved() const NOEXCEPT {
            return valid()
                && type_.value() == Type::Leave;
        }

        bool valid() const NOEXCEPT {
            return type_.has_value();
        }

        std::optional<Type> type() const {
            return type_;
        }

        const Point& pos() const NOEXCEPT {
            return data_;
        }

        const RawDelta& rawDelta() const NOEXCEPT {
            return data_;
        }

    private:
        Data2D data_;
        std::optional<Type> type_;
    };

    Mouse(std::size_t bufferSize = 32ull, int wheelThresholdVal = 120)
        : buf_(), bufSize_(bufferSize),
        wheelThreshold_(wheelThresholdVal), wheelDeltaCarry_(),
        bInWindow_(false) {};

    virtual ~Mouse() = default;

    Mouse(const Mouse&) = delete;
    Mouse& operator=(const Mouse&) = delete;
    Mouse(Mouse&&) = default;
    Mouse& operator=(Mouse&&) = default;
    
    const std::optional<Event> read() NOEXCEPT;
    const std::optional<Event> peek() const NOEXCEPT {
        if (empty()) {
            return {};
        }

        return buf_.front();
    }

    void setBufSize(std::size_t bufferSize) NOEXCEPT {
        bufSize_ = bufferSize;
    }

    std::size_t bufSize() const NOEXCEPT {
        return bufSize_;
    }

    bool empty() const NOEXCEPT {
        return buf_.empty();
    }

    void flush() NOEXCEPT {
        buf_ = decltype(buf_)();
    }

    void setWheelThreshold(int threshold) NOEXCEPT {
        wheelThreshold_ = threshold;
    }

    int wheelThreshold() const NOEXCEPT {
        return wheelThreshold_;
    }

    bool inWindow() const NOEXCEPT {
        return bInWindow_;
    }

    virtual void setPos(const Point& pt) {}
    virtual void hideCursor() {}
    virtual void showCursor() {}
    virtual void confineCursor(void* param) {}
    virtual void freeCursor() {}
    virtual bool cursorShown() const { return true; }
    virtual bool cursorConfined() const { return false; }

private:
    void onMouseMove(Point pos) {
        buf_.emplace( Event::Type::Move, pos );
        trimBuf();
    }

    void onLeftPressed(Point pos) {
        buf_.emplace( Event::Type::LPress, pos );
        trimBuf();
    }

    void onLeftReleased(Point pos) {
        buf_.emplace( Event::Type::LRelease, pos );
        trimBuf();
    }

    void onRightPressed(Point pos) {
        buf_.emplace( Event::Type::RPress, pos );
        trimBuf();
    }

    void onRightReleased(Point pos) {
        buf_.emplace( Event::Type::RRelease, pos );
        trimBuf();
    }

    void onMidPressed(Point pos) {
        buf_.emplace( Event::Type::MPress, pos );
        trimBuf();
    }

    void onMidReleased(Point pos) {
        buf_.emplace( Event::Type::MRelease, pos );
        trimBuf();
    }

    void onRaw(RawDelta delta) {
        buf_.emplace( Event::Type::RawDelta, delta );
        trimBuf();
    }

    void onMouseWheel(Point pos, int wheelDelta);
    void onEnter(Point pos);
    void onLeave(Point pos);
    void trimBuf() NOEXCEPT;

    std::queue<Event> buf_;
    std::size_t bufSize_;
    int wheelThreshold_;
    int wheelDeltaCarry_;
    bool bInWindow_;
};

class MouseMsgAPI {
public:
    static void onMouseMove(Mouse& mouse, Mouse::Point pos) {
        mouse.onMouseMove(pos);
    }

    static void onLeftPressed(Mouse& mouse, Mouse::Point pos) {
        mouse.onLeftPressed(pos);
    }

    static void onLeftReleased(Mouse& mouse, Mouse::Point pos) {
        mouse.onLeftReleased(pos);
    }

    static void onRightPressed(Mouse& mouse, Mouse::Point pos) {
        mouse.onRightPressed(pos);
    }

    static void onRightReleased(Mouse& mouse, Mouse::Point pos) {
        mouse.onRightReleased(pos);
    }

    static void onMidPressed(Mouse& mouse, Mouse::Point pos) {
        mouse.onMidPressed(pos);
    }

    static void onMidReleased(Mouse& mouse, Mouse::Point pos) {
        mouse.onMidReleased(pos);
    }

    static void onMouseWheel(
        Mouse& mouse, Mouse::Point pos, int wheelDelta
    ) {
        mouse.onMouseWheel(pos, wheelDelta);
    }

    static void onEnter(Mouse& mouse, Mouse::Point pos) {
        mouse.onEnter(pos);
    }

    static void onLeave(Mouse& mouse, Mouse::Point pos) {
        mouse.onLeave(pos);
    }

    static void onRaw(Mouse& mouse, Mouse::RawDelta delta) {
        mouse.onRaw(delta);
    }
};

}   // namespace ic

#endif  // __Mouse