#ifndef __threadPool_HPP
#define __threadPool_HPP

#include "function.hpp"
#include "concurrentqueue.h"

#include <vector>
#include <thread>
#include <functional>
#include <type_traits>
#include <concepts>
#include <atomic>

class ThreadPool {
public:
	ThreadPool() = default;
	~ThreadPool() { done_ = true; }

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) noexcept = default;
	ThreadPool& operator=(ThreadPool&&) noexcept = default;

	void run(std::size_t threadCnt);
	void stop();

	template <class Fn, class ... Args>
		requires std::invocable<Fn, Args...>
	void addJob(Fn&& fn, Args&& ... args) {
		auto producerIdx = (++roundRobinCounter_) % producerTokens_.size();
		jobQ_.enqueue(producerTokens_[producerIdx], Function128<void()>(
			std::bind_front(std::forward<Fn>(fn), std::forward<Args>(args)...)
		));
	}

private:
	void worker(std::size_t i);

	moodycamel::ConcurrentQueue<Function128<void()>> jobQ_{};
	std::vector<moodycamel::ProducerToken> producerTokens_{};
	std::vector<std::jthread> workerThreads_{};
	std::atomic<int> roundRobinCounter_ = 0;
	std::atomic<bool> done_ = false;
};

#endif	// __threadPool_HPP