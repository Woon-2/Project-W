#include "pch.hpp"
#include "particleImporter.hpp"

#include "simpleJson.hpp"

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.f;

struct SerializedProp {
    std::string type;
    std::string value;
};

using PropMap = std::unordered_map<std::string, SerializedProp>;

bool readTextFile(const std::filesystem::path& path, std::string& out) {
    auto ifs = std::ifstream(path, std::ios::binary);
    if (!ifs)
        return false;

    ifs.seekg(0, std::ios::end);
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if (!out.empty())
        ifs.read(out.data(), static_cast<std::streamsize>(out.size()));

    return ifs.good() || ifs.eof();
}

const json::Value* find(const json::Value* value, std::string_view key) {
    return value ? value->find(key) : nullptr;
}

std::string readString(const json::Value* object, std::string_view key, std::string fallback = {}) {
    const auto* value = find(object, key);
    return value && value->isString() ? value->asString() : std::move(fallback);
}

float readFloat(const json::Value* object, std::string_view key, float fallback = 0.f) {
    const auto* value = find(object, key);
    return value && value->isNumber() ? static_cast<float>(value->asNumber()) : fallback;
}

int readInt(const json::Value* object, std::string_view key, int fallback = 0) {
    return static_cast<int>(std::lround(readFloat(object, key, static_cast<float>(fallback))));
}

bool readBool(const json::Value* object, std::string_view key, bool fallback = false) {
    const auto* value = find(object, key);
    return value && value->isBool() ? value->asBool() : fallback;
}

mu::Vec3 readVec3(const json::Value* object, std::string_view key, mu::Vec3 fallback = {}) {
    const auto* value = find(object, key);
    if (!value || !value->isObject())
        return fallback;
    return {
        readFloat(value, "x", fallback.x()),
        readFloat(value, "y", fallback.y()),
        readFloat(value, "z", fallback.z())
    };
}

mu::Vec4 readColor(const json::Value* object,
                   std::string_view key,
                   mu::Vec4 fallback = { 1.f, 1.f, 1.f, 1.f }) {
    const auto* value = find(object, key);
    if (!value || !value->isObject())
        return fallback;
    return {
        readFloat(value, "r", fallback.x()),
        readFloat(value, "g", fallback.y()),
        readFloat(value, "b", fallback.z()),
        readFloat(value, "a", fallback.w())
    };
}

float propFloat(const PropMap& props, std::string_view path, float fallback = 0.f) {
    const auto it = props.find(std::string(path));
    if (it == props.end())
        return fallback;
    char* end = nullptr;
    const float value = std::strtof(it->second.value.c_str(), &end);
    return end == it->second.value.c_str() ? fallback : value;
}

bool propBool(const PropMap& props, std::string_view path, bool fallback = false) {
    const auto it = props.find(std::string(path));
    if (it == props.end())
        return fallback;
    return it->second.value == "True" || it->second.value == "true" || it->second.value == "1";
}

std::string propValue(const PropMap& props, std::string_view path) {
    const auto it = props.find(std::string(path));
    return it == props.end() ? std::string{} : it->second.value;
}

PropMap buildPropMap(const json::Value* arrayValue) {
    PropMap props;
    if (!arrayValue || !arrayValue->isArray())
        return props;

    for (const auto& prop : arrayValue->asArray()) {
        const std::string path = readString(&prop, "path");
        if (path.empty())
            continue;
        props[path] = SerializedProp{
            .type = readString(&prop, "type"),
            .value = readString(&prop, "value")
        };
    }
    return props;
}

ps::FloatCurve readCurveKeys(const json::Value* curve) {
    ps::FloatCurve out;
    const auto* keys = find(curve, "keys");
    if (!keys || !keys->isArray())
        return out;

    for (const auto& key : keys->asArray()) {
        out.keys.push_back(ps::FloatKey{
            .t = readFloat(&key, "time"),
            .value = readFloat(&key, "value"),
            .inTangent = readFloat(&key, "inTangent"),
            .outTangent = readFloat(&key, "outTangent")
        });
    }
    return out;
}

ps::MinMaxCurveChannel::Mode readCurveMode(std::string_view mode) {
    if (mode == "TwoConstants") return ps::MinMaxCurveChannel::Mode::TwoConstants;
    if (mode == "Curve") return ps::MinMaxCurveChannel::Mode::Curve;
    if (mode == "TwoCurves") return ps::MinMaxCurveChannel::Mode::TwoCurves;
    return ps::MinMaxCurveChannel::Mode::Constant;
}

