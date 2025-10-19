#ifndef __errorHandling_HPP
#define __errorHandling_HPP

#include "pch.hpp"

#include <system_error>

// @param result 실행된 함수의 반환값
// @param willExit 오류 발생시 종료 여부
// @brief result 값이 0, nullptr, 혹은 false이면
// std::cout을 통해, GetLastError 함수로 반환되는 예외 코드를
// 해당 예외 코드의 자연어 해석과 함께 출력한다.
#define DISPLAY_ERROR_GLE(result, willExit)	\
	{	\
		auto __dp_e_gle_result = (result);	\
		if (!__dp_e_gle_result) {	\
			std::cout << "[Error Code: " << GetLastError() << "] - " << errorMsgGLE()	\
				<< ", from file " << __FILE__ << ", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}	

// @param hr 실행된 함수의 반환값(HRESULT 타입)
// @param willExit 오류 발생시 종료 여부
// @brief result 값이 0, nullptr, 혹은 false이면
// std::cout을 통해, GetLastError 함수로 반환되는 예외 코드를
// 해당 예외 코드의 자연어 해석과 함께 출력한다.
#define DISPLAY_ERROR_HR(hr, willExit)	\
	{	\
		auto __dp_e_hr_result = (hr);	\
		if (!__dp_e_hr_result) {	\
			std::cout << "[Error Code: " << hr << "] - " << errorMsgHR(hr)	\
				<< ", from file " << __FILE__ << ", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}	

// GetLastError 함수를 통해 발견되는 예외 코드를 자연어 문장으로 해석해
// std::string으로 돌려준다.
inline std::string errorMsgGLE() {
	return std::system_category().message(GetLastError());
}

// 예외 코드 값을 가진 HRESULT 변수를 전달받았을 때,
// 해당 예외 코드를 자연어 문장으로 해석해 std::string으로 돌려준다.
inline std::string errorMsgHR(HRESULT hResult) {
	return std::system_category().message(hResult);
}

#endif	//__errorHandling_HPP