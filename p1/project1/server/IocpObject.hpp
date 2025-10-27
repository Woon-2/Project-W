#ifndef IOCP_ObjECT_HPP
#define IOCP_ObjECT_HPP

#include "pch.hpp"

class IocpObject {
public:
	IocpObject( ) = default;
	virtual ~IocpObject( ) {}

	virtual HANDLE getHandle( ) = 0;
	virtual SOCKET getSock( ) = 0;
	virtual void dispatch( ) = 0;
};

#endif // IOCP_ObjECT_HPP