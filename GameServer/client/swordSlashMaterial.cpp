#include "pch.hpp"
#include "swordSlashMaterial.hpp"

#include <cctype>
#include <cstdlib>

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

std::size_t skipWhitespace(std::string_view text, std::size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
    return pos;
}

std::size_t findMatchingBrace(std::string_view text, std::size_t openPos) {
    if (openPos >= text.size() || text[openPos] != '{')
        return std::string_view::npos;

    bool inString = false;
    bool escaped = false;
    int depth = 0;

    for (std::size_t i = openPos; i < text.size(); ++i) {
        const char c = text[i];

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0)
                return i;
        }
    }

    return std::string_view::npos;
}

bool findObject(std::string_view scope, std::string_view key, std::string_view& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = scope.find(needle);
    if (keyPos == std::string_view::npos)
        return false;

    const auto colonPos = scope.find(':', keyPos + needle.size());
    if (colonPos == std::string_view::npos)
        return false;

    const auto openPos = skipWhitespace(scope, colonPos + 1);
    if (openPos >= scope.size() || scope[openPos] != '{')
        return false;

    const auto closePos = findMatchingBrace(scope, openPos);
    if (closePos == std::string_view::npos)
        return false;

    out = scope.substr(openPos, closePos - openPos + 1);
    return true;
}

bool readNumber(std::string_view scope, std::string_view key, float& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = scope.find(needle);
    if (keyPos == std::string_view::npos)
        return false;

    const auto colonPos = scope.find(':', keyPos + needle.size());
    if (colonPos == std::string_view::npos)
        return false;

    auto begin = skipWhitespace(scope, colonPos + 1);
    auto end = begin;
    while (end < scope.size()) {
        const char c = scope[end];
        if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
              c == '.' || c == 'e' || c == 'E'))
            break;
        ++end;
    }
    if (begin == end)
        return false;

    const std::string token(scope.substr(begin, end - begin));
    char* parseEnd = nullptr;
    const float value = std::strtof(token.c_str(), &parseEnd);
    if (parseEnd == token.c_str())
        return false;

    out = value;
    return true;
}

float readNumberOr(std::string_view scope, std::string_view key, float fallback) {
    float ret = fallback;
    readNumber(scope, key, ret);
    return ret;
}

mu::Vec2 readXYObjectOr(std::string_view scope, std::string_view key, mu::Vec2 fallback) {
    std::string_view object;
    if (!findObject(scope, key, object))
        return fallback;

    return {
        readNumberOr(object, "x", fallback.x()),
        readNumberOr(object, "y", fallback.y())
    };
}

mu::Vec4 readXYZWObjectOr(std::string_view scope, std::string_view key, mu::Vec4 fallback) {
    std::string_view object;
    if (!findObject(scope, key, object))
        return fallback;

    return {
        readNumberOr(object, "x", fallback.x()),
        readNumberOr(object, "y", fallback.y()),
        readNumberOr(object, "z", fallback.z()),
        readNumberOr(object, "w", fallback.w())
    };
}

mu::Vec4 readRGBAObjectOr(std::string_view scope, std::string_view key, mu::Vec4 fallback) {
    std::string_view object;
    if (!findObject(scope, key, object))
        return fallback;

    return {
        readNumberOr(object, "r", fallback.x()),
        readNumberOr(object, "g", fallback.y()),
        readNumberOr(object, "b", fallback.z()),
        readNumberOr(object, "a", fallback.w())
    };
}

mu::Vec4 readTextureSTOr(std::string_view scope, std::string_view key, mu::Vec4 fallback) {
    std::string_view texture;
    if (!findObject(scope, key, texture))
        return fallback;

    const mu::Vec2 scale = readXYObjectOr(texture, "scale", { fallback.x(), fallback.y() });
    const mu::Vec2 offset = readXYObjectOr(texture, "offset", { fallback.z(), fallback.w() });
    return { scale.x(), scale.y(), offset.x(), offset.y() };
}

}  // namespace

bool loadSwordSlashMaterialMetadata(const std::filesystem::path& path, ps::MatSwordSlash& material) {
    std::string json;
    if (!readTextFile(path, json))
        return false;

    std::string_view root(json);
    std::string_view mappedValues;
    if (!findObject(root, "swordSlashMappedValues", mappedValues))
        return false;

    material.opacity = readNumberOr(mappedValues, "opacity", material.opacity);
    material.emission = readNumberOr(mappedValues, "emission", material.emission);
    material.desaturation = readNumberOr(mappedValues, "desaturation", material.desaturation);
    material.flowPower = readNumberOr(mappedValues, "flowPower", material.flowPower);
    material.useSmoothDissolve =
        readNumberOr(mappedValues, "useSmoothDissolve", material.useSmoothDissolve ? 1.f : 0.f) > 0.5f;

    const mu::Vec4 remap = readXYZWObjectOr(
        mappedValues, "remap",
        { material.remap.x(), material.remap.y(), 0.f, 0.f }
    );
    material.remap = { remap.x(), remap.y() };

    material.addColor = readRGBAObjectOr(mappedValues, "addColor", material.addColor);

    const mu::Vec4 speedMainTexUVNoise = readXYZWObjectOr(
        mappedValues, "speedMainTexUVNoise",
        { material.speedMainTexUV.x(), material.speedMainTexUV.y(),
          material.speedDissolveUV.x(), material.speedDissolveUV.y() }
    );
    material.speedMainTexUV = { speedMainTexUVNoise.x(), speedMainTexUVNoise.y() };
    material.speedDissolveUV = { speedMainTexUVNoise.z(), speedMainTexUVNoise.w() };

    const mu::Vec4 speedFlow = readXYZWObjectOr(
        mappedValues, "speedFlow",
        { material.speedFlow.x(), material.speedFlow.y(), 0.f, 0.f }
    );
    material.speedFlow = { speedFlow.x(), speedFlow.y() };

    material.mainTexST = readTextureSTOr(mappedValues, "mainTexture", material.mainTexST);
    material.emissionTexST = readTextureSTOr(mappedValues, "emissionTex", material.emissionTexST);
    material.dissolveTexST = readTextureSTOr(mappedValues, "dissolveTex", material.dissolveTexST);
    material.flowTexST = readTextureSTOr(mappedValues, "flowTex", material.flowTexST);

    return true;
}
