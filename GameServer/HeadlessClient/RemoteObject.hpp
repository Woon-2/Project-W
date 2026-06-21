#ifndef headless_remote_object_hpp
#define headless_remote_object_hpp

#include "Common.hpp"

namespace hc {

// (확장 슬롯) 다른 플레이어/NPC 의 마지막 알려진 상태. MVP 에서는 사용하지 않으며,
// NPC 타겟팅·상태 검증 등 후속 기능에서 BotSession 이 채워 넣도록 구조만 준비한다.
struct RemoteObject {
	uint16            id   = 0;
	ObjectType        type = ObjectType::Player;
	DirectX::XMFLOAT3 pos{0, 0, 0};
};

} // namespace hc

#endif // headless_remote_object_hpp
