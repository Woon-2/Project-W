#ifndef MACRO_HPP
#define MACRO_HPP

#include "types.hpp"

#define CRASH(cause) {\
	int32* crash = nullptr;\
	__analysis_assume(crash != nullptr);\
	*crash = 0xDEADBEEF;\
}

#define ASSERT_CRASH(expr) {\
	if (!(expr)) {\
		CRASH("Assertion failed");\
	}\
}

// @param condition bool로 평가 가능한, false일 시 예외로 판단할 값
// @param msg 예외 발생 시 출력할 문자열
// @param willExit 예외 발생 시 종료 여부
// @brief condition 값이 false로 평가되면 std::wcout을 통해,
// msg로 전달받은 문자열을 출력한다.
#define DISPLAY_ERROR_STR(condition, msg, willExit)	\
	{	\
		auto __dp_e_str_condition = (condition);	\
		if (!__dp_e_str_condition) {	\
			gSharedLog << (msg) << ", from file " << __FILE__ << ", line " << __LINE__ << '\n';	\
			dumpLog();	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}

#endif // MACRO_HPP