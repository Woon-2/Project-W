#ifndef room_server_log_hpp
#define room_server_log_hpp

// 아래 매크로의 활성화 여부에 따라 로깅이 활성화/비활성화된다.
// 로깅 기능이 필요 없다면 아래 #define 지시문을 주석 처리하자.
#define ENABLE_LOG

#include <map>
#include <iostream>
#include <string>
#include <sstream>

template <class CharT>
struct NullBuffer : public std::basic_streambuf<CharT> {
	using int_type = std::basic_streambuf<CharT>::int_type;
    int_type overflow(int_type c) override { return c; } // 아무것도 안 함
};

// std::basic_ostream과 호환되면서
// operator<<, write 등의 함수에서 아무 일도 하지 않는 클래스
// 로깅 기능의 비활성화 시에 사용된다.
template <class CharT>
class NullStream : public std::basic_ostream<CharT> {
public:
    NullStream() : std::basic_ostream<CharT>(&nullBuffer_) {}
private:
    NullBuffer<CharT> nullBuffer_;
};

// pushLoggerA, popLoggerA 등의 함수에서 이곳에 스트림들을 등록/제거한다.
// dumpLog() 호출 시 gSharedLog의 내용이 스트림들에 복사되어 출력된다.
extern std::map<std::string, std::ostream*> gLoggers;
// pushLoggerW, popLoggerW 등의 함수에서 이곳에 스트림들을 등록/제거한다.
// dumpLog() 호출 시 gwSharedLog의 내용이 스트림들에 복사되어 출력된다.
extern std::map<std::string, std::wostream*> gwLoggers;

#ifdef ENABLE_LOG
// std::cout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
extern std::ostringstream gSharedLog;
// std::wcout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
extern std::wostringstream gwSharedLog;
#else
// std::cout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
extern NullStream<char> gSharedLog;
// std::wcout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
extern NullStream<wchar_t> gwSharedLog;
#endif

#ifdef ENABLE_LOG
inline void pushLoggerA(const std::string& key, std::ostream* pLogger) {
	gLoggers.try_emplace(key, pLogger);
}

inline void pushLoggerW(const std::string& key, std::wostream* pLogger) {
	gwLoggers.try_emplace(key, pLogger);
}

inline void popLoggerA(const std::string& key) {
	gLoggers.erase(key);
}

inline void popLoggerW(const std::string& key) {
	gwLoggers.erase(key);
}
#else
inline void pushLoggerA(const std::string& key, std::ostream* pLogger) {}
inline void pushLoggerW(const std::string& key, std::wostream* pLogger) {}
inline void popLoggerA(const std::string& key) {}
inline void popLoggerW(const std::string& key) {}
#endif

// gSharedLog와 gwSharedLog에 기록된 로그들을
// pushLoggerA, pushLoggerW 등의 함수로 등록된 모든 스트림들에 출력한다.
// ENABLE_LOG 매크로가 활성화되어있지 않다면, 아무 일도 하지 않는다.
void dumpLog();


#endif // room_server_log_hpp