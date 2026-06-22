#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct NetworkEndpoint {
    std::string   ip{};
    std::uint16_t port = 0;
};

struct NetworkConfig {
    NetworkEndpoint lobby{};
    NetworkEndpoint room{};
};

// Finds the nearest network_config.json by walking upward from the executable
// directory, then parses and validates both endpoints. No fallback values are
// used: false means the process must stop and report error.
bool loadNetworkConfig(NetworkConfig& out, std::filesystem::path& loadedPath,
                       std::string& error);
