#include "dbConfig.hpp"

#include "simpleJson.hpp"
#include "simpleWindows.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kConfigFileName[] = L"db_config.json";

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

    error = "db_config.json was not found at or above executable directory: "
        + startDir.string();
    return false;
}

// JSON 파서는 UTF-8 std::string을 주지만 ODBC(SQLDriverConnect)는 와이드 문자열을 받는다.
bool utf8ToWide(const std::string& in, std::wstring& out, std::string& error) {
    if (in.empty()) {
        out.clear();
        return true;
    }

    const int len = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), static_cast<int>(in.size()), nullptr, 0);
    if (len <= 0) {
        error = "connectionString is not valid UTF-8";
        return false;
    }

    out.resize(static_cast<size_t>(len));
    MultiByteToWideChar(
        CP_UTF8, 0, in.data(), static_cast<int>(in.size()), out.data(), len);
    return true;
}

}  // namespace

bool loadDbConfig(DbConfig& out, std::filesystem::path& loadedPath, std::string& error) {
    out = {};
    loadedPath.clear();
    error.clear();

    if (!findConfigPath(loadedPath, error)) return false;

    std::ifstream input(loadedPath, std::ios::binary);
    if (!input) {
        error = "failed to open db config: " + loadedPath.string();
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
        error = "db config root must be a JSON object";
        return false;
    }

    const json::Value* db = root.find("db");
    if (!db || !db->isObject()) {
        error = "db must be a JSON object";
        return false;
    }

    const json::Value* connStr = db->find("connectionString");
    if (!connStr || !connStr->isString() || connStr->asString().empty()) {
        error = "db.connectionString must be a non-empty string";
        return false;
    }

    const json::Value* connCnt = db->find("connectionCount");
    if (!connCnt || !connCnt->isNumber()) {
        error = "db.connectionCount must be an integer from 1 to 64";
        return false;
    }

    const double cnt = connCnt->asNumber();
    if (!std::isfinite(cnt) || std::trunc(cnt) != cnt || cnt < 1.0 || cnt > 64.0) {
        error = "db.connectionCount must be an integer from 1 to 64";
        return false;
    }

    DbConfig parsed;
    if (!utf8ToWide(connStr->asString(), parsed.connectionString, error)) return false;
    parsed.connectionCount = static_cast<std::int32_t>(cnt);

    out = std::move(parsed);
    return true;
}
