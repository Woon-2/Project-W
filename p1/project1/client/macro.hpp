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

#endif // MACRO_HPP