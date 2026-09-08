#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

struct SecurityConfig {
    std::array<std::uint8_t, 32> entryTicketSecret{};   // HMAC-SHA256 키 (secretHex 64자를 디코드)
    std::int32_t entryTicketTtlSeconds = 0;             // 티켓 유효 기간
};

// exe 디렉터리에서 부모로 거슬러 올라가며 가장 가까운 security_config.json을 찾아 파싱한다.
// 폴백 값 없음: false면 프로세스는 에러를 출력하고 종료해야 한다. (dbConfig/networkConfig와 동일 규약)
//
// 서버 전용: 이 파일을 client.vcxproj에 절대 추가하지 말 것.
// 입장 티켓의 비밀키가 클라이언트에 실리면 누구나 티켓을 위조할 수 있어
// 메커니즘 전체가 무의미해진다. network_config.json과 분리한 이유가 이것이다.
bool loadSecurityConfig(SecurityConfig& out, std::filesystem::path& loadedPath, std::string& error);
