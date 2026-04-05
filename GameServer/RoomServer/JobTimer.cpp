#include "rspch.hpp"
#include "JobTimer.hpp"
#include "Job.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"

/*----------------
	 JobTimer
----------------*/

void JobTimer::addJob(uint64 delay, uint32 roomId, Job* job) {
	const uint64 executionTime = GetTickCount64() + delay;
	auto jobData = ObjectPool<JobData>::pop(roomId, job);

	std::lock_guard<std::mutex> lock(jobTimerMtx_);
	timerQueue_.push({executionTime, jobData});
}

void JobTimer::distribute(uint64 now) {
	if(distributing_.exchange(true) == true) {
		return; // 이미 분배 중인 경우, 중복 실행 방지
	}

	std::vector<TimerItem> readyItems;
	{
		std::lock_guard<std::mutex> lock(jobTimerMtx_);
		while (!timerQueue_.empty()) {
			const TimerItem& item = timerQueue_.top();
			if (item.executionTime > now) {
				break; // 아직 실행 시간이 되지 않은 작업이 있으므로 종료
			}

			readyItems.push_back(item);
			timerQueue_.pop();
		}
	}

	for (const TimerItem& item : readyItems) {
		if (auto room = RoomManager::findRoom(item.jobData->roomId)) {
			room->pushJob(item.jobData->job);
		}
		else {
			// 방이 존재하지 않는 경우, 작업을 실행할 수 없으므로 Job을 반환
			ObjectPool<Job>::push(item.jobData->job);
		}

		ObjectPool<JobData>::push(item.jobData);
	}

	distributing_.store(false);
}

void JobTimer::clear() {
	std::lock_guard<std::mutex> lock(jobTimerMtx_);

	while (!timerQueue_.empty()) {
		const TimerItem& item = timerQueue_.top();
		ObjectPool<Job>::push(item.jobData->job);
		ObjectPool<JobData>::push(item.jobData);
		timerQueue_.pop();
	}
}

std::mutex JobTimer::jobTimerMtx_;
std::priority_queue<TimerItem> JobTimer::timerQueue_;
std::atomic_bool JobTimer::distributing_ = false;
