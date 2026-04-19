#include "sepch.hpp"
#include "JobQueue.hpp"
#include "JobQueuePool.hpp"

void JobQueue::push(Job* job) {
	const int32 prevCnt = jobCount_.fetch_add(1);
	queue_.enqueue(job);

	if (prevCnt == 0) {
		JobQueuePool::push(this);
	}
}

void JobQueue::execute() {
	while (true) {
		const int32 bulkSize = 100;
		auto jobs = std::vector<Job*>(bulkSize);
		auto jobCount = static_cast<int32>(queue_.try_dequeue_bulk(jobs.begin(), bulkSize));

		for (int32 i = 0; i < jobCount; ++i) {
			jobs[i]->execute();
			ObjectPool<Job>::push(jobs[i]);
		}

		if ( jobCount_.fetch_sub( jobCount ) == jobCount ) break;
	}
}
