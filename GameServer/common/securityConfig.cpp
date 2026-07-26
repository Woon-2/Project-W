#include "securityConfig.hpp"

#include "simpleJson.hpp"
#include "simpleWindows.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kConfigFileName[] = L"security_config.json";

bool executableDirectory(std::filesystem::path& out, std::string& error) {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            error = "GetModuleFileNameW failed: " + std::to_string(GetLastError());
            return false;
        }
        if (length < buffer.size()) {
            out = std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
            return true;
        }
        if (buffer.size() >= 32768) {
            error = "executable path is too long";
            return false;
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool findConfigPath(std::filesystem::path& out, std::string& error) {
    std::filesystem::path dir;
    if (!executableDirectory(dir, error)) return false;

    const auto startDir = dir;
    for (;;) {
        const auto candidate = dir / kConfigFileName;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            out = candidate;
            return true;
        }

        const auto parent = dir.parent_path();
        if (parent.empty() || parent == dir) break;
        dir = parent;
    }

    error = "security_config.json was not found at or above executable directory: "
        + startDir.string();
    return false;
}

bool hexNibble(char c, std::uint8_t& out) {
    if (c >= '0' && c <= '9') { out = static_cast<std::uint8_t>(c - '0');      return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<std::uint8_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<std::uint8_t>(c - 'A' + 10); return true; }
    return false;
}

bool decodeHexSecret(const std::string& hex, std::array<std::uint8_t, 32>& out, std::string& error) {
    if (hex.size() != out.size() * 2) {
        error = "entryTicket.secretHex must be exactly "
            + std::to_string(out.size() * 2) + " hex characters";
        return false;
    }

    for (std::size_t i = 0; i < out.size(); ++i) {
        std::uint8_t hi = 0;
        std::uint8_t lo = 0;
        if (!hexNibble(hex[i * 2], hi) || !hexNibble(hex[i * 2 + 1], lo)) {
            error = "entryTicket.secretHex contains a non-hex character";
            return false;
        }
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }

    // 전부 0인 키는 설정 파일을 채우지 않고 넘어간 경우다. 서명이 사실상 무력해지므로 거부한다.
    bool allZero = true;
    for (const std::uint8_t b : out) {
        if (b != 0) { allZero = false; break; }
    }
    if (allZero) {
        error = "entryTicket.secretHex must not be all zeros";
        return false;
    }

    return true;
}

}  // namespace

bool loadSecurityConfig(SecurityConfig& out, std::filesystem::path& loadedPath, std::string& error) {
    out = {};
    loadedPath.clear();
    error.clear();

    if (!findConfigPath(loadedPath, error)) return false;

    std::ifstream input(loadedPath, std::ios::binary);
    if (!input) {
        error = "failed to open security config: " + loadedPath.string();
        return false;
    }

    const std::string text{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>{}};
    json::Value root;
    std::string parseError;
    if (!json::parse(text, root, &parseError)) {
        error = "invalid JSON in " + loadedPath.string() + ": " + parseError;
        return false;
    }
    if (!root.isObject()) {
        error = "security config root must be a JSON object";
        return false;
    }

    const json::Value* ticket = root.find("entryTicket");
    if (!ticket || !ticket->isObject()) {
        error = "entryTicket must be a JSON object";
        return false;
    }

    const json::Value* secretHex = ticket->find("secretHex");
    if (!secretHex || !secretHex->isString()) {
        error = "entryTicket.secretHex must be a string";
        return false;
    }

    const json::Value* ttl = ticket->find("ttlSeconds");
    if (!ttl || !ttl->isNumber()) {
        error = "entryTicket.ttlSeconds must be an integer from 5 to 3600";
        return false;
    }

    const double ttlValue = ttl->asNumber();
    if (!std::isfinite(ttlValue) || std::trunc(ttlValue) != ttlValue
        || ttlValue < 5.0 || ttlValue > 3600.0) {
        error = "entryTicket.ttlSeconds must be an integer from 5 to 3600";
        return false;
    }

    SecurityConfig parsed;
    if (!decodeHexSecret(secretHex->asString(), parsed.entryTicketSecret, error)) return false;
    parsed.entryTicketTtlSeconds = static_cast<std::int32_t>(ttlValue);

    out = parsed;
    return true;
}
