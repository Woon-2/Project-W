#include "pch.hpp"
#include "MemoryPool.hpp"

void MemoryPool::push(MemoryHeader* header) {
	header->allocSize = 0;
	ASSERT_CRASH(queue_.try_enqueue(header));
	--useCount_;
}

MemoryHeader* MemoryPool::pop() {
	MemoryHeader* header = nullptr;

	if (queue_.try_dequeue(header)) {
		ASSERT_CRASH(header->allocSize == 0);
	}
	else {
		header = reinterpret_cast<MemoryHeader*>(::malloc(poolSize_));
	}

	++useCount_;
	return header;
}
