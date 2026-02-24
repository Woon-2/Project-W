#ifndef memory_pool_manager_hpp
#define memory_pool_manager_hpp

class MemoryPool;

/**
* @brief SingletonBase
*/
class MemoryManager {
public:
	static void init();
	static void release();
	static void* allocate(int32 size);
	static void deallocate(void* ptr);

private:
	static std::vector<MemoryPool*> pools_;
	static const int32 maxAllocSize_{4096};
	static std::array<MemoryPool*, maxAllocSize_ + 1> poolTable_;
};

template<class T, class... Args>
// ToDo

#endif // memory_manager_hpp