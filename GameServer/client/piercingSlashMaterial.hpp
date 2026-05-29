#ifndef __piercingSlashMaterial_HPP
#define __piercingSlashMaterial_HPP

#include <filesystem>
#include "particleModules.hpp"

// Loads the numeric/color/vector parameters of a Vefects Slash material
// (M_VFX_Slash_Fire-style, SH_VFX_Vefects_Slash_BIRP_New shader) from its
// "shaderProperties" array. Texture pointers are left untouched — bind them
// separately from AssetManager.
bool loadPiercingSlashMaterialMetadata(const std::filesystem::path& path, ps::MatPiercingSlash& material);

#endif  // __piercingSlashMaterial_HPP
