#ifndef send_buffer_hpp
#define send_buffer_hpp

#include "ObjectPool.hpp"
#include "globalTLS.hpp"

/*
* 사용자의 입장에서 볼 때, 괜찮은 인터페이스가 되지 못할 수도 있다.
* SendBufferManager는 open할 때, thread local인 SendBufferChunk에게 메모리를 할당해준다.
* 따라서 프로그램이 종료될 때, clear 함수를 호출해주어야 한다.
* open은 여러 thread의 send 요청에 쓰이므로 그 사용 위치가 애매할 수 있다.
* 하지만 clear는 main thread에서 호출해주어야 하므로, 인터페이스가 좋지 않다고 생각한다.
*
* 그리고 SendBufferChunk의 open으로 할당된 SendBuffer는 Session의 registerSend 함수에서 문제가 생기거나,
* processSend 함수에서 send event의 sendBuffers_ 안에 있는 buffer들을 모두 해제하고 있기 때문에
* 서버가 정상적으로 동작하고 있는 중에는 메모리 누수가 발생하지 않는다.
* 하지만 서버가 shutdown 된다면, SendBufferManager의 open이 호출되어 있는 경우에는 메모리 누수가 발생할 가능성이 있다.
* 만약 서버의 shutdown을 정상적으로 처리하고 싶다면, manager의 open으로 얻은 SendBuffer를 손수 한땀한땀 해제해주어야 한다.
* 
* shared_ptr을 사용한다면 위의 문제들을 어느 정도 해결할 수 있을 것 같지만, shared_ptr의 성능이 걱정된다.
* 
* [ 2026.04.02 ]
* SendBufferManager와 그에 따른 SendBufferChunk의 open 함수에서 shared_ptr을 반환하도록 수정하였다.
* 왜냐하면 여러 session에서 같은 SendBuffer의 포인터를 참조할 수 있기 때문이다.
* 정확히는 send 완료 후에 SendBuffer(raw pointer)를 해제할 때 double free가 발생할 수 있기 때문이다.
*/

/*------------------
     SendBuffer
------------------*/

class SendBufferChunk;

class SendBuffer {
public:
    SendBuffer(const std::shared_ptr<SendBufferChunk>& owner, byte* buffer, uint32 allocSize)
        : owner_(owner), buffer_(buffer), allocSize_(allocSize), writeSize_(0) {}

    void close(uint32 writeSize);

    byte* data() { return buffer_; }
	uint32 allocSize() const { return allocSize_; }
	uint32 writeSize() const { return writeSize_; }

private:
    std::shared_ptr<SendBufferChunk> owner_;
    byte* buffer_;
	uint32 allocSize_;
	uint32 writeSize_;
};

/*-----------------------
     SendBufferChunk
-----------------------*/

class SendBufferChunk : public std::enable_shared_from_this<SendBufferChunk> {
public:
    SendBufferChunk() : buffer_(), open_(false), usedSize_(0u) {}

    std::shared_ptr<SendBuffer> open(uint32 size) {
        ASSERT_CRASH(size <= chunkSize_);
        ASSERT_CRASH(!open_);

        open_ = true;
		return ObjectPool<SendBuffer>::makeShared(shared_from_this(), buffer(), size);
    }

    void close(uint32 writeSize) {
		ASSERT_CRASH(open_);
        open_ = false;
		usedSize_ += writeSize;
    }

    void reset() {
        open_ = false;
        usedSize_ = 0;
    }

    byte* buffer() { return &buffer_[usedSize_]; }
	bool isOpen() const { return open_; }
	uint32 freeSize() const { return chunkSize_ - usedSize_; }

private:
	static const uint32 chunkSize_{8192u};
	std::array<byte, chunkSize_> buffer_;
	bool open_;
	uint32 usedSize_;
};

/*-------------------------
     SendBufferManager
-------------------------*/

/**
* @brief SingletonBase
*/
class SendBufferManager {
public:
    static std::shared_ptr<SendBuffer> open( uint32 size );

private:
    static void push( SendBufferChunk* chunk );
    static std::shared_ptr<SendBufferChunk> pop();

	static ccqueue< std::shared_ptr<SendBufferChunk> > sendBufferChunks_;
};

#endif // send_buffer_hpp