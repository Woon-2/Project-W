#pragma once

#include "particleModules.hpp"

#include <filesystem>

bool loadSwordSlashMaterialMetadata(const std::filesystem::path& path, ps::MatSwordSlash& material);
