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
	// 이 큐를 동시에 실행 중인 스레드 수. 1을 넘으면 Room의 "잡 큐가 직렬화하므로
	// 락이 필요 없다"는 전제가 깨진 것이다(= Room 상태 데이터 레이스).
	const int32 depth = executing_.fetch_add(1) + 1;
	if (depth != 1) {
		std::cout << "[JobQueue] CONCURRENT EXECUTE this=" << static_cast<void*>(this)
			<< " depth=" << depth << '\n';
	}

	while (true) {
		const int32 bulkSize = 100;
		auto jobs = std::vector<Job*>(bulkSize);
		auto jobCount = static_cast<int32>(queue_.try_dequeue_bulk(jobs.begin(), bulkSize));

		for (int32 i = 0; i < jobCount; ++i) {
			jobs[i]->execute();
			ObjectPool<Job>::push(jobs[i]);
		}

		// fetch_sub 이후 잔량이 음수면, 이 JobQueue가 파괴된 뒤 그 메모리를 물려받은
		// 다른 인스턴스의 카운터를 깎았다는 뜻이다(Room 자기 파괴 경로 UAF).
		// 원본은 `== jobCount`일 때만 탈출해서, 음수가 되면 빈 큐를 영원히 스핀하며 워커 하나를
		// 통째로 점유했다. 1회 보고 후 빠져나오게 해 손상이 워커 고갈로 번지지 않게 한다.
		const int32 remaining = jobCount_.fetch_sub(jobCount) - jobCount;
		if (remaining < 0) {
			std::cout << "[JobQueue] NEGATIVE jobCount this=" << static_cast<void*>(this)
				<< " value=" << remaining << " (executed=" << jobCount << ")\n";
		}
		if (remaining <= 0) break;
	}

	executing_.fetch_sub(1);
}
