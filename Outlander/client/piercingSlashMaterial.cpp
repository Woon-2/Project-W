#include "pch.hpp"
#include "piercingSlashMaterial.hpp"

#include <fstream>
#include <unordered_map>
#include "simpleJson.hpp"

namespace {

bool readTextFile(const std::filesystem::path& path, std::string& out) {
    auto ifs = std::ifstream(path, std::ios::binary);
    if (!ifs) return false;

    ifs.seekg(0, std::ios::end);
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if (!out.empty())
        ifs.read(out.data(), static_cast<std::streamsize>(out.size()));

    return ifs.good() || ifs.eof();
}

float readNum(const json::Value* obj, std::string_view key, float fallback) {
    const auto* v = obj ? obj->find(key) : nullptr;
    return v && v->isNumber() ? static_cast<float>(v->asNumber()) : fallback;
}

mu::Vec2 readVec2(const json::Value* prop, mu::Vec2 fallback) {
    const auto* vec = prop ? prop->find("vector") : nullptr;
    if (!vec || !vec->isObject()) return fallback;
    return { readNum(vec, "x", fallback.x()), readNum(vec, "y", fallback.y()) };
}

mu::Vec4 readColor(const json::Value* prop, mu::Vec4 fallback) {
    const auto* col = prop ? prop->find("color") : nullptr;
    if (!col || !col->isObject()) return fallback;
    return {
        readNum(col, "r", fallback.x()),
        readNum(col, "g", fallback.y()),
        readNum(col, "b", fallback.z()),
        readNum(col, "a", fallback.w()),
    };
}

mu::Vec4 readTexST(const json::Value* prop, mu::Vec4 fallback) {
    const auto* tex = prop ? prop->find("texture") : nullptr;
    if (!tex || !tex->isObject()) return fallback;
    const auto* scale = tex->find("scale");
    const auto* offset = tex->find("offset");
    const float sx = scale ? readNum(scale, "x", fallback.x()) : fallback.x();
    const float sy = scale ? readNum(scale, "y", fallback.y()) : fallback.y();
    const float ox = offset ? readNum(offset, "x", fallback.z()) : fallback.z();
    const float oy = offset ? readNum(offset, "y", fallback.w()) : fallback.w();
    return { sx, sy, ox, oy };
}

}  // namespace

bool loadPiercingSlashMaterialMetadata(const std::filesystem::path& path, ps::MatPiercingSlash& material) {
    std::string text;
    if (!readTextFile(path, text)) return false;

    json::Value root;
    if (!json::parse(text, root, nullptr)) return false;

    const auto* props = root.find("shaderProperties");
    if (!props || !props->isArray()) return false;

    std::unordered_map<std::string, const json::Value*> byName;
    for (const auto& prop : props->asArray()) {
        const auto* name = prop.find("name");
        if (name && name->isString())
            byName[name->asString()] = &prop;
    }

    const auto prop = [&](std::string_view key) -> const json::Value* {
        const auto it = byName.find(std::string(key));
        return it == byName.end() ? nullptr : it->second;
    };
    const auto propFloat = [&](std::string_view key, float fallback) {
        return readNum(prop(key), "floatValue", fallback);
    };

    material.color1        = readColor(prop("_Color_1"), material.color1);
    material.color2        = readColor(prop("_Color_2"), material.color2);
    material.emissiveColor = readColor(prop("_Emissive_Color"), material.emissiveColor);

    material.colorNoiseScale       = readVec2(prop("_ColorNoise_Scale"), material.colorNoiseScale);
    material.colorNoiseSpeed       = readVec2(prop("_ColorNoise_Speed"), material.colorNoiseSpeed);
    material.slashNoiseScale       = readVec2(prop("_Slash_Noise_Scale"), material.slashNoiseScale);
    material.slashNoiseSpeed       = readVec2(prop("_Slash_Noise_Speed"), material.slashNoiseSpeed);
    material.emissiveDissolveScale = readVec2(prop("_Emissive_Dissolve_Scale"), material.emissiveDissolveScale);
    material.emissiveDissolveSpeed = readVec2(prop("_Emissive_Dissolve_Speed"), material.emissiveDissolveSpeed);
    material.distortionNoiseScale  = readVec2(prop("_Distortion_Noise_Scale"), material.distortionNoiseScale);
    material.distortionNoiseSpeed  = readVec2(prop("_Distortion_Noise_Speed"), material.distortionNoiseSpeed);

    material.maskST       = readTexST(prop("_Mask"), material.maskST);
    material.cutoutOffset = readVec2(prop("_CutoutOffset"), material.cutoutOffset);

    material.slashScale              = propFloat("_Slash_Scale", material.slashScale);
    material.slashSpeed              = propFloat("_Slash_Speed", material.slashSpeed);
    material.emissiveSlashScale      = propFloat("_Emissive_Slash_Scale", material.emissiveSlashScale);
    material.emissiveSlashSpeed      = propFloat("_Emissive_Slash_Speed", material.emissiveSlashSpeed);
    material.slashNoiseIntensity     = propFloat("_Slash_Noise_Intensity", material.slashNoiseIntensity);
    material.distortionIntensity     = propFloat("_Distortion_Intensity", material.distortionIntensity);
    material.colorBoost              = propFloat("_Color_Boost", material.colorBoost);
    material.emissiveIntensity       = propFloat("_Emissive_Intensity", material.emissiveIntensity);
    material.opacityBoost            = propFloat("_Opacity_Boost", material.opacityBoost);
    material.additiveLerp            = propFloat("_AdditiveLerp", material.additiveLerp);
    material.cutoutErosion           = propFloat("_CutoutErosion", material.cutoutErosion);
    material.cutoutErosionSmoothness = propFloat("_CutoutErosionSmoothness", material.cutoutErosionSmoothness);
    material.cutoutRotation          = propFloat("_CutoutRotation", material.cutoutRotation);

    return true;
}
