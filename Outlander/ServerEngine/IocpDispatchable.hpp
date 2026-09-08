#ifndef iocp_dispatchable_hpp
#define iocp_dispatchable_hpp

#include "simpleWindows.hpp"
#include "types.hpp"
#include <memory>

class IoEvent;

class IocpDispatchable : public std::enable_shared_from_this<IocpDispatchable> {
public:
	IocpDispatchable() = default;
	virtual ~IocpDispatchable() = default;

	/**
	* @brief IOCP 완료 통지를 처리하기 위한 진입 함수.
	*
	* GetQueuedCompletionStatus로부터 전달된 I/O 완료 이벤트를 받아
	* 해당 객체의 컨텍스트에서 적절한 후속 처리를 수행한다.
	*
	* @param ioEvent   완료된 Overlapped 기반 I/O 이벤트 정보
	* @param numBytes  실제 전송(또는 수신) 완료된 바이트 수
	*/
	virtual void dispatch(IoEvent* ioEvent, int32 numBytes) = 0;

	// SOCKET 타입을 HANDLE 타입으로 변환하여 반환하는 멤버 함수
	virtual HANDLE getHandle() const = 0;
	// SOCKET을 반환하는 멤버 함수
	virtual SOCKET getSocket() const = 0;
};

#endif // iocp_dispatchable_hpp