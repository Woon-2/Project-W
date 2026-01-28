#ifndef memory_pool_manager_hpp
#define memory_pool_manager_hpp

class MemoryPool;

class Memory {
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

template<class T, typename... Args>
T* xnew(Args&&... args) {
	T* mem = static_cast<T*>(Memory::allocate(sizeof(T)));
	new(mem)T(std::forward<Args>(args)...);
	return mem;
}

template<class T>
void xdelete(T* obj) {
	obj->~T();
	Memory::deallocate(static_cast<void*>(obj));
}

template<class T, typename... Args>
std::shared_ptr<T> MakeShared(Args&&... args) {
	return std::shared_ptr<T>{xnew<T>(std::forward<Args>(args)...), xdelete<T>};
}

#endif	// memory_pool_manager_hpp