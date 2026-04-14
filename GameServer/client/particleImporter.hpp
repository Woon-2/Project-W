#pragma once

#include "particleSystem.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct ParticleImportSystem {
    std::string name;
    std::string relativePath;
    ps::ParticleSystemConfig config;
};

struct ParticleImportOptions {
    bool includeInactive = false;
    bool logDiagnostics = true;
};

bool loadParticleSystemsFromUnityJson(const std::filesystem::path& path,
                                      std::vector<ParticleImportSystem>& outSystems,
                                      const ParticleImportOptions& options = {});

bool loadParticleSystemFromUnityJson(const std::filesystem::path& path,
                                     std::string_view relativePath,
                                     ParticleImportSystem& outSystem,
                                     const ParticleImportOptions& options = {});

bool loadParticleSystemConfigFromUnityJson(const std::filesystem::path& path,
                                           std::string_view relativePath,
                                           ps::ParticleSystemConfig& outConfig,
                                           const ParticleImportOptions& options = {});
