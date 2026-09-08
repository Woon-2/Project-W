#include "sepch.hpp"
#include "JobQueuePool.hpp"

ccqueue<JobQueue*> JobQueuePool::queue_;
