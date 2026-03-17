#ifndef macro_hpp
#define macro_hpp

#define CRASH(cause) {\
	int* crash = nullptr;\
	__analysis_assume(crash != nullptr);\
	*crash = 0xDEADBEEF;\
}

#define ASSERT_CRASH(expr) {\
	if (!(expr)) {\
		CRASH("Assertion failed");\
	}\
}

#endif // macro_hpp