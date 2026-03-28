#ifndef network_address_hpp
#define network_address_hpp

#include "simpleWindows.hpp"
#include <string>
#include <array>

#include "../common/types.hpp"

class NetAddress{
public:
	NetAddress() : ip_(), port_(0u), sockAddr_() {}

	// INADDR_ANY(0.0.0.0)를 사용하여 로컬 호스트의 특정 포트로 바인드할 주소를 생성한다.
	// 소켓을 모든 로컬 네트워크 인터페이스에 바인드할 수 있도록 한다.
	// 일반적으로 서버 listen 소켓의 bind() 호출에 사용된다.
	NetAddress(uint16 port) : ip_(), port_(port), sockAddr_() {
		sockAddr_.sin_family = AF_INET;
		sockAddr_.sin_port = ::htons(port);
		sockAddr_.sin_addr.s_addr = ::htonl(INADDR_ANY);
	}

	// 특정 IPv4 주소와 포트로 네트워크 주소를 생성한다.
	// 이 생성자는 소켓 사용 방식에 따라
	// - 특정 로컬 네트워크 인터페이스에 바인드하기 위한 주소로,
	// - 또는 원격 호스트를 지정하기 위한 주소로
	// 사용할 수 있다.
	NetAddress(const std::string& ip, uint16 port) : ip_(ip), port_(port), sockAddr_() {
		sockAddr_.sin_family = AF_INET;
		sockAddr_.sin_port = ::htons(port);
		::inet_pton(AF_INET, ip.c_str(), &sockAddr_.sin_addr);
	}
	
	NetAddress(const SOCKADDR_IN& sockAddr) : ip_(), port_(::ntohs(sockAddr.sin_port)), sockAddr_(sockAddr) {
		auto ipBuffer = std::array<char, INET_ADDRSTRLEN>();
		::inet_ntop(AF_INET, &sockAddr_.sin_addr, ipBuffer.data(), INET_ADDRSTRLEN);
		ip_ = ipBuffer.data();
	}

	const std::string& ip() const { return ip_; }
	uint16 port() const { return port_; }
	const SOCKADDR_IN& sockAddr() const { return sockAddr_; }

private:
	std::string ip_;
	uint16 port_;
	SOCKADDR_IN sockAddr_;
};

#endif	// network_address_hpp