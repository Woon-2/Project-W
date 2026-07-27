#ifndef job_queue_hpp
#define job_queue_hpp

#include "job.hpp"
#include "objectPool.hpp"

using CallbackType = std::function<void()>;

class JobQueue {
public:
	JobQueue() = default;

	void push(Job* job);
	void execute();

	// True when no thread is inside execute() and no jobs remain. push() bumps jobCount_
	// before enqueueing, so while idle() holds this queue is not registered in JobQueuePool
	// either. An owner destroyed via deferred reaping must observe this before being
	// returned to its pool.
	bool idle() const { return executing_.load() == 0 && jobCount_.load() <= 0; }

	void doAsync(CallbackType&& callback) {
		push(ObjectPool<Job>::pop(std::move(callback)));
	}

	template<class T, class Ret, class... Args>
	void doAsync(T* obj, Ret(T::* memFn)(Args...), Args&&... args) {
		push(ObjectPool<Job>::pop(obj, memFn, std::forward<Args>(args)...));
	}

private:
	ccqueue<Job*> queue_;
	std::atomic_int32_t jobCount_;

	// Concurrency-invariant probe. A Room assumes its JobQueue is executed by
	// exactly one thread at a time (that is why Room state needs no locks). If a Room
	// is destroyed while its own execute() loop is still on the stack, the pooled
	// memory can be reused by a new Room and the stale loop then corrupts the new
	// jobCount_, which breaks that invariant. These counters make it observable.
	std::atomic_int32_t executing_{ 0 };
};

#endif // job_queue_hpp