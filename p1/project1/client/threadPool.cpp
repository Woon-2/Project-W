#include "threadPool.hpp"

void ThreadPool::run(std::size_t threadCnt) {
	workerThreads_.reserve(threadCnt);
	producerTokens_.reserve(threadCnt);

	for (std::size_t i = 0u; i < threadCnt; ++i) {
		workerThreads_.emplace_back([this, i](){ this->worker(i); });
		producerTokens_.emplace_back(jobQ_);
	}
}

void ThreadPool::stop() {
	done_ = true;
	workerThreads_.clear();
	producerTokens_.clear();
}

void ThreadPool::worker(std::size_t threadIdx) {
	while (!done_) {
		// moodycamel::ConcurrentQueue의 다음 조언에 따라 Job을 꺼낸다.
		// When producing or consuming many elements, the most efficient way is to:
		// 1. Use the bulk methods of the queue with tokens
		// 2. Failing that, use the bulk methods without tokens
		// 3. Failing that, use the single-item methods with tokens
		// 4. Failing that, use the single-item methods without tokens
		const auto bulkSize = 4u;

		auto jobs = std::vector<Function128<void()>>(bulkSize);

		if ( auto jobCnt = jobQ_.try_dequeue_bulk_from_producer(
			producerTokens_[threadIdx], jobs.begin(), bulkSize
		) ) {
			for (std::size_t i = 0; i < jobCnt; ++i) {
				jobs[i]();
			}
		}
		else if ( auto jobCnt = jobQ_.try_dequeue_bulk(jobs.begin(), bulkSize) ) {
			for (std::size_t i = 0; i < jobCnt; ++i) {
				jobs[i]();
			}
		}
		else if (jobQ_.try_dequeue_from_producer(producerTokens_[threadIdx], jobs[0])) {
			jobs[0]();
		}
		else if (jobQ_.try_dequeue(jobs[0])) {
			jobs[0]();
		}
	} 
}
