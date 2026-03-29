#include "sepch.hpp"
#include "MemoryManager.hpp"
#include "MemoryPool.hpp"

void MemoryManager::init() {
	int32 poolSize{};
	int32 tableIdx{};

	const int32 poolCnt = (1024 / 32) + (1024 / 128) + (2048 / 256);
	pools_.reserve(poolCnt);

	for (poolSize = 32; poolSize <= 1024; poolSize += 32) {
		auto pool = new MemoryPool(poolSize);
		pools_.emplace_back(pool);

		while (tableIdx <= poolSize) {
			poolTable_[tableIdx++] = pool;
		}
	}

	poolSize = 1024 + 128;
	for (; poolSize <= 2048; poolSize += 128) {
		auto pool = new MemoryPool(poolSize);
		pools_.emplace_back(pool);

		while (tableIdx <= poolSize) {
			poolTable_[tableIdx++] = pool;
		}
	}

	poolSize = 2048 + 256;
	for (; poolSize <= maxAllocSize_; poolSize += 256) {
		auto pool = new MemoryPool(poolSize);
		pools_.emplace_back(pool);

		while (tableIdx <= poolSize) {
			poolTable_[tableIdx++] = pool;
		}
	}
}

void MemoryManager::release() {
	for (auto pool : pools_) {
		delete pool;
	}
	pools_.clear();
}

void* MemoryManager::allocate(int32 size) {
	MemoryHeader* header = nullptr;
	const int32 allocSize = size + sizeof(MemoryHeader);

	if (allocSize > maxAllocSize_) {
		// 메모리 풀링 최대 크기를 벗어나면 일반 할당
		header = reinterpret_cast<MemoryHeader*>(malloc(allocSize));
	}
	else {
		header = poolTable_[allocSize]->pop();
	}

	return MemoryHeader::attachHeader(header, allocSize);
}

void MemoryManager::deallocate(void* ptr) {
	auto header = MemoryHeader::detachHeader(ptr);

	const int32 allocSize = header->allocSize;
	ASSERT_CRASH(allocSize > 0);

	if (allocSize > maxAllocSize_) {
		// 메모리 풀링 최대 크기를 벗어나면 일단 해제
		free(header);
	}
	else {
		poolTable_[allocSize]->push(header);
	}
}

std::vector<MemoryPool*> MemoryManager::pools_;
std::array<MemoryPool*, MemoryManager::maxAllocSize_ + 1> MemoryManager::poolTable_;
