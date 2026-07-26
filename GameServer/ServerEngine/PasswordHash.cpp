#include "sepch.hpp"
#include "PasswordHash.hpp"

#include <bcrypt.h>

/*--------------------
     PasswordHash
--------------------*/

namespace {
	// bcrypt.h의 반환형 NTSTATUS는 음수가 실패다 (NT_SUCCESS 매크로는 ntdef.h에 있어 직접 정의).
	bool ntSuccess( NTSTATUS status ) {
		return status >= 0;
	}
}

namespace PasswordHash {

bool generateSalt( byte ( &salt )[ kSaltSize ] ) {
	return ntSuccess( ::BCryptGenRandom(
		nullptr, salt, kSaltSize, BCRYPT_USE_SYSTEM_PREFERRED_RNG ) );
}

bool hash( const char* password, const byte* salt, int32 saltSize, byte ( &out )[ kHashSize ] ) {
	if ( password == nullptr || salt == nullptr || saltSize <= 0 ) {
		return false;
	}

	BCRYPT_ALG_HANDLE hAlg{};
	if ( !ntSuccess( ::BCryptOpenAlgorithmProvider( &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0 ) ) ) {
		return false;
	}

	bool ok = false;
	BCRYPT_HASH_HANDLE hHash{};

	if ( ntSuccess( ::BCryptCreateHash( hAlg, &hHash, nullptr, 0, nullptr, 0, 0 ) ) ) {
		// 해시 입력은 salt || password. 솔트가 앞에 있어야 같은 비밀번호라도 계정마다 해시가 달라진다.
		const bool hashed =
			ntSuccess( ::BCryptHashData( hHash, const_cast<byte*>( salt ), saltSize, 0 ) ) &&
			ntSuccess( ::BCryptHashData( hHash,
				reinterpret_cast<byte*>( const_cast<char*>( password ) ),
				static_cast<ULONG>( strlen( password ) ), 0 ) ) &&
			ntSuccess( ::BCryptFinishHash( hHash, out, kHashSize, 0 ) );

		ok = hashed;
		::BCryptDestroyHash( hHash );
	}

	::BCryptCloseAlgorithmProvider( hAlg, 0 );
	return ok;
}

bool verify( const char* password, const byte* salt, int32 saltSize, const byte* expectedHash ) {
	if ( expectedHash == nullptr ) {
		return false;
	}

	byte computed[ kHashSize ]{};
	if ( !hash( password, salt, saltSize, computed ) ) {
		return false;
	}

	// 로그인 시도는 네트워크 왕복이 지배적이라 타이밍 부채널 우려가 낮아 memcmp로 충분하다.
	return memcmp( computed, expectedHash, kHashSize ) == 0;
}

}
