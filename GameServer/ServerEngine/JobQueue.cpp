#include "sepch.hpp"
#include "JobQueue.hpp"
#include "JobQueuePool.hpp"

void JobQueue::push(Job* job, bool pushOnly) {
	const int32 prevCnt = jobCount_.fetch_add(1);
	queue_.enqueue(job);

	// 첫 번째 job을 넣은 스레드가 실행까지 담당한다.
	if (prevCnt == 0) {
		if (LJobQueue == nullptr && pushOnly == false) {
			execute();
		}
		else {
			// 여유있는 스레드에게 실행을 넘긴다.
			JobQueuePool::push(this);
		}
	}
}

void JobQueue::execute() {
	LJobQueue = this;

	while (true) {
		const int32 bulkSize = 100;
		auto jobs = std::vector<Job*>(bulkSize);
		auto jobCount = static_cast<int32>(queue_.try_dequeue_bulk(jobs.begin(), bulkSize));

		for (int32 i = 0; i < jobCount; ++i) {
			jobs[i]->execute();
			ObjectPool<Job>::push(jobs[i]);
		}

		if (jobCount_.fetch_sub(jobCount) == jobCount) {
			LJobQueue = nullptr;
			break;
		}
	}
}
