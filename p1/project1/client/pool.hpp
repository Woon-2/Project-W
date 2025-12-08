#ifndef __pool_HPP
#define __pool_HPP

#include <utility>

template <class T>
class Pool {
public:
	class Returner {
		Returner() = default;
		Returner(Pool* pool) : pool_(pool) {}

		void operator()(T* exhausted) noexcept {
			pool_->free(exhausted);
		}

	private:
		Pool* pool_;
	};

	Pool(std::size_t capacity)
		: pMem_( new char[ safe_mem() * capacity + sizeof(void*) ] ),
		pFree_(pMem_), availableCnt_(capacity) {
		
		char** curr = reinterpret_cast<char**>(pMem_);
		char* next = pMem_;

		for ( std::size_t i = 0; i < capacity; ++i ) {
			next += safe_mem();
			*cur = next;
			cur = reinterpret_cast<char**>( next );
		}

		*cur = nullptr;
	}

	~Pool() {
		if (pMem_) {
			delete[] pMem_;
		}
	}

	Pool(const Pool&) = delete;
	Pool& operator=(const Pool&) = delete;

	Pool(Pool&& other) noexcept
		: pMem_(std::exchange(other.pMem_, nullptr)),
		pFree_(std::exchange(other.pFree_, nullptr)),
		availableCnt_(std::exchange(other.availableCnt_, 0ull)) {}

	Pool& operator=(Pool&& other) noexcept {
		if (this == &other) {
			return *this;
		}

		pMem_ = std::exchange(other.pMem_, nullptr);
		pFree_ = std::exchange(other.pFree_, nullptr);
		availableCnt_ = std::exchange(other.availableCnt_, 0ull);

		return *this;
	}

	template <class ... Args>
	T* alloc(Args&& ... args) {
		return new( getMem() ) T( std::forward<Args>(args)... );
	}

	void free(T* exhausted) noexcept {
		if (exhausted) {
			exhausted->~T();
			putMem(reinterpret_cast<char*>(exhausted));
		}
	}

private:
	// for safe memory writing, each memory size must be over sizeof( void* ).
	static constexpr const size_t safe_mem() noexcept {
		return std::max( sizeof( T ), sizeof( void* ) );
	}

	char* getMem() noexcept
	{
		char* ret = pFree_;
		pFree_ = *reinterpret_cast<char**>(pFree_);
		--availableCnt_;
		return ret;
	}

	void putMem( char* mem ) noexcept
	{
		*reinterpret_cast<char**>(mem) = pFree_;
		pFree_ = mem;
		++availableCnt_;
	}

	char* pMem_;
	char* pFree_;
	std::size_t availableCnt_;
};

#endif	// __pool_HPP