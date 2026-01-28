#ifndef object_pool_hpp
#define object_pool_hpp

#include "../common/types.hpp"
#include "MemoryPool.hpp"

template<class T>
class ObjectPool {
public:
	template<typename... Args>
	static T* pop(Args&&... args) {
		T* mem = static_cast<T*>(MemoryHeader::attachHeader(pool_.pop(), poolSize_));
		new(mem)T(std::forward<Args>(args)...);
		return mem;
	}

	static void push(T* obj) {
		obj->~T();
		pool_.push(MemoryHeader::detachHeader(obj));
	}

	static std::shared_ptr<T> makeShared() {

	}

private:
	static int32 poolSize_;
	static MemoryPool pool_;
};

template<class T>
int32 ObjectPool<T>::poolSize_ = sizeof(T) + sizeof(MemoryHeader);

template<class T>
MemoryPool ObjectPool<T>::pool_{poolSize_};

#endif	// object_pool_hpp