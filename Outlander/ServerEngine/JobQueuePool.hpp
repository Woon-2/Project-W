#ifndef global_queue_hpp
#define global_queue_hpp

#include "types.hpp"
#include "JobQueue.hpp"

/**
* @brief SingletonBase
*/
class JobQueuePool {
public:
	static void push(JobQueue* jobQueue) {
		queue_.enqueue(jobQueue);
	}

	static JobQueue* pop() {
		JobQueue* jobQueue;

		if (queue_.try_dequeue(jobQueue)) {
			return jobQueue;
		}
		else {
			return nullptr;
		}
	}

private:
	static ccqueue<JobQueue*> queue_;
};

#endif // global_queue_hpp