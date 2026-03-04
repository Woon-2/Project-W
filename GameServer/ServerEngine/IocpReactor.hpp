#ifndef iocp_reactor_hpp
#define iocp_reactor_hpp

#include "simpleWindows.hpp"
#include <iostream>
#include <system_error>

#include "macro.hpp"
#include "IocpDispatchable.hpp"
#include "IoEvent.hpp"

class IocpReactor {
public:
	IocpReactor() : iocpHandle_(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)) {
		ASSERT_CRASH(iocpHandle_ != nullptr);
	}

	~IocpReactor() {
		CloseHandle(iocpHandle_);
	}

	bool registerHandle(HANDLE handle) {
		auto h = CreateIoCompletionPort(handle, iocpHandle_, 0, 0);
		return h == iocpHandle_;
	}

	bool dispatch(DWORD timeOutMs = INFINITE) {
		DWORD numBytes{};
		ULONG_PTR key{};
		IoEvent* event = nullptr;

		if (GetQueuedCompletionStatus(iocpHandle_, &numBytes, &key, reinterpret_cast<LPOVERLAPPED*>(&event), timeOutMs)) {
			auto owner = event->owner();
			owner->dispatch(event, numBytes);
		}
		else {
			const int32 errCode = WSAGetLastError();

			if (errCode == WAIT_TIMEOUT) {
				return false;
			}
			else {
				std::cerr << "GQCS Error Code - " << errCode << '\n'
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

#endif // iocp_reactor_hpp