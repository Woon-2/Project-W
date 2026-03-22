#ifndef IOCP_ObjECT_HPP
#define IOCP_ObjECT_HPP

// #include "pch.hpp"

class IocpObject : public std::enable_shared_from_this<IocpObject> {
public:
	IocpObject( ) = default;
	virtual ~IocpObject( ) {}

	virtual void dispatch( class IoEvent* event, int32 numBytes ) = 0;
	virtual HANDLE getHandle( ) const = 0;
	virtual SOCKET getSock( ) const = 0;
};

#endif // IOCP_ObjECT_HPP