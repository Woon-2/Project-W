#ifndef __piercingMaterial_HPP
#define __piercingMaterial_HPP

#include <filesystem>
#include "particleModules.hpp"

// Loads the numeric/color/vector parameters of a Vefects Piercing material
// (exported by MaterialMetadataExporter) from its "shaderProperties" array.
// Texture pointers are left untouched — bind them separately from AssetManager.
bool loadPiercingMaterialMetadata(const std::filesystem::path& path, ps::MatPiercing& material);

#endif  // __piercingMaterial_HPP