ps::MinMaxCurveChannel readMinMaxCurve(const json::Value* value) {
    ps::MinMaxCurveChannel out;
    if (!value || !value->isObject())
        return out;

    out.mode = readCurveMode(readString(value, "mode"));
    out.constant = readFloat(value, "constant");
    out.constantMin = readFloat(value, "constantMin");
    out.constantMax = readFloat(value, "constantMax", out.constant);
    out.curveMultiplier = readFloat(value, "curveMultiplier", 1.f);
    out.curve = readCurveKeys(find(value, "curve"));
    out.curveMin = readCurveKeys(find(value, "curveMin"));
    out.curveMax = readCurveKeys(find(value, "curveMax"));
    return out;
}

std::pair<float, float> readCurveRange(const json::Value* value) {
    const auto curve = readMinMaxCurve(value);
    switch (curve.mode) {
    case ps::MinMaxCurveChannel::Mode::TwoConstants:
        return { curve.constantMin, curve.constantMax };
    case ps::MinMaxCurveChannel::Mode::Curve:
        return { curve.curve.evaluate(0.f) * curve.curveMultiplier,
                 curve.curve.evaluate(1.f) * curve.curveMultiplier };
    case ps::MinMaxCurveChannel::Mode::TwoCurves:
        return { curve.curveMin.evaluate(0.f) * curve.curveMultiplier,
                 curve.curveMax.evaluate(1.f) * curve.curveMultiplier };
    case ps::MinMaxCurveChannel::Mode::Constant:
    default:
        return { curve.constant, curve.constant };
    }
}

std::pair<float, float> orderedRange(std::pair<float, float> range) {
    if (range.first > range.second)
        std::swap(range.first, range.second);
    return range;
}

ps::CustomDataChannel::Mode readCustomDataMode(std::string_view mode) {
    if (mode == "TwoConstants") return ps::CustomDataChannel::Mode::TwoConstants;
    if (mode == "Curve") return ps::CustomDataChannel::Mode::Curve;
    if (mode == "TwoCurves") return ps::CustomDataChannel::Mode::TwoCurves;
    return ps::CustomDataChannel::Mode::Constant;
}

ps::CustomDataChannel readCustomDataChannel(const json::Value* value) {
    ps::CustomDataChannel out;
    if (!value || !value->isObject())
        return out;

    out.mode = readCustomDataMode(readString(value, "mode"));
    out.constant = readFloat(value, "constant");
    out.constantMin = readFloat(value, "constantMin");
    out.constantMax = readFloat(value, "constantMax", out.constant);
    out.curveMultiplier = readFloat(value, "curveMultiplier", 1.f);
    out.curve = readCurveKeys(find(value, "curve"));
    out.curveMin = readCurveKeys(find(value, "curveMin"));
    out.curveMax = readCurveKeys(find(value, "curveMax"));
    return out;
}

ColorGradient readGradient(const json::Value* gradientValue) {
    ColorGradient out = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });
    if (!gradientValue || !gradientValue->isObject())
        return out;

    const auto* colorKeys = find(gradientValue, "colorKeys");
    const auto* alphaKeys = find(gradientValue, "alphaKeys");
    if (!colorKeys || !colorKeys->isArray())
        return out;

    out.keys.clear();
    for (const auto& key : colorKeys->asArray()) {
        const float t = readFloat(&key, "time");
        mu::Vec4 color = readColor(&key, "color");

        float bestAlpha = color.w();
        float bestDist = std::numeric_limits<float>::max();
        if (alphaKeys && alphaKeys->isArray()) {
            for (const auto& alphaKey : alphaKeys->asArray()) {
                const float alphaT = readFloat(&alphaKey, "time");
                const float dist = std::abs(alphaT - t);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestAlpha = readFloat(&alphaKey, "alpha", bestAlpha);
                }
            }
        }

        out.keys.push_back({ t, { color.x(), color.y(), color.z(), bestAlpha } });
    }

    if (alphaKeys && alphaKeys->isArray()) {
        for (const auto& alphaKey : alphaKeys->asArray()) {
            const float t = readFloat(&alphaKey, "time");
            const bool alreadyPresent = std::ranges::any_of(out.keys, [&](const auto& key) {
                return std::abs(key.t - t) < 0.0001f;
            });
            if (alreadyPresent)
                continue;

            mu::Vec4 color = out.evaluate(t);
            out.keys.push_back({ t, { color.x(), color.y(), color.z(), readFloat(&alphaKey, "alpha", color.w()) } });
        }
    }

    std::ranges::sort(out.keys, {}, &ColorKey::t);
    return out;
}

