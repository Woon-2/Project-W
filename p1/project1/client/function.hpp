#ifndef __function_HPP
#define __function_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <type_traits>
#include <cstring>
#include <functional>

template< class >
class Function128;

template < class R, class ... Args>
class Function128<R(Args...)> {
    static constexpr size_t BUFFER_SIZE = 128;  // 여기서 버퍼 크기 조정
    static constexpr size_t BUFFER_ALIGN = alignof(std::max_align_t);

    struct ICallable {
        virtual ~ICallable() = default;
        virtual R invoke(Args... args) = 0;
        virtual ICallable* clone_to(void* buffer) const = 0;
        virtual ICallable* clone_heap() const = 0;
        virtual void destroy(bool inBuffer) noexcept = 0;
    };

    template< class F >
    struct Callable : ICallable {
        F func;
        template< class Fn >
        Callable(Fn&& f) : func(std::forward<Fn>(f)) {}
        R invoke(Args... args) override { return func(std::forward<Args>(args)...); }

        ICallable* clone_to(void* buffer) const override {
            return new (buffer) Callable(func);
        }

        ICallable* clone_heap() const override {
            return new Callable(func);
        }

        void destroy(bool inBuffer) noexcept override {
            if (inBuffer)
                this->~Callable();
            else
                delete this;
        }
    };

    alignas(BUFFER_ALIGN) unsigned char buffer_[BUFFER_SIZE];
    ICallable* callable_ = nullptr;
    bool inBuffer_ = false;

public:
    Function128() = default;

    template<typename F>
    Function128(F f) {
        using DecayedF = std::decay_t<F>;
        constexpr bool fitsInBuffer =
            sizeof(Callable<DecayedF>) <= BUFFER_SIZE &&
            alignof(Callable<DecayedF>) <= BUFFER_ALIGN &&
            std::is_nothrow_move_constructible_v<DecayedF>;

        if constexpr (fitsInBuffer) {
            callable_ = new (buffer_) Callable<DecayedF>(std::forward<F>(f));
            inBuffer_ = true;
        } else {
            callable_ = new Callable<DecayedF>(std::forward<F>(f));
            inBuffer_ = false;
        }
    }

    ~Function128() { reset(); }

    Function128(const Function128& other) {
        if (!other.callable_) return;
        if (other.inBuffer_) {
            callable_ = other.callable_->clone_to(buffer_);
            inBuffer_ = true;
        } else {
            callable_ = other.callable_->clone_heap();
            inBuffer_ = false;
        }
    }

    Function128(Function128&& other) noexcept {
        move_from(std::move(other));
    }

    Function128& operator=(const Function128& other) {
        if (this != &other) {
            reset();
            if (other.callable_) {
                if (other.inBuffer_) {
                    callable_ = other.callable_->clone_to(buffer_);
                    inBuffer_ = true;
                } else {
                    callable_ = other.callable_->clone_heap();
                    inBuffer_ = false;
                }
            }
        }
        return *this;
    }

    Function128& operator=(Function128&& other) noexcept {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    explicit operator bool() const noexcept { return callable_ != nullptr; }

    R operator()(Args... args) const {
        if (!callable_) throw std::bad_function_call();
        return callable_->invoke(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        if (callable_) {
            callable_->destroy(inBuffer_);
            callable_ = nullptr;
            inBuffer_ = false;
        }
    }

private:
    void move_from(Function128&& other) noexcept {
        if (!other.callable_) return;

        if (other.inBuffer_) {
            std::memcpy(buffer_, other.buffer_, BUFFER_SIZE);
            callable_ = reinterpret_cast<ICallable*>(buffer_);
            inBuffer_ = true;
        } else {
            callable_ = other.callable_;
            inBuffer_ = false;
        }
        other.callable_ = nullptr;
        other.inBuffer_ = false;
    }
};

#endif	// __function_HPP