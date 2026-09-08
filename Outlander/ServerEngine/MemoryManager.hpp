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
T* onew(Args&&... args) {
	T* mem = static_cast<T*>(MemoryManager::allocate(sizeof(T)));
	new(mem)T(std::forward<Args>(args)...);
	return mem;
}

template<class T>
void odelete(T* obj) {
	obj->~T();
	MemoryManager::deallocate(static_cast<void*>(obj));
}

template<class T, class... Args>
std::shared_ptr<T> MakeShared(Args&&... args) {
	return std::shared_ptr<T>{ onew<T>(std::forward<Args>(args)...), odelete<T> };
}

#endif // memory_manager_hpp