#include "log.hpp"

// pushLoggerA, popLoggerA 등의 함수에서 이곳에 스트림들을 등록/제거한다.
// dumpLog() 호출 시 gSharedLog의 내용이 스트림들에 복사되어 출력된다.
std::atomic<bool> gLogMuted{ false };

std::map<std::string, std::ostream*> gLoggers;
// pushLoggerW, popLoggerW 등의 함수에서 이곳에 스트림들을 등록/제거한다.
// dumpLog() 호출 시 gwSharedLog의 내용이 스트림들에 복사되어 출력된다.
std::map<std::string, std::wostream*> gwLoggers;

#ifdef ENABLE_LOG
// std::cout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
std::ostringstream gSharedLog;
// std::wcout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
std::wostringstream gwSharedLog;
#else
// std::cout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
NullStream<char> gSharedLog;
// std::wcout처럼 사용한다.
// 이곳에 쓰인 로그들은 dumpLog 함수 호출 시 출력된다.
NullStream<wchar_t> gwSharedLog;
#endif

#ifdef ENABLE_LOG
// gSharedLog와 gwSharedLog에 기록된 로그들을
// pushLoggerA, pushLoggerW 등의 함수로 등록된 모든 스트림들에 출력한다.
// ENABLE_LOG 매크로가 활성화되어있지 않다면, 아무 일도 하지 않는다.
void dumpLog() {
	if (gLogMuted.load(std::memory_order_relaxed)) {
		return;
	}

	auto log = gSharedLog.rdbuf()->str();
	for (auto& [key, logger] : gLoggers) {
		logger->write(log.data(), log.size());
	}
	gSharedLog.str("");
	gSharedLog.clear();

	auto wlog = gwSharedLog.rdbuf()->str();
	for (auto& [key, logger] : gwLoggers) {
		logger->write(wlog.data(), wlog.size());
	}
	gwSharedLog.str(L"");
	gwSharedLog.clear();
}
#else
// gSharedLog와 gwSharedLog에 기록된 로그들을
// pushLoggerA, pushLoggerW 등의 함수로 등록된 모든 스트림들에 출력한다.
// ENABLE_LOG 매크로가 활성화되어있지 않다면, 아무 일도 하지 않는다.
void dumpLog() {}
#endif