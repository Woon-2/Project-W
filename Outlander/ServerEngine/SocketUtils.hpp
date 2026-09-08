#ifndef socket_utils_hpp
#define socket_utils_hpp

/**
* @brief SingletonBase
*/
class SocketUtils {
public:
	static void init();
	static void release();

	static SOCKET createSocket();
	static void closeSocket(SOCKET& sock);
	static void bindWindowsFuncEx(SOCKET sock, GUID guid, LPVOID* fn);

	static void setReuseAddr(SOCKET sock, bool reuse);
	static void setTcpNoDelay(SOCKET sock, bool noDelay);
	static void setUpdateAcceptContext(SOCKET sock, SOCKET listenSock);

	static void bind(SOCKET sock, const NetAddress& netAddr);
	static void listen(SOCKET sock, int32 backlog = SOMAXCONN);

public:
	static LPFN_CONNECTEX ConnectEx;
	static LPFN_DISCONNECTEX DisconnectEx;

private:
	template<class T>
	static bool setSockOpt(SOCKET sock, int32 level, int32 optName, const T& optVal) {
		return ::setsockopt(sock, level, optName, 
			reinterpret_cast<const char*>(&optVal), sizeof(optVal)) != SOCKET_ERROR;
	}
};

#endif // socket_utils_hpp