ColorGradient readMinMaxGradient(const json::Value* value) {
    if (!value || !value->isObject())
        return ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });

    const std::string mode = readString(value, "mode");
    if (mode == "Gradient" || mode == "TwoGradients")
        return readGradient(find(value, "gradient"));
    return ColorGradient::constant(readColor(value, "color"));
}

mu::Vec4 readStartColor(const json::Value* value) {
    if (!value || !value->isObject())
        return { 1.f, 1.f, 1.f, 1.f };

    const std::string mode = readString(value, "mode");
    if (mode == "Gradient")
        return readGradient(find(value, "gradient")).evaluate(0.f);
    return readColor(value, "color");
}

ps::ShapeModule::Type readShapeType(std::string_view type) {
    if (type == "Edge") return ps::ShapeModule::Type::Edge;
    if (type == "Cone") return ps::ShapeModule::Type::Cone;
    if (type == "Sphere") return ps::ShapeModule::Type::Sphere;
    if (type == "Box") return ps::ShapeModule::Type::Box;
    if (type == "Circle") return ps::ShapeModule::Type::Circle;
    return ps::ShapeModule::Type::Point;
}

ps::RendererModule::Mode readRenderMode(std::string_view mode) {
    if (mode == "Mesh") return ps::RendererModule::Mode::Mesh;
    if (mode == "Stretch" || mode == "StretchedBillboard")
        return ps::RendererModule::Mode::StretchedBillboard;
    return ps::RendererModule::Mode::Billboard;
}

ps::RendererModule::Alignment readAlignment(std::string_view alignment) {
    if (alignment == "World") return ps::RendererModule::Alignment::World;
    if (alignment == "Local") return ps::RendererModule::Alignment::Local;
    if (alignment == "Facing") return ps::RendererModule::Alignment::Facing;
    return ps::RendererModule::Alignment::View;
}

ps::RendererModule::SortMode readSortMode(std::string_view sortMode) {
    if (sortMode == "Distance") return ps::RendererModule::SortMode::Distance;
    if (sortMode == "OldestInFront") return ps::RendererModule::SortMode::OldestInFront;
    if (sortMode == "YoungestInFront") return ps::RendererModule::SortMode::YoungestInFront;
    return ps::RendererModule::SortMode::None;
}

mu::Mat4x4 eulerDegreesToMat(mu::Vec3 degrees) {
    return mu::rotateXH(mu::Degree{ degrees.x() })
         * mu::rotateYH(mu::Degree{ degrees.y() })
         * mu::rotateZH(mu::Degree{ degrees.z() });
}

