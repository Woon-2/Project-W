#include "sepch.hpp"
#include "HmacSha256.hpp"

#include <bcrypt.h>

/*--------------------
     HmacSha256
--------------------*/

namespace {
	// bcrypt.h의 반환형 NTSTATUS는 음수가 실패다 (NT_SUCCESS 매크로는 ntdef.h에 있어 직접 정의).
	bool ntSuccess( NTSTATUS status ) {
		return status >= 0;
	}
}

namespace HmacSha256 {

bool compute( const byte* key, int32 keySize, const byte* data, int32 dataSize,
	byte ( &out )[ kMacSize ] ) {

	if ( key == nullptr || keySize <= 0 || ( data == nullptr && dataSize > 0 ) || dataSize < 0 ) {
		return false;
	}

	// BCRYPT_ALG_HANDLE_HMAC_FLAG가 있어야 BCryptCreateHash에 키를 넘길 수 있다.
	// 플래그 없이 열면 키 인자가 무시되고 그냥 SHA-256이 되어버린다.
	BCRYPT_ALG_HANDLE hAlg{};
	if ( !ntSuccess( ::BCryptOpenAlgorithmProvider(
		&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG ) ) ) {
		return false;
	}

	bool ok = false;
	BCRYPT_HASH_HANDLE hHash{};

	if ( ntSuccess( ::BCryptCreateHash( hAlg, &hHash, nullptr, 0,
		const_cast<byte*>( key ), static_cast<ULONG>( keySize ), 0 ) ) ) {

		ok = ( dataSize == 0 || ntSuccess( ::BCryptHashData( hHash,
				const_cast<byte*>( data ), static_cast<ULONG>( dataSize ), 0 ) ) ) &&
			ntSuccess( ::BCryptFinishHash( hHash, out, kMacSize, 0 ) );

		::BCryptDestroyHash( hHash );
	}

	::BCryptCloseAlgorithmProvider( hAlg, 0 );
	return ok;
}

bool verify( const byte* key, int32 keySize, const byte* data, int32 dataSize,
	const byte* expected ) {

	if ( expected == nullptr ) {
		return false;
	}

	byte computed[ kMacSize ]{};
	if ( !compute( key, keySize, data, dataSize, computed ) ) {
		return false;
	}

	// 상수 시간 비교. 이 경로는 공격자가 포트 9000에 붙어 자유롭게 반복 시도할 수 있어,
	// memcmp의 조기 반환이 MAC 바이트를 한 개씩 맞춰나가는 단서가 된다.
	// volatile 누산으로 컴파일러가 조기 탈출로 최적화하지 못하게 막는다.
	volatile byte diff = 0;
	for ( int32 i = 0; i < kMacSize; ++i ) {
		diff = static_cast<byte>( diff | ( computed[ i ] ^ expected[ i ] ) );
	}
	return diff == 0;
}

}
