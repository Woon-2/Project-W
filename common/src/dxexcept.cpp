#include "dxexcept.hpp"

#ifdef ENABLE_DXGI_INFO
#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>

#define DXINFO_THROW_FAILED(hrcall) \
    if (auto hr = (hrcall); hr < 0) { \
        throw DXInfoQException(__LINE__, __FILE__, detail::makeHRDesc(hr)); \
    }

namespace gfx {

void DXInfoQueue::init() {
    DXINFO_THROW_FAILED(
        DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), &pDebug)
    );
    DXINFO_THROW_FAILED(
        DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), &pInfoQ)
    );
}

void DXInfoQueue::dump(std::ostream& os) {
    UINT64 msgCnt = pInfoQ->GetNumStoredMessages(DXGI_DEBUG_ALL);

    for (UINT64 i = 0; i < msgCnt; ++i) {
        SIZE_T msgLen = 0u;
        DXINFO_THROW_FAILED(
            pInfoQ->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &msgLen)
        );

        auto bytes = std::vector<std::uint8_t>(msgLen);
        auto pMsg = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(bytes.data());
        
        DXINFO_THROW_FAILED(
            pInfoQ->GetMessage(DXGI_DEBUG_ALL, i, pMsg, &msgLen)
        );

        os << std::string_view(
            pMsg->pDescription, pMsg->DescriptionByteLength
        ) << '\n';
    }
}

void DXInfoQueue::cleanup() {
    DXINFO_THROW_FAILED(
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL)
    );
}

};  // namespace gfx

#endif // ENABLE_DXGI_INFO