#ifndef __SHADERPATH_HPP
#define __SHADERPATH_HPP

#include <filesystem>

inline std::filesystem::path getShaderPath() {
    static std::filesystem::path path{};
    if (!path.empty()) {
        return path;
    }

    auto iniFile = std::ifstream("shaderPath.ini");
    if (!iniFile) {
        throw std::runtime_error("Failed to open shaderPath.ini");
    }

    std::string line;
    std::getline(iniFile, line);
    if (!line.starts_with("shaderPath=\"")) {
        throw std::runtime_error("Invalid format in shaderPath.ini");
    }

    line = line.substr(12, line.size() - 13); // Remove "shaderPath=\"" and "\""
    return std::filesystem::path(line);
}

#endif  // __SHADERPATH_HPP
