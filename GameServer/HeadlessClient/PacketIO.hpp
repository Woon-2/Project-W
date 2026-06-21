#ifndef headless_packet_io_hpp
#define headless_packet_io_hpp

#include "Common.hpp"
#include <vector>
#include <cstring>

namespace hc {

// 임의 패킷이라도 너무 크면 깨진 스트림으로 간주(자기방어).
inline constexpr uint16 kMaxPacketSize = 16 * 1024;

// 패킷 구조체를 바이트 버퍼에 추가. size 필드만큼만 복사한다.
template<class P>
inline void appendPacket(std::vector<char>& out, const P& pkt) {
	const char* p = reinterpret_cast<const char*>(&pkt);
	out.insert(out.end(), p, p + pkt.size);
}

// C_Enter: 로비 코드로 방 그룹화. RoomServer는 코드를 검증하지 않으므로 임의 결정 코드면 충분.
inline void buildEnter(std::vector<char>& out, const char* lobbyCode) {
	CEnterPacket pkt{};
	pkt.size = sizeof(CEnterPacket);
	pkt.type = PacketType::C_Enter;
	std::memset(pkt.lobbyCode, 0, sizeof(pkt.lobbyCode));
	// 코드는 최대 6자 + null. 넘치면 잘라 담는다(나머지는 memset 으로 이미 0).
	size_t n = std::strlen(lobbyCode);
	if (n > sizeof(pkt.lobbyCode) - 1) {
		n = sizeof(pkt.lobbyCode) - 1;
	}
	std::memcpy(pkt.lobbyCode, lobbyCode, n);
	appendPacket(out, pkt);
}

// C_Move: 실제 클라이언트와 동일한 pos + velocity (orientation 없음).
inline void buildMove(std::vector<char>& out,
                      const DirectX::XMFLOAT3& pos,
                      const DirectX::XMFLOAT3& velocity) {
	CMovePacket pkt{};
	pkt.size     = sizeof(CMovePacket);
	pkt.type     = PacketType::C_Move;
	pkt.pos      = pos;
	pkt.velocity = velocity;
	appendPacket(out, pkt);
}

} // namespace hc

#endif // headless_packet_io_hpp
