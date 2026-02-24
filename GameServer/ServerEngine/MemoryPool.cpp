#include "pch.hpp"
#include "MemoryPool.hpp"

/*------------------
     MemoryPool
------------------*/

void MemoryPool::push(MemoryHeader* header) {
    header->allocSize = 0;
    ASSERT_CRASH(pool_.enqueue(header));
    --useCount_;
}

MemoryHeader* MemoryPool::pop() {
    MemoryHeader* header = nullptr;

    if (pool_.try_dequeue(header)) {
        ASSERT_CRASH(header->allocSize == 0);
    }
    else {
        header = reinterpret_cast<MemoryHeader*>(::malloc(poolSize_));
    }

    ++useCount_;
    return header;
}
