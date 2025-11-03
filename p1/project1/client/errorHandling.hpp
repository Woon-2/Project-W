#ifndef __errorHandling_HPP
#define __errorHandling_HPP

#include "pch.hpp"

#include <system_error>

// @param result 실행된 함수의 반환값
// @param willExit 예외 발생 시 종료 여부
// @brief result 값이 false로 평가되면 std::wcout을 통해,
// GetLastError 함수로 반환되는 예외 코드를 해당 예외 코드의 자연어 해석과 함께 출력한다.
#define DISPLAY_ERROR_GLE(result, willExit)	\
	{	\
		auto __dp_e_gle_result = (result);	\
		if (!__dp_e_gle_result) {	\
			std::wcout << L"[Error Code: " << GetLastError() << L"] - " << errorMsgGLE()	\
				<< L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}	

// @param hr 실행된 함수의 반환값(HRESULT 타입)
// @param willExit 예외 발생 시 종료 여부
// @brief hr 값이 0보다 작으면 std::wcout을 통해,
// hr과 함께 hr이 나타내는 예외 내용을 해석한 자연어 문장을 출력한다.
#define DISPLAY_ERROR_HR(hr, willExit)	\
	{	\
		auto __dp_e_hr_result = (hr);	\
		if (__dp_e_hr_result < 0) {	\
			std::wcout << L"[Error Code: " << __dp_e_hr_result << L"] - " << errorMsgHR(__dp_e_hr_result)	\
				<< L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
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
			std::wcout << (msg) << L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}

#ifdef DXGI_DEBUG_INFO

// @param hr 실행된 함수의 반환값(HRESULT 타입)
// @param willExit 예외 발생 시 종료 여부
// @brief hr 값이 0보다 작으면 std::wcout과 std::cout을 통해,
// hr과 함께 hr이 나타내는 예외 내용을 해석한 자연어 문장을 출력한다.
// 또한 DXGI Info Queue에 저장된 예외 내용을 출력한다.
// (실행 전에 DXGIDebugInfo::init이 정상 호출 완료되어야 한다.)
#define DISPLAY_ERROR_DX_HR(hr, willExit)	\
	{	\
		auto __dp_e_hr_result = (hr);	\
		if (__dp_e_hr_result < 0) {	\
			std::wcout << L"[Error Code: " << __dp_e_hr_result << L"] - " << errorMsgHR(__dp_e_hr_result)	\
				<< L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			if (!DXGIDebugInfo::infoQ) {	\
				std::wcout << L"[DXGIDebugInfo] DXGIDebugInfo::infoQ가 활성화되지 않았습니다. "	\
					L"DXGI에서 발생한 자세한 예외의 정보를 확인할 수 없습니다.\n";	\
			}	\
			else {	\
				DXGIDebugInfo::dump(std::cout, true);	\
			}	\
				\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}

// @param voidCall 검사할 함수 호출
// @param willExit 예외 발생 시 종료 여부
// @brief voidCall을 수행 후 DXGI Info Queue에 예외 메시지가
// 추가되었다면 std::wcout과 std::cout을 통해 그 내용을 출력한다.
// (실행 전에 DXGIDebugInfo::init이 정상 호출 완료되어야 한다.)
#define DISPLAY_ERROR_DX_VOID(voidCall, willExit)	\
	{	\
		if (!DXGIDebugInfo::infoQ) {	\
			std::wcout << L"[DXGIDebugInfo] DXGIDebugInfo::infoQ가 활성화되지 않았습니다. "	\
				L"DISPLAY_ERROR_DX_VOID로 예외를 감지할 수 없습니다."	\
				L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			(voidCall);	\
		}	\
		else {	\
			auto __dp_e_vc_numMsg = DXGIDebugInfo::infoQ	\
				->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);	\
			(voidCall);	\
			auto __dp_e_vc_numMsg_After = DXGIDebugInfo::infoQ	\
				->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);	\
			if (__dp_e_vc_numMsg_After > __dp_e_vc_numMsg) {	\
				std::wcout << L"[DXVoid]: " << L#voidCall << L"] - 예외발생"	\
					<< L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
				DXGIDebugInfo::dump(std::cout, true);	\
				if (willExit) {	\
					std::exit(-1);	\
				}	\
			}	\
		}	\
	}

#else	// if not defined(DXGI_DEBUG_INFO)

// @param hr 실행된 함수의 반환값(HRESULT 타입)
// @param willExit 예외 발생 시 종료 여부
// @brief hr 값이 false로 평가되지 않으면 std::wcout을 통해,
// hr과 함께 hr이 나타내는 예외 내용을 해석한 자연어 문장을 출력한다.
// DXGI의 자세한 예외 내용을 알고 싶으면 DXGI_DEBUG_INFO 매크로를 활성화한다.
#define DISPLAY_ERROR_DX_HR(hr, willExit)	\
	{	\
		auto __dp_e_hr_result = (hr);	\
		if (__dp_e_hr_result < 0) {	\
			std::wcout << L"[Error Code: " << hr << L"] - " << errorMsgHR(hr)	\
				<< L", from file " << __FILE__ << L", line " << __LINE__ << '\n';	\
			if (willExit) {	\
				std::exit(-1);	\
			}	\
		}	\
	}

// @param voidCall 검사할 함수 호출
// @param willExit 예외 발생 시 종료 여부
// @brief voidCall을 수행한다. willExit 매개변수는 사용되지 않는다.
// DXGI의 자세한 예외 내용을 알고 싶으면 DXGI_DEBUG_INFO 매크로를 활성화한다.
#define DISPLAY_ERROR_DX_VOID(voidCall, willExit) (voidCall)

#endif	// DXGI_DEBUG_INFO

// GetLastError 함수를 통해 발견되는 예외 코드를 자연어 문장으로 해석해
// std::wstring으로 돌려준다.
inline std::wstring errorMsgGLE() {
	auto msg = std::system_category().message(GetLastError());
	auto ret = std::wstring(msg.size(), L'\0');
	std::mbstowcs(ret.data(), msg.data(), msg.size());
	return ret;
}

// 예외 코드 값을 가진 HRESULT 변수를 전달받았을 때,
// 해당 예외 코드를 자연어 문장으로 해석해 std::wstring으로 돌려준다.
inline std::wstring errorMsgHR(HRESULT hResult) {
	auto msg = std::system_category().message(hResult);
	auto ret = std::wstring(msg.size(), L'\0');
	std::mbstowcs(ret.data(), msg.data(), msg.size());
	return ret;
}

#ifdef DXGI_DEBUG_INFO
namespace DXGIDebugInfo
{
extern ComPtr<IDXGIDebug1> debug;	// reportLiveObjects를 수행하기 위한 디버그 객체
extern ComPtr<IDXGIInfoQueue> infoQ;	// std::cout, 혹은 파일로 DXGI 예외를 출력하기 위해
										// DXGI 메시지들이 담긴 Info Queue를 다룰 수 있는 인터페이스 사용

// DXGI Debug 계층을 활성화한다.
// debug 객체와 infoQ 객체를 초기화한다.
void init();
// 살아있는 DXGI 객체들을 감지해 보고한다.
// 프로그램의 마지막에 불리지 않으면, 소멸될 예정인 객체들이 있음에도 불구하고
// 엉뚱하게 누수 보고를 할 수 있다.
// 추가적으로, ID3D12Object::SetName을 통해 객체의 이름을 설정하여
// 어떤 객체들이 살아있는지 확인할 수 있도록 하자.
void reportLiveObjects(DXGI_DEBUG_RLO_FLAGS flags, std::ostream& os);
// @brief 전달받은 출력 스트림으로 저장된 메시지들을 출력한다.
// @param os 출력 스트림
// @param willClear 출력 후 infoQ를 비울지 여부
void dump(std::ostream& os, bool willClear);
}	// namespace DXGIDebugInfo
#endif	// DXGI_DEBUG_INFO

template<typename T>
inline void setD3DName(T* obj, const std::wstring& name)
{
#ifdef DXGI_DEBUG_INFO
	if (obj) obj->SetName(name.c_str());
#endif
}

template<typename T>
inline void setDXName(T* obj, const std::wstring& name)
{
#ifdef DXGI_DEBUG_INFO
	if (obj) {
		obj->SetPrivateData(WKPDID_D3DDebugObjectNameW,
			static_cast<UINT>(name.size() * sizeof(wchar_t)), name.data()
		);
	}
#endif
}

#endif	//__errorHandling_HPP