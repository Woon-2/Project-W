#ifndef entry_ticket_hpp
#define entry_ticket_hpp

#include "types.hpp"
#include "protocol.hpp"

#include <array>
#include <string>

/*--------------------
     EntryTicket
--------------------*/

// 로비→룸 입장 티켓의 발급(로비서버)과 검증(룸서버).
//
// 왜 필요한가: 로비서버와 룸서버 사이에는 소켓이 없다. 클라가 S_GameStart를 받고
// 소켓을 룸서버로 갈아탄 뒤 C_Enter를 보내는 구조라, 클라가 나르는 값은 서명하지 않으면
// 전부 위조 가능하다. 룸서버는 그 값만 보고 accountId를 확정해 DB에 인벤토리를 쓰므로
// 위조를 막지 못하면 남의 계정 데이터를 덮어쓸 수 있다.
//
// 비밀키는 security_config.json에서 읽어 양쪽 서버가 공유한다(클라에는 절대 넣지 않는다).
// 상세: ServerEngine/docs/entryTicket.md
namespace EntryTicketDetail {
constexpr int32 kSecretSize = 32;

// 로비/룸이 다른 머신이면 wall clock이 조금씩 어긋난다. 이 정도 음수 오차는 허용한다.
constexpr int64 kClockSkewToleranceMs = 5000;
}

enum class EntryTicketResult : uint8 {
	Ok,
	NotInitialized,      // 서버가 security_config.json을 안 읽었다(설정 버그)
	BadVersion,          // payload 레이아웃 버전 불일치 — 로비/룸 배포 버전이 다르다
	BadMac,              // 서명 불일치 — 위조이거나 비밀키가 서로 다르다
	Expired,
	NotYetValid,         // 발급 시각이 미래 — 시계 어긋남
	MalformedNickname,   // 널 종료 없음
	MalformedLobbyCode,  // 널 종료 없음 또는 빈 값
};

class EntryTicketAuthority {
public:
	// 프로세스 기동 시 1회. 리스너가 뜨기 전에 호출해야 한다.
	static void init( const byte* secret, int32 secretSize, int32 ttlSeconds );

	// 로비서버 전용. 실패(false)는 입력 형식 오류 또는 CNG 오류.
	static bool mint( int64 accountId, const wchar_t* nickname, const std::string& lobbyCode,
		EntryTicket& out );

	// 룸서버 전용. IOCP 워커에서 호출되며 블로킹하지 않는다(순수 CPU).
	static EntryTicketResult verify( const EntryTicket& ticket );

	// 로그용.
	static const char* toString( EntryTicketResult result );

private:
	static std::array<byte, EntryTicketDetail::kSecretSize> secret_;
	static int32 ttlSeconds_;
	static bool  initialized_;
};

#endif // entry_ticket_hpp
