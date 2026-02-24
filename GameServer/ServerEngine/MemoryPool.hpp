#ifndef memory_pool_hpp
#define memory_pool_hpp

/*--------------------
     MemoryHeader
--------------------*/

struct MemoryHeader {
public:
	MemoryHeader(int32 size) : allocSize(size) {}

    static void* attachHeader(MemoryHeader* header, int32 size) {
        new(header)MemoryHeader(size);
        return reinterpret_cast<void*>(++header);
    }

    static MemoryHeader* detachHeader(void* ptr) {
        auto header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
		return header;
    }

    int32 allocSize;
};

/*------------------
     MemoryPool
------------------*/

class MemoryPool {
public:
    MemoryPool(int32 poolSize) : poolSize_(poolSize), useCount_(), pool_() {}

    ~MemoryPool() {
        MemoryHeader* header = nullptr;
        while (pool_.try_dequeue(header)) {
            ::free(header);
        }
    }

    void push(MemoryHeader* header);
    MemoryHeader* pop();


private:
    int32 poolSize_;
    std::atomic_int32_t useCount_;
    ccqueue<MemoryHeader*> pool_;
};

#endif // memory_pool_hpp