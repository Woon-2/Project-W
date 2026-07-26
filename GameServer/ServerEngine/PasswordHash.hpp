#ifndef password_hash_hpp
#define password_hash_hpp

#include "types.hpp"

/*--------------------
     PasswordHash
--------------------*/

// 비밀번호를 평문 대신 SHA-256(salt || password)로 저장하기 위한 도우미.
// Windows CNG(BCrypt) API만 사용하므로 외부 라이브러리 의존이 없다.
// 링크: EXE 쪽 pch에 #pragma comment(lib, "bcrypt.lib") 필요 (lspch.hpp 참조).
//
// 저장 형식 (db/schema.sql의 Account 테이블과 일치):
//   pwSalt BINARY(16) — 계정마다 BCryptGenRandom으로 새로 생성
//   pwHash BINARY(32) — SHA-256(salt || password)
namespace PasswordHash {

constexpr int32 kSaltSize = 16;
constexpr int32 kHashSize = 32;   // SHA-256

// 암호학적 난수로 솔트를 채운다.
bool generateSalt( byte ( &salt )[ kSaltSize ] );

// out = SHA-256(salt || password). password는 널 종료 ASCII.
bool hash( const char* password, const byte* salt, int32 saltSize, byte ( &out )[ kHashSize ] );

// 저장된 해시와 비교. 실패(false)는 불일치 또는 내부 오류.
bool verify( const char* password, const byte* salt, int32 saltSize, const byte* expectedHash );

}

#endif // password_hash_hpp
