#ifndef IOCP_ObjECT_HPP
#define IOCP_ObjECT_HPP

#include "pch.hpp"
#include "IoEvent.hpp"

class IocpObject {
public:
	IocpObject( ) = default;
	virtual ~IocpObject( ) {}

	virtual HANDLE getHandle( ) = 0;
	virtual SOCKET getSock( ) = 0;
	virtual void dispatch( const IoEvent& e, int32 numBytes ) = 0;
};

#endif // IOCP_ObjECT_HPP