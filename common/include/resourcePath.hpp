#ifndef __RESOURCEPATH_HPP
#define __RESOURCEPATH_HPP

#include <filesystem>
#include <fstream>

inline std::filesystem::path getResourcePath() {
    static std::filesystem::path path{};
    if (!path.empty()) {
        return path;
    }

    auto iniFile = std::ifstream("resourcePath.ini");
    if (!iniFile) {
        throw std::runtime_error("Failed to open resourcePath.ini");
    }

    std::string line;
    std::getline(iniFile, line);
    if (!line.starts_with("resourcePath=\"")) {
        throw std::runtime_error("Invalid format in resourcePath.ini");
    }

    line = line.substr(14, line.size() - 15); // Remove "resourcePath=\"" and "\""
    return std::filesystem::path(line);
}

#endif  // __RESOURCEPATH_HPP
