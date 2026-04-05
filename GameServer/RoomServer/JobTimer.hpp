#ifndef job_timer_hpp
#define job_timer_hpp

class Job;

/*---------------
     JobData
---------------*/

struct JobData {
	uint32 roomId;
	Job* job;
};

/*-----------------
     TimerItem
-----------------*/

struct TimerItem {
	uint64 executionTime;
	JobData* jobData;

	bool operator<(const TimerItem& other) const {
		return executionTime > other.executionTime; // 우선순위 큐에서 가장 작은 executionTime이 먼저 나오도록
	}
};

/*----------------
     JobTimer
----------------*/

/**
* @brief SingletonBase
*/
class JobTimer {
public:
	static void addJob(uint64 delay, uint32 roomId, Job* job);
	static void distribute(uint64 now);
	static void clear();

private:
    static std::mutex jobTimerMtx_;
	static std::priority_queue<TimerItem> timerQueue_;
	static std::atomic_bool distributing_;
};

#endif // job_timer_hpp