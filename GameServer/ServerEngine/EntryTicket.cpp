#include "sepch.hpp"
#include "EntryTicket.hpp"
#include "HmacSha256.hpp"

#include <bcrypt.h>
#include <chrono>

/*--------------------
     EntryTicket
--------------------*/

std::array<byte, EntryTicketDetail::kSecretSize> EntryTicketAuthority::secret_{};
int32 EntryTicketAuthority::ttlSeconds_ = 0;
bool  EntryTicketAuthority::initialized_ = false;

namespace {

// wall clock을 쓴다. steady_clock은 프로세스마다 epoch가 달라 로비가 찍은 시각을
// 룸서버가 해석할 수 없다.
int64 nowUtcMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch() ).count();
}

bool generateNonce( uint64& out ) {
	return ::BCryptGenRandom( nullptr, reinterpret_cast<byte*>( &out ),
		static_cast<ULONG>( sizeof( out ) ), BCRYPT_USE_SYSTEM_PREFERRED_RNG ) >= 0;
}

}

void EntryTicketAuthority::init( const byte* secret, int32 secretSize, int32 ttlSeconds ) {
	ASSERT_CRASH( secret != nullptr );
	ASSERT_CRASH( secretSize == EntryTicketDetail::kSecretSize );
	ASSERT_CRASH( ttlSeconds > 0 );

	std::memcpy( secret_.data(), secret, EntryTicketDetail::kSecretSize );
	ttlSeconds_ = ttlSeconds;
	initialized_ = true;
}

bool EntryTicketAuthority::mint( int64 accountId, const wchar_t* nickname,
	const std::string& lobbyCode, EntryTicket& out ) {

	if ( !initialized_ || nickname == nullptr ) {
		return false;
	}

	const size_t nickLen = wcsnlen_s( nickname, kNicknameMax );
	if ( nickLen == 0 || nickLen >= kNicknameMax ) {
		return false;
	}
	if ( lobbyCode.empty() || lobbyCode.size() >= sizeof( out.payload.lobbyCode ) ) {
		return false;
	}

	// 값 초기화가 필수다. 남는 바이트가 그대로 MAC 입력에 들어가므로
	// 초기화하지 않으면 같은 입력이라도 매번 다른 MAC이 나온다.
	out = EntryTicket{};

	const int64 issuedAt = nowUtcMs();

	out.payload.version = kEntryTicketVersion;
	out.payload.accountId = accountId;
	std::wmemcpy( out.payload.nickname, nickname, nickLen );
	std::memcpy( out.payload.lobbyCode, lobbyCode.data(), lobbyCode.size() );
	out.payload.issuedAtUtcMs = issuedAt;
	out.payload.expiresAtUtcMs = issuedAt + static_cast<int64>( ttlSeconds_ ) * 1000;

	if ( !generateNonce( out.payload.nonce ) ) {
		return false;
	}

	return HmacSha256::compute( secret_.data(), EntryTicketDetail::kSecretSize,
		reinterpret_cast<const byte*>( &out.payload ), sizeof( out.payload ), out.mac );
}

EntryTicketResult EntryTicketAuthority::verify( const EntryTicket& ticket ) {
	if ( !initialized_ ) {
		return EntryTicketResult::NotInitialized;
	}

	// MAC을 먼저 본다. 서명이 깨진 티켓의 내용은 아무 의미가 없으므로
	// 그 값으로 만료·형식을 판정해봐야 공격자에게 정보만 준다.
	if ( !HmacSha256::verify( secret_.data(), EntryTicketDetail::kSecretSize,
		reinterpret_cast<const byte*>( &ticket.payload ), sizeof( ticket.payload ), ticket.mac ) ) {
		return EntryTicketResult::BadMac;
	}

	if ( ticket.payload.version != kEntryTicketVersion ) {
		return EntryTicketResult::BadVersion;
	}

	const int64 now = nowUtcMs();
	if ( now > ticket.payload.expiresAtUtcMs ) {
		return EntryTicketResult::Expired;
	}
	if ( now + EntryTicketDetail::kClockSkewToleranceMs < ticket.payload.issuedAtUtcMs ) {
		return EntryTicketResult::NotYetValid;
	}

	if ( wcsnlen_s( ticket.payload.nickname, kNicknameMax ) >= kNicknameMax ) {
		return EntryTicketResult::MalformedNickname;
	}

	const size_t codeLen = strnlen_s( ticket.payload.lobbyCode, sizeof( ticket.payload.lobbyCode ) );
	if ( codeLen == 0 || codeLen >= sizeof( ticket.payload.lobbyCode ) ) {
		return EntryTicketResult::MalformedLobbyCode;
	}

	return EntryTicketResult::Ok;
}

const char* EntryTicketAuthority::toString( EntryTicketResult result ) {
	switch ( result ) {
	case EntryTicketResult::Ok:                 return "Ok";
	case EntryTicketResult::NotInitialized:     return "NotInitialized";
	case EntryTicketResult::BadVersion:         return "BadVersion";
	case EntryTicketResult::BadMac:             return "BadMac";
	case EntryTicketResult::Expired:            return "Expired";
	case EntryTicketResult::NotYetValid:        return "NotYetValid";
	case EntryTicketResult::MalformedNickname:  return "MalformedNickname";
	case EntryTicketResult::MalformedLobbyCode: return "MalformedLobbyCode";
	}
	return "Unknown";
}