void readMainModule(const json::Value* main, ps::ParticleSystemConfig& cfg) {
    if (!main)
        return;

    auto [lifetimeMin, lifetimeMax] = orderedRange(readCurveRange(find(main, "startLifetime")));
    auto [speedMin, speedMax] = orderedRange(readCurveRange(find(main, "startSpeed")));
    auto [sizeMin, sizeMax] = orderedRange(readCurveRange(find(main, "startSize")));
    auto [gravityMin, gravityMax] = orderedRange(readCurveRange(find(main, "gravityModifier")));
    auto [rotationMin, rotationMax] = orderedRange(readCurveRange(find(main, "startRotation")));

    cfg.main.lifetimeMin = lifetimeMin;
    cfg.main.lifetimeMax = lifetimeMax;
    cfg.main.speedMin = speedMin;
    cfg.main.speedMax = speedMax;
    cfg.main.startSizeMin = sizeMin;
    cfg.main.startSizeMax = sizeMax;
    cfg.main.gravityModifierMin = gravityMin;
    cfg.main.gravityModifierMax = gravityMax;
    cfg.main.startRotationMin = rotationMin;
    cfg.main.startRotationMax = rotationMax;
    cfg.main.startColor = readStartColor(find(main, "startColor"));
    cfg.main.duration = readFloat(main, "duration", cfg.main.duration);
    cfg.main.looping = readBool(main, "loop", cfg.main.looping);
    cfg.main.prewarm = readBool(main, "prewarm", cfg.main.prewarm);
    cfg.main.playOnAwake = readBool(main, "playOnAwake", cfg.main.playOnAwake);
    cfg.main.startDelay = readCurveRange(find(main, "startDelay")).first;
    cfg.main.simulationSpeed = readFloat(main, "simulationSpeed", cfg.main.simulationSpeed);
    cfg.main.maxParticles = readInt(main, "maxParticles", cfg.main.maxParticles);

    cfg.main.startRotation3DEnabled = readBool(main, "startRotation3D", false);
    if (cfg.main.startRotation3DEnabled) {
        auto [xMin, xMax] = orderedRange(readCurveRange(find(main, "startRotationX")));
        auto [yMin, yMax] = orderedRange(readCurveRange(find(main, "startRotationY")));
        auto [zMin, zMax] = orderedRange(readCurveRange(find(main, "startRotationZ")));
        cfg.main.startRotation3DMin = { xMin, yMin, zMin };
        cfg.main.startRotation3DMax = { xMax, yMax, zMax };
    }

    const std::string simulationSpace = readString(main, "simulationSpace");
    cfg.main.simulationSpace = simulationSpace == "Local"
                             ? ps::MainModule::SimulationSpace::Local
                             : ps::MainModule::SimulationSpace::World;

    const std::string scalingMode = readString(main, "scalingMode");
    if (scalingMode == "Local")
        cfg.main.scalingMode = ps::MainModule::ScalingMode::Local;
    else if (scalingMode == "Shape")
        cfg.main.scalingMode = ps::MainModule::ScalingMode::Shape;
    else
        cfg.main.scalingMode = ps::MainModule::ScalingMode::Hierarchy;
}

bool isRootSystemPath(std::string_view relativePath) {
    return relativePath.find('/') == std::string_view::npos
        && relativePath.find('\\') == std::string_view::npos;
}

void readTransform(const json::Value* transform,
                   std::string_view relativePath,
                   ps::ParticleSystemConfig& cfg) {
    if (!transform)
        return;

    if (!isRootSystemPath(relativePath))
        cfg.shape.position += readVec3(transform, "localPosition");

    cfg.main.startRotation3D = eulerDegreesToMat(readVec3(transform, "localEulerAngles"));
    cfg.main.transformScale = readVec3(transform, "localScale", cfg.main.transformScale);
    cfg.shape.orientation = cfg.main.startRotation3D;
    cfg.shape.scale *= cfg.main.transformScale;
}

void readEmissionModule(const json::Value* emission, ps::ParticleSystemConfig& cfg) {
    if (!emission)
        return;

    cfg.emission.enabled = readBool(emission, "enabled", cfg.emission.enabled);
    cfg.emission.emitRate = readCurveRange(find(emission, "rateOverTime")).second;
    cfg.emission.rateOverDistance = readCurveRange(find(emission, "rateOverDistance")).second;

    cfg.emission.bursts.clear();
    const auto* bursts = find(emission, "bursts");
    if (!bursts || !bursts->isArray())
        return;

    for (const auto& burst : bursts->asArray()) {
        auto [countMin, countMax] = orderedRange(readCurveRange(find(&burst, "count")));
        cfg.emission.bursts.push_back(ps::EmissionModule::Burst{
            .time = readFloat(&burst, "time"),
            .countMin = static_cast<int>(std::lround(countMin)),
            .countMax = static_cast<int>(std::lround(countMax)),
            .cycleCount = readInt(&burst, "cycleCount", 1),
            .repeatInterval = readFloat(&burst, "repeatInterval", 0.01f),
            .probability = readFloat(&burst, "probability", 1.f)
        });
    }
}

