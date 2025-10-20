#include "errorHandling.hpp"

#ifdef DXGI_DEBUG_INFO
namespace DXGIDebugInfo
{

ComPtr<IDXGIDebug1> debug;	// reportLiveObjects를 수행하기 위한 디버그 객체
ComPtr<IDXGIInfoQueue> infoQ;	// std::cout, 혹은 파일로 DXGI 예외를 출력하기 위해
								// DXGI 메시지들이 담긴 Info Queue를 다룰 수 있는 인터페이스 사용
// DXGI Debug 계층을 활성화한다.
// debug 객체와 infoQ 객체를 초기화한다.
void init() {
	DISPLAY_ERROR_HR( DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), &debug), true );
	DISPLAY_ERROR_HR( DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), &infoQ), true );

	// Info Queue를 통해서 Corruption, Error 범주의 메시지만 얻어오도록 한다.
	// Info Queue는 std::cout이나 파일을 통해 DXGI의 예외 메시지를 출력하기 위한 용도이므로.
	// Warning, Message, Info 범주의 메시지는 얻어올 필요가 없다.
	DXGI_INFO_QUEUE_MESSAGE_SEVERITY severties[] = {
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION,
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,
	};

	auto filter = DXGI_INFO_QUEUE_FILTER{
		.AllowList = DXGI_INFO_QUEUE_FILTER_DESC{
			.NumSeverities = 2,
			.pSeverityList = severties
		}
	};

	infoQ->AddRetrievalFilterEntries(DXGI_DEBUG_ALL, &filter);
}

// 살아있는 DXGI 객체들을 감지해 보고한다.
// 프로그램의 마지막에 불리지 않으면, 소멸될 예정인 객체들이 있음에도 불구하고
// 엉뚱하게 누수 보고를 할 수 있다.
// 추가적으로, ID3D12Object::SetName을 통해 객체의 이름을 설정하여
// 어떤 객체들이 살아있는지 확인할 수 있도록 하자.
void reportLiveObjects(DXGI_DEBUG_RLO_FLAGS flags, std::ostream& os) {
	DISPLAY_ERROR_STR(debug, "[DXGIDebugInfo] DXGIDebugInfo::debug 객체가 활성화되지 않았습니다. "
		"DXGIDebugInfo::init을 먼저 호출하세요.", false
	);
	DISPLAY_ERROR_STR(infoQ, "[DXGIDebugInfo] DXGIDebugInfo::infoQ 객체가 활성화되지 않았습니다. "
		"DXGIDebugInfo::init을 먼저 호출하세요.", false
	);

	if (debug) {
		if (infoQ) {
			// ReportLiveObjects로 보고되는 것은 ERROR나 CORRUPTION이 아니다.
			// ReportLiveObjects로 보고되는 내용도 std::cout이나 파일로 확인하기 위해서는,
			// 여기서 필터를 제거해줄 필요가 있다.
			infoQ->PopRetrievalFilter(DXGI_DEBUG_ALL);
		}

		debug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);

		if (infoQ) {
			dump(os, true);
		}
	}
}

// @brief 전달받은 출력 스트림으로 저장된 메시지들을 출력한다.
// @param os 출력 스트림
// @param willClear 출력 후 infoQ를 비울지 여부
void dump(std::ostream& os, bool willClear) {
	UINT64 msgCnt = infoQ->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);

	for (UINT64 i = 0; i < msgCnt; ++i) {
		SIZE_T msgLen = 0u;
		DISPLAY_ERROR_HR(infoQ->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &msgLen), false);

		auto bytes = std::vector<std::uint8_t>(msgLen);
		auto pMsg = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(bytes.data());

		DISPLAY_ERROR_HR(infoQ->GetMessage(DXGI_DEBUG_ALL, i, pMsg, &msgLen), false);

		os << std::string_view(
			pMsg->pDescription, pMsg->DescriptionByteLength
		) << '\n';
	}
}

}	 // namespace DXGIDebugInfo
#endif	// DXGI_DEBUG_INFO