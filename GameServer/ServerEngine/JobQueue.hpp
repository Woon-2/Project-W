#ifndef job_queue_hpp
#define job_queue_hpp

#include "job.hpp"
#include "objectPool.hpp"

class JobQueue {
public:
	JobQueue() = default;

	void execute();

	void doAsync(CallbackType&& callback) {
		push(ObjectPool<Job>::pop(std::move(callback)));
	}

	template<class T, class Ret, class... Args>
	void doAsync(T* obj, Ret(T::* memFn)(Args...), Args&&... args) {
		push(ObjectPool<Job>::pop(obj, memFn, std::forward<Args>(args)...));
	}

private:
	void push(Job* job, bool pushOnly = false);

private:
	ccqueue<Job*> queue_;
	std::atomic_int32_t jobCount_;
};

#endif // job_queue_hpp