void readShapeModule(const json::Value* shape,
                     ps::ParticleSystemConfig& cfg) {
    if (!shape)
        return;

    cfg.shape.enabled = readBool(shape, "enabled", cfg.shape.enabled);
    cfg.shape.type = readShapeType(readString(shape, "shapeType"));
    cfg.shape.position = readVec3(shape, "position", cfg.shape.position);

    cfg.shape.rotation = readVec3(shape, "rotation") * kDegToRad;
    cfg.shape.scale = readVec3(shape, "scale", cfg.shape.scale);
    cfg.shape.coneAngle = readFloat(shape, "angle", 25.f) * kDegToRad;
    cfg.shape.coneRadius = readFloat(shape, "radius", cfg.shape.coneRadius);
    cfg.shape.sphereRadius = readFloat(shape, "radius", cfg.shape.sphereRadius);
    cfg.shape.radiusThickness = readFloat(shape, "radiusThickness", cfg.shape.radiusThickness);
    cfg.shape.arc = readFloat(shape, "arc", 360.f) * kDegToRad;
    cfg.shape.alignToDirection = readBool(shape, "alignToDirection", cfg.shape.alignToDirection);
    cfg.shape.randomDirectionAmount = readFloat(shape, "randomDirectionAmount", cfg.shape.randomDirectionAmount);
    cfg.shape.sphericalDirectionAmount = readFloat(shape, "sphericalDirectionAmount", cfg.shape.sphericalDirectionAmount);
    cfg.shape.randomPositionAmount = readFloat(shape, "randomPositionAmount", cfg.shape.randomPositionAmount);

    if (cfg.shape.type == ps::ShapeModule::Type::Box)
        cfg.shape.boxSize = cfg.shape.scale;

}

void readColorModule(const json::Value* color, ps::ParticleSystemConfig& cfg) {
    if (!color)
        return;

    cfg.colorOverLifetime.enabled = readBool(color, "enabled", cfg.colorOverLifetime.enabled);
    cfg.colorOverLifetime.gradient = readMinMaxGradient(find(color, "color"));
}

void readSizeModule(const json::Value* size, ps::ParticleSystemConfig& cfg) {
    if (!size)
        return;

    cfg.sizeOverLifetime.enabled = readBool(size, "enabled", cfg.sizeOverLifetime.enabled);
    const auto range = readCurveRange(find(size, "size"));
    cfg.sizeOverLifetime.sizeBegin = range.first;
    cfg.sizeOverLifetime.sizeEnd = range.second;
    cfg.sizeOverLifetime.curve = readMinMaxCurve(find(size, "size")).curve;
}

void readRotationModule(const json::Value* rotation, ps::ParticleSystemConfig& cfg) {
    if (!rotation)
        return;

    cfg.rotationOverLifetime.enabled = readBool(rotation, "enabled", cfg.rotationOverLifetime.enabled);
    cfg.rotationOverLifetime.separateAxes = readBool(rotation, "separateAxes", cfg.rotationOverLifetime.separateAxes);
    cfg.rotationOverLifetime.useCurves = cfg.rotationOverLifetime.enabled;
    cfg.rotationOverLifetime.x = readMinMaxCurve(find(rotation, "x"));
    cfg.rotationOverLifetime.y = readMinMaxCurve(find(rotation, "y"));
    cfg.rotationOverLifetime.z = readMinMaxCurve(find(rotation, "z"));

    if (!cfg.rotationOverLifetime.separateAxes) {
        const auto range = readCurveRange(find(rotation, "z"));
        cfg.rotationOverLifetime.angularVelocityMin = range.first;
        cfg.rotationOverLifetime.angularVelocityMax = range.second;
    }
}

void readCustomDataStream(const json::Value* stream, ps::CustomDataStream& out) {
    if (!stream || readString(stream, "mode") != "Vector")
        return;

    const auto* components = find(stream, "vectorComponents");
    if (!components || !components->isArray())
        return;

    const auto& array = components->asArray();
    if (!array.empty())
        out.x = readCustomDataChannel(&array[0]);
    if (array.size() > 1)
        out.y = readCustomDataChannel(&array[1]);
}

void readCustomDataModule(const json::Value* customData, ps::ParticleSystemConfig& cfg) {
    if (!customData)
        return;

    cfg.customData.enabled = readBool(customData, "enabled", cfg.customData.enabled);
    readCustomDataStream(find(customData, "custom1"), cfg.customData.custom1);
    readCustomDataStream(find(customData, "custom2"), cfg.customData.custom2);
}

