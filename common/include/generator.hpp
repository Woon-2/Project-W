#ifndef __Generator_HPP
#define __Generator_HPP

#include <coroutine>
#include <iterator>
#include <exception>
#include <variant>
#include <utility>
#include <cassert>
#include <memory>

template <class T>
class Generator {
private:
	struct Promise;
	class ConstIterator;
	class Iterator;
	struct Sentinel {};

public:
	using promise_type = Promise;
	using value_type = T;
	using iterator = Iterator;
	using const_iterator = ConstIterator;
	using sentinel_t = Sentinel;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;

	explicit Generator(std::coroutine_handle<Promise> h) noexcept
		: h_(h) {}

	Generator(Generator&& other) noexcept
		: h_(std::exchange(other.h_, {})) {}

	Generator& operator=(Generator&& other) noexcept {
		destroy();
		h_ = std::exchange(other.h_, {});
	}

	~Generator() {
		destroy();
	}

	Iterator begin();
	ConstIterator begin() const;
	Sentinel end() const noexcept {
		return Sentinel{};
	}
	ConstIterator cbegin() const;
	Sentinel cend() const noexcept {
		return Sentinel{};
	}

	void destroy() noexcept {
		if (h_) [[likely]] {
			h_.destroy();
			h_ = nullptr;
		}
	}

	bool done() const {
		return h_ && h_.done();
	}

	[[maybe_unused]] bool resume();

private:
	std::coroutine_handle<Promise> h_;
};

template <class T>
struct Generator<T>::Promise {
	Generator get_return_object() noexcept {
		return Generator(std::coroutine_handle<Promise>
			::from_promise(*this)
		);
	}

	std::suspend_never initial_suspend() noexcept {
		return {};
	}

	std::suspend_always final_suspend() noexcept {
		return {};
	}

	std::suspend_always yield_value(T val) noexcept(noexcept(
		std::is_nothrow_move_assignable_v<T>
		)) {
		stored_ = std::move(val);
		return {};
	}

	void return_void() noexcept {}

	void unhandled_exception() noexcept {
		stored_ = std::current_exception();
	}

	std::variant<std::monostate, T, std::exception_ptr> stored_;
};

template <class T>
[[maybe_unused]] bool Generator<T>::resume() {
	if (!done()) [[likely]] {
		h_.resume();
		if (std::holds_alternative<std::exception_ptr>(
			h_.promise().stored_
		)) [[unlikely]] {
			std::rethrow_exception(h_.promise().stored_);
		}
	}

	return !done();
}

template <class T>
class Generator<T>::ConstIterator {
public:
	using iterator_category = std::input_iterator_tag;
	using value_type = typename Generator::value_type;
	using difference_type = std::ptrdiff_t;
	using reference = typename Generator::const_reference;
	using pointer = typename Generator::const_pointer;
	using sentinel_t = Sentinel;

	explicit ConstIterator(std::coroutine_handle<Promise> h) noexcept
		: h_(h) {}

	ConstIterator(ConstIterator&& other) noexcept
		: h_(std::exchange(other.h_, {})) {}

	ConstIterator& operator=(ConstIterator&& other) noexcept {
		h_ = std::exchange(other.h_, {});
		return *this;
	}

	[[maybe_unused]] ConstIterator& operator++() {
		h_.resume();
		return *this;
	}

	void operator++(int) {
		operator++();
	}

	reference operator*() const {
		assert(h_ != nullptr);
		return std::get<value_type>(h_.promise().stored_);
	}

	pointer operator->() const {
		return std::addressof(operator*());
	}

	friend bool operator==(const ConstIterator& lhs, Sentinel) noexcept {
		return lhs.h_.done();
	}

	friend bool operator!=(const ConstIterator& lhs, Sentinel s) noexcept {
		return !(lhs == s);
	}

	friend bool operator==(Sentinel s, const ConstIterator& rhs) noexcept {
		return rhs == s;
	}

	friend bool operator!=(Sentinel s, const ConstIterator& rhs) noexcept {
		return rhs != s;
	}

private:
	std::coroutine_handle<Promise> h_;
};

template <class T>
class Generator<T>::Iterator : public Generator<T>::ConstIterator {
public:
	using iterator_category = typename ConstIterator::iterator_category;
	using value_type = typename Generator::value_type;
	using difference_type = typename ConstIterator::difference_type;
	using reference = typename Generator::reference;
	using pointer = typename Generator::pointer;
	using sentinel_t = Sentinel;

	explicit Iterator(std::coroutine_handle<Promise> h)
		: ConstIterator(h) {}

	Iterator(Iterator&& other) noexcept
		: ConstIterator(std::move(other)) {}

	Iterator& operator=(Iterator&& other) noexcept {
		ConstIterator::operator=(std::move(other));
		return *this;
	}

	[[maybe_unused]] Iterator& operator++() {
		ConstIterator::operator++();
		return *this;
	}

	void operator++(int) {
		operator++();
	}

	reference operator*() const {
		return const_cast<reference>(
			ConstIterator::operator*()
			);
	}

	pointer operator->() const {
		return std::addressof(operator*());
	}

};

template <class T>
Generator<T>::Iterator Generator<T>::begin() {
	return Iterator(h_);
}

template <class T>
Generator<T>::ConstIterator Generator<T>::begin() const {
	return cbegin();
}

template <class T>
Generator<T>::ConstIterator Generator<T>::cbegin() const {
	return ConstIterator(h_);
}

#endif // __Generator_HPP