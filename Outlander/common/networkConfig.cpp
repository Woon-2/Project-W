#include "networkConfig.hpp"

#include "simpleJson.hpp"
#include "simpleWindows.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kConfigFileName[] = L"network_config.json";

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

    error = "network_config.json was not found at or above executable directory: "
        + startDir.string();
    return false;
}

bool parseEndpoint(const json::Value& root, std::string_view key,
                   NetworkEndpoint& out, std::string& error) {
    const json::Value* endpoint = root.find(key);
    if (!endpoint || !endpoint->isObject()) {
        error = std::string(key) + " must be a JSON object";
        return false;
    }

    const json::Value* ipValue = endpoint->find("ip");
    if (!ipValue || !ipValue->isString() || ipValue->asString().empty()) {
        error = std::string(key) + ".ip must be a non-empty IPv4 string";
        return false;
    }

    IN_ADDR address{};
    if (inet_pton(AF_INET, ipValue->asString().c_str(), &address) != 1) {
        error = std::string(key) + ".ip is not a valid IPv4 address: "
            + ipValue->asString();
        return false;
    }

    const json::Value* portValue = endpoint->find("port");
    if (!portValue || !portValue->isNumber()) {
        error = std::string(key) + ".port must be an integer from 1 to 65535";
        return false;
    }

    const double port = portValue->asNumber();
    if (!std::isfinite(port) || std::trunc(port) != port || port < 1.0 || port > 65535.0) {
        error = std::string(key) + ".port must be an integer from 1 to 65535";
        return false;
    }

    out.ip = ipValue->asString();
    out.port = static_cast<std::uint16_t>(port);
    return true;
}

}  // namespace

bool loadNetworkConfig(NetworkConfig& out, std::filesystem::path& loadedPath,
                       std::string& error) {
    out = {};
    loadedPath.clear();
    error.clear();

    if (!findConfigPath(loadedPath, error)) return false;

    std::ifstream input(loadedPath, std::ios::binary);
    if (!input) {
        error = "failed to open network config: " + loadedPath.string();
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
        error = "network config root must be a JSON object";
        return false;
    }

    NetworkConfig parsed;
    if (!parseEndpoint(root, "lobby", parsed.lobby, error)
        || !parseEndpoint(root, "room", parsed.room, error)) {
        return false;
    }

    out = std::move(parsed);
    return true;
}
