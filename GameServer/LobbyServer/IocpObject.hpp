#ifndef iocp_object_hpp
#define iocp_object_hpp

#include "simpleWindows.hpp"

#include "../common/types.hpp"

class IoEvent;

class IocpObject {
public:
	IocpObject() = default;
	virtual ~IocpObject() = default;

	virtual void dispatch(IoEvent* ioEvent, int32 numBytes) = 0;
	// SOCKET 타입을 HANDLE 타입으로 변환하여 반환하는 멤버 함수
	virtual HANDLE getHandle() const = 0;
	virtual SOCKET getSocket() const = 0;
};

#endif	// iocp_object_hpp