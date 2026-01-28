#ifndef iocp_core_hpp
#define iocp_core_hpp

#include "simpleWindows.hpp"
#include <iostream>

#include "IocpObject.hpp"
#include "IoEvent.hpp"

#include "../common/macro.hpp"

class IocpCore {
public:
	IocpCore()
		: iocpHandle_(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)
	) {
		ASSERT_CRASH(iocpHandle_ != nullptr);
	}

	~IocpCore() {
		::CloseHandle(iocpHandle_);
	}

	bool registerHandle(const IocpObject* iocpObject) {
		auto h = ::CreateIoCompletionPort(iocpObject->getHandle(), iocpHandle_, 0, 0);
		return h == iocpHandle_;
	}

	bool dispatch(uint32 timeOutMs = INFINITE) {
		DWORD numBytes{};
		ULONG_PTR key{};
		IoEvent* event = nullptr;

		if (::GetQueuedCompletionStatus(iocpHandle_, &numBytes, &key,
			reinterpret_cast<LPOVERLAPPED*>(&event), timeOutMs)
		) {
			auto owner = event->owner();
			owner->dispatch(event, numBytes);
		}
		else {
			const int32 errCode = ::WSAGetLastError();

			if (errCode == WAIT_TIMEOUT) {
				return false;
			}
			else {
				std::cout << "GQCS Error Code - " << errCode << '\n'
					<< "Error Message - " << std::system_category().message(errCode) << '\n';

				auto owner = event->owner();
				owner->dispatch(event, numBytes);
			}
		}

		return true;
	}

	HANDLE iocpHandle() const { return iocpHandle_; }

private:
	HANDLE iocpHandle_;
};

#endif	// iocp_core_hpp