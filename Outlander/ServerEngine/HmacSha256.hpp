#ifndef hmac_sha256_hpp
#define hmac_sha256_hpp

#include "types.hpp"

/*--------------------
     HmacSha256
--------------------*/

// HMAC-SHA256 프리미티브. Windows CNG(BCrypt)만 사용해 외부 의존이 없다.
// 링크: EXE 쪽 pch에 #pragma comment(lib, "bcrypt.lib") 필요 (lspch.hpp / rspch.hpp 참조).
//
// PasswordHash와 달리 이쪽은 공격자가 반복 시도할 수 있는 경로(룸서버 입장 검증)에 쓰이므로
// verify는 상수 시간 비교를 한다. 용도는 EntryTicket 서명 — 그쪽 문서는
// ServerEngine/docs/entryTicket.md 참조.
namespace HmacSha256 {

constexpr int32 kMacSize = 32;   // SHA-256

// out = HMAC-SHA256(key, data). 실패(false)는 CNG 내부 오류.
bool compute( const byte* key, int32 keySize, const byte* data, int32 dataSize,
	byte ( &out )[ kMacSize ] );

// expected와 상수 시간 비교. 실패(false)는 불일치 또는 내부 오류.
bool verify( const byte* key, int32 keySize, const byte* data, int32 dataSize,
	const byte* expected );

}

#endif // hmac_sha256_hpp