void readVelocityModule(const PropMap& props, ps::ParticleSystemConfig& cfg) {
    cfg.velocityOverLifetime.enabled = propBool(props, "VelocityModule.enabled", cfg.velocityOverLifetime.enabled);
    if (!cfg.velocityOverLifetime.enabled)
        return;

    cfg.velocityOverLifetime.linear = {
        propFloat(props, "VelocityModule.x.scalar"),
        propFloat(props, "VelocityModule.y.scalar"),
        propFloat(props, "VelocityModule.z.scalar")
    };
    cfg.velocityOverLifetime.orbital = {
        propFloat(props, "VelocityModule.orbitalX.scalar"),
        propFloat(props, "VelocityModule.orbitalY.scalar"),
        propFloat(props, "VelocityModule.orbitalZ.scalar")
    };
    cfg.velocityOverLifetime.radial = propFloat(props, "VelocityModule.radial.scalar");
    cfg.velocityOverLifetime.speedModifier = propFloat(props, "VelocityModule.speedModifier.scalar", 1.f);
    cfg.velocityOverLifetime.inWorldSpace = propBool(props, "VelocityModule.inWorldSpace");
}

void readTextureSheetModule(const PropMap& props, ps::ParticleSystemConfig& cfg) {
    cfg.textureSheetAnimation.enabled = propBool(
        props,
        "UVModule.enabled",
        cfg.textureSheetAnimation.enabled
    );
    if (!cfg.textureSheetAnimation.enabled)
        return;

    cfg.textureSheetAnimation.tilesX = std::max(1, static_cast<int>(std::lround(
        propFloat(props, "UVModule.tilesX", static_cast<float>(cfg.textureSheetAnimation.tilesX))
    )));
    cfg.textureSheetAnimation.tilesY = std::max(1, static_cast<int>(std::lround(
        propFloat(props, "UVModule.tilesY", static_cast<float>(cfg.textureSheetAnimation.tilesY))
    )));
    cfg.textureSheetAnimation.startFrame = static_cast<int>(std::lround(
        propFloat(props, "UVModule.startFrame.scalar", static_cast<float>(cfg.textureSheetAnimation.startFrame))
    ));
    cfg.textureSheetAnimation.cycles = propFloat(
        props,
        "UVModule.cycles",
        cfg.textureSheetAnimation.cycles
    );

    const int animationType = static_cast<int>(std::lround(propFloat(props, "UVModule.animationType", 0.f)));
    cfg.textureSheetAnimation.animation = animationType == 1
                                        ? ps::TextureSheetAnimationModule::Animation::SingleRow
                                        : ps::TextureSheetAnimationModule::Animation::WholeSheet;
}

void readRendererModule(const json::Value* renderer,
                        const PropMap& rendererProps,
                        ps::ParticleSystemConfig& cfg) {
    if (!renderer)
        return;

    cfg.renderer.mode = readRenderMode(readString(renderer, "renderMode"));
    cfg.renderer.alignment = readAlignment(readString(renderer, "alignment"));
    cfg.renderer.sortMode = readSortMode(readString(renderer, "sortMode"));
    cfg.renderer.minParticleSize = readFloat(renderer, "minParticleSize", cfg.renderer.minParticleSize);
    cfg.renderer.maxParticleSize = readFloat(renderer, "maxParticleSize", cfg.renderer.maxParticleSize);
    cfg.renderer.normalDirection = readFloat(renderer, "normalDirection", cfg.renderer.normalDirection);
    cfg.renderer.allowRoll = readBool(renderer, "allowRoll", cfg.renderer.allowRoll);
    cfg.renderer.pivot = readVec3(renderer, "pivot", cfg.renderer.pivot);
    cfg.renderer.flip = readVec3(renderer, "flip", cfg.renderer.flip);
    cfg.renderer.cameraVelocityScale = propFloat(rendererProps, "m_CameraVelocityScale", cfg.renderer.cameraVelocityScale);
    cfg.renderer.velocityScale = propFloat(rendererProps, "m_VelocityScale", cfg.renderer.velocityScale);
    cfg.renderer.lengthScale = propFloat(rendererProps, "m_LengthScale", cfg.renderer.lengthScale);
    cfg.renderer.sortingFudge = propFloat(rendererProps, "m_SortingFudge", cfg.renderer.sortingFudge);

}

void logUnhandledEnabledModules(const std::filesystem::path& path,
                                std::string_view relativePath,
                                const PropMap& props,
                                bool logDiagnostics) {
    if (!logDiagnostics)
        return;

    static const std::unordered_set<std::string> supported = {
        "InitialModule",
        "ShapeModule",
        "EmissionModule",
        "ColorModule",
        "SizeModule",
        "RotationModule",
        "VelocityModule",
        "CustomDataModule",
        "UVModule"
    };

    for (const auto& [propPath, prop] : props) {
        if (!propBool(props, propPath))
            continue;
        if (!propPath.ends_with(".enabled"))
            continue;

        const auto dot = propPath.find('.');
        const std::string module = dot == std::string::npos ? propPath : propPath.substr(0, dot);
        if (supported.contains(module))
            continue;

        gSharedLog << "[Particle Import] Warning: " << path.string()
                   << " / " << relativePath
                   << " has unsupported active module: " << module
                   << " (" << propPath << ")\n";
    }
}

