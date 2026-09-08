#ifndef job_hpp
#define job_hpp

#include <functional>
#include "JobQueue.hpp"

using CallbackType = std::function<void()>;

class Job {
public:
	Job(CallbackType&& callback) : callback_(std::move(callback)) {}

	template<class T, class Ret, class... Args>
	Job(T* obj, Ret(T::* memFn)(Args...), Args&&... args) {
		callback_ = [obj, memFn, args...]() {
			//(obj->*memFn)(args...);
			std::invoke(memFn, obj, args...);
		};
	}

	void execute() { callback_(); }

private:
	CallbackType callback_;
};

#endif // job_hpp