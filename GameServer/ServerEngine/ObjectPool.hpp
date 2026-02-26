#ifndef object_pool_hpp
#define object_pool_hpp

#include "types.hpp"
#include "MemoryPool.hpp"

/**
* @brief SingletonBase
*/
template<class T>
class ObjectPool {
public:
	static void push(T* obj) {
		obj->~T();
		pool_.push(MemoryHeader::detachHeader(obj));
	}

	template<class... Args>
	static T* pop(Args&&... args) {
		T* mem = static_cast<T*>(MemoryHeader::attachHeader(pool_.pop(), poolSize_));
		new(mem)T(std::forward<Args>(args)...);
		return mem;
	}

	template<class... Args>
	static std::shared_ptr<T> makeShared(Args&&... args) {
		return std::shared_ptr<T>{ pop(std::forward<Args>(args)...), push };
	}

private:
	static int32 poolSize_;
	static MemoryPool pool_;
};

template<class T>
int32 ObjectPool<T>::poolSize_ = sizeof(T) + sizeof(MemoryHeader);

template<class T>
MemoryPool ObjectPool<T>::pool_{poolSize_};

#endif // object_pool_hpp