bool parseSystem(const std::filesystem::path& path,
                 const json::Value& system,
                 const ParticleImportOptions& options,
                 ParticleImportSystem& out) {
    out.name = readString(&system, "name");
    out.relativePath = readString(&system, "relativePath", out.name);

    if (!options.includeInactive && !readBool(&system, "activeInHierarchy", true))
        return false;

    const auto* summary = find(&system, "summary");
    if (!summary)
        return false;

    const PropMap psProps = buildPropMap(find(&system, "particleSystemSerializedProperties"));
    const PropMap rendererProps = buildPropMap(find(&system, "rendererSerializedProperties"));

    readMainModule(find(summary, "main"), out.config);
    readEmissionModule(find(summary, "emission"), out.config);
    readShapeModule(find(summary, "shape"), out.config);
    readTransform(find(&system, "transform"), out.relativePath, out.config);
    readVelocityModule(psProps, out.config);
    readTextureSheetModule(psProps, out.config);
    readColorModule(find(summary, "colorOverLifetime"), out.config);
    readSizeModule(find(summary, "sizeOverLifetime"), out.config);
    readRotationModule(find(summary, "rotationOverLifetime"), out.config);
    readCustomDataModule(find(summary, "customData"), out.config);
    readRendererModule(find(summary, "renderer"), rendererProps, out.config);

    logUnhandledEnabledModules(path, out.relativePath, psProps, options.logDiagnostics);
    return true;
}

}  // namespace

bool loadParticleSystemsFromUnityJson(const std::filesystem::path& path,
                                      std::vector<ParticleImportSystem>& outSystems,
                                      const ParticleImportOptions& options) {
    std::string text;
    if (!readTextFile(path, text)) {
        if (options.logDiagnostics)
            gSharedLog << "[Particle Import] Warning: failed to read " << path.string() << "\n";
        return false;
    }

    json::Value root;
    std::string error;
    if (!json::parse(text, root, &error)) {
        if (options.logDiagnostics)
            gSharedLog << "[Particle Import] Warning: failed to parse " << path.string()
                       << ": " << error << "\n";
        return false;
    }

    const auto* systems = root.find("systems");
    if (!systems || !systems->isArray()) {
        if (options.logDiagnostics)
            gSharedLog << "[Particle Import] Warning: " << path.string()
                       << " has no systems array.\n";
        return false;
    }

    const std::size_t oldSize = outSystems.size();
    for (const auto& system : systems->asArray()) {
        ParticleImportSystem imported;
        if (parseSystem(path, system, options, imported))
            outSystems.push_back(std::move(imported));
    }

    if (options.logDiagnostics) {
        gSharedLog << "[Particle Import] File I/O: " << path.string()
                   << " imported " << (outSystems.size() - oldSize)
                   << " particle system(s).\n";
    }

    return outSystems.size() > oldSize;
}

bool loadParticleSystemFromUnityJson(const std::filesystem::path& path,
                                     std::string_view relativePath,
                                     ParticleImportSystem& outSystem,
                                     const ParticleImportOptions& options) {
    std::vector<ParticleImportSystem> systems;
    if (!loadParticleSystemsFromUnityJson(path, systems, options))
        return false;

    for (auto& system : systems) {
        if (system.relativePath == relativePath) {
            outSystem = std::move(system);
            return true;
        }
    }

    if (options.logDiagnostics) {
        gSharedLog << "[Particle Import] Warning: " << path.string()
                   << " has no ParticleSystem at relativePath \"" << relativePath << "\".\n";
    }
    return false;
}

bool loadParticleSystemConfigFromUnityJson(const std::filesystem::path& path,
                                           std::string_view relativePath,
                                           ps::ParticleSystemConfig& outConfig,
                                           const ParticleImportOptions& options) {
    ParticleImportSystem system;
    if (!loadParticleSystemFromUnityJson(path, relativePath, system, options))
        return false;

    outConfig = std::move(system.config);
    return true;
}
