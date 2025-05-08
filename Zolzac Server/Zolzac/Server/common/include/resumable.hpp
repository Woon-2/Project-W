#ifndef __Resumable_HPP
#define __Resumable_HPP

#include <coroutine>
#include <utility>

class Resumable {
private:
	struct Promise {
		Resumable get_return_object() {
			return Resumable(std::coroutine_handle<Promise>
				::from_promise(*this)
			);
		}

		std::suspend_always initial_suspend() noexcept {
			return {};
		}

		std::suspend_always final_suspend() noexcept {
			return {};
		}

		void return_void() {}
		void unhandled_exception() {
			std::terminate();
		}
	};

public:
	using promise_type = Promise;

	explicit Resumable(std::coroutine_handle<Promise> h)
		: h_(h) {}

	~Resumable() {
		if (h_) {
			h_.destroy();
		}
	}

	Resumable(Resumable&& other) noexcept
		: h_(std::exchange(other.h_, {})) {}

	Resumable& operator=(Resumable&& other) noexcept {
		if (this != &other) {
			if (h_) {
				h_.destroy();
			}
			h_ = std::exchange(other.h_, {});
		}
		return *this;
	}

	bool done() const noexcept {
		return h_ && h_.done();
	}

	bool resume() {
		if (!done()) {
			h_.resume();
		}
		return !done();
	}

private:
	std::coroutine_handle<Promise> h_;
};

#endif // __Resumable_HPP