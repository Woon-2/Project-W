#include "pch.hpp"
#include "TacticalZoneIntro.hpp"
#include "../../AssetManager.hpp"

namespace UI {

void TacticalZoneIntro::init(UIManager& uiManager, AssetManager& assetManager) {
    uiManager_ = &uiManager;
    assetManager_ = &assetManager;
    buildUI();
}

void TacticalZoneIntro::buildUI() {
    if (root_ || !uiManager_ || !assetManager_) return;

    root_ = static_cast<Panel*>(
        uiManager_->root()->addChild(std::make_unique<Panel>())
    );
    root_->name = "tacticalZoneIntro";
    root_->anchor = Anchors::Center;
    root_->pivot = Pivots::Center;
    root_->width = DimValue::pct(100.f);
    root_->height = DimValue::pct(100.f);
    root_->zOrder = 900;
    root_->visible = false;

    scrim_ = static_cast<Panel*>(
        root_->addChild(std::make_unique<Panel>())
    );
    scrim_->name = "tacticalZoneIntroScrim";
    scrim_->anchor = Anchors::Center;
    scrim_->pivot = Pivots::Center;
    scrim_->width = DimValue::pct(140.f);
    scrim_->height = DimValue::px(270.f);
    scrim_->drawSolidBackground = true;
    scrim_->colorTint = { 0.01f, 0.012f, 0.014f, 0.f };

    banner_ = static_cast<Image*>(
        root_->addChild(std::make_unique<Image>())
    );
    banner_->name = "tacticalZoneIntroBanner";
    banner_->anchor = Anchors::Center;
    banner_->pivot = Pivots::Center;
    banner_->width = DimValue::px(980.f);
    banner_->height = DimValue::px(327.f);
    banner_->texture = assetManager_->tacticalZoneTitleBanner();
    banner_->colorMul = { 0.035f, 0.04f, 0.045f, 0.f };
    banner_->zOrder = 1;

    emblem_ = static_cast<Image*>(
        root_->addChild(std::make_unique<Image>())
    );
    emblem_->name = "tacticalZoneIntroEmblem";
    emblem_->anchor = Anchors::Center;
    emblem_->pivot = Pivots::Center;
    emblem_->offsetX = DimValue::px(-285.f);
    emblem_->width = DimValue::px(248.f);
    emblem_->height = DimValue::px(248.f);
    emblem_->texture = assetManager_->hobgoblinEmblem();
    emblem_->colorMul = { 1.f, 0.78f, 0.64f, 0.f };
    emblem_->zOrder = 2;

    auto makeAccentLine = [&](const char* name, float offsetX) {
        auto* line = static_cast<Panel*>(
            root_->addChild(std::make_unique<Panel>())
        );
        line->name = name;
        line->anchor = Anchors::Center;
        line->pivot = Pivots::Center;
        line->offsetX = DimValue::px(offsetX);
        line->offsetY = DimValue::px(-70.f);
        line->width = DimValue::px(92.f);
        line->height = DimValue::px(2.f);
        line->drawSolidBackground = true;
        line->colorTint = { 0.85f, 0.20f, 0.12f, 0.f };
        line->zOrder = 3;
        return line;
    };
    lineLeft_ = makeAccentLine("tacticalZoneIntroLineLeft", -141.f);
    lineRight_ = makeAccentLine("tacticalZoneIntroLineRight", 281.f);

    category_ = static_cast<Label*>(
        root_->addChild(std::make_unique<Label>())
    );
    category_->name = "tacticalZoneIntroCategory";
    category_->anchor = Anchors::Center;
    category_->pivot = Pivots::Center;
    category_->offsetX = DimValue::px(70.f);
    category_->offsetY = DimValue::px(-70.f);
    category_->width = DimValue::px(300.f);
    category_->height = DimValue::px(44.f);
    category_->setText(L"전술 전투 구역 1");
    category_->setFontFamily(L"Malgun Gothic");
    category_->setFontSize(22.f);
    category_->setTextHAlign(TextHAlign::Center);
    category_->setTextVAlign(TextVAlign::Center);
    category_->setTextColor(0.72f, 0.70f, 0.66f, 1.f);
    category_->colorTint.a = 0.f;
    category_->zOrder = 4;

    title_ = static_cast<Label*>(
        root_->addChild(std::make_unique<Label>())
    );
    title_->name = "tacticalZoneIntroTitle";
    title_->anchor = Anchors::Center;
    title_->pivot = Pivots::Center;
    title_->offsetX = DimValue::px(30.f);
    title_->offsetY = DimValue::px(30.f);
    title_->width = DimValue::px(440.f);
    title_->height = DimValue::px(96.f);
    title_->setText(L"홉 고블린");
    title_->setFontFamily(L"Malgun Gothic");
    title_->setFontSize(56.f);
    title_->setTextHAlign(TextHAlign::Center);
    title_->setTextVAlign(TextVAlign::Center);
    title_->setTextColor(0.93f, 0.90f, 0.84f, 1.f);
    title_->colorTint.a = 0.f;
    title_->zOrder = 5;

    warningScrim_ = static_cast<Panel*>(
        root_->addChild(std::make_unique<Panel>())
    );
    warningScrim_->name = "tacticalZoneWarningScrim";
    warningScrim_->anchor = Anchors::Center;
    warningScrim_->pivot = Pivots::Center;
    warningScrim_->width = DimValue::pct(140.f);
    warningScrim_->height = DimValue::pct(100.f);
    warningScrim_->drawSolidBackground = true;
    warningScrim_->colorTint = { 0.f, 0.f, 0.f, 0.f };
    warningScrim_->zOrder = 20;
    warningScrim_->visible = false;

    warningNoise_ = static_cast<Image*>(
        root_->addChild(std::make_unique<Image>())
    );
    warningNoise_->name = "tacticalZoneWarningNoise";
    warningNoise_->anchor = Anchors::Center;
    warningNoise_->pivot = Pivots::Center;
    warningNoise_->width = DimValue::pct(105.f);
    warningNoise_->height = DimValue::pct(100.f);
    warningNoise_->texture = assetManager_->warningNoiseMask();
    warningNoise_->colorMul = { 1.f, 1.f, 1.f, 0.f };
    warningNoise_->zOrder = 21;
    warningNoise_->visible = false;

    auto makeWarningGlitch = [&](const char* name, const XMFLOAT4& tint, int zOrder) {
        auto* image = static_cast<Image*>(
            root_->addChild(std::make_unique<Image>())
        );
        image->name = name;
        image->anchor = Anchors::Center;
        image->pivot = Pivots::Center;
        image->width = DimValue::px(1024.f);
        image->height = DimValue::px(440.f);
        image->texture = assetManager_->warningGlitchFragments();
        image->colorMul = tint;
        image->zOrder = zOrder;
        image->visible = false;
        return image;
    };
    warningGlitchRed_ =
        makeWarningGlitch("tacticalZoneWarningGlitchRed", { 1.f, 0.08f, 0.04f, 0.f }, 22);
    warningGlitchCyan_ =
        makeWarningGlitch("tacticalZoneWarningGlitchCyan", { 0.08f, 0.9f, 0.95f, 0.f }, 23);

    auto makeWarningLabel = [&](const char* name, const Color& color, int zOrder) {
        auto* label = static_cast<Label*>(
            root_->addChild(std::make_unique<Label>())
        );
        label->name = name;
        label->anchor = Anchors::Center;
        label->pivot = Pivots::Center;
        label->width = DimValue::px(1000.f);
        label->height = DimValue::px(190.f);
        label->setText(L"WARNING");
        label->setFontFamily(L"Arial Black");
        label->setFontSize(112.f);
        label->setTextHAlign(TextHAlign::Center);
        label->setTextVAlign(TextVAlign::Center);
        label->setTextColor(color.r, color.g, color.b, 1.f);
        label->colorTint.a = 0.f;
        label->zOrder = zOrder;
        label->visible = false;
        return label;
    };
    warningTextRed_ =
        makeWarningLabel("tacticalZoneWarningTextRed", { 0.94f, 0.08f, 0.05f, 1.f }, 24);
    warningTextCyan_ =
        makeWarningLabel("tacticalZoneWarningTextCyan", { 0.12f, 0.88f, 0.9f, 1.f }, 25);
    warningText_ =
        makeWarningLabel("tacticalZoneWarningText", { 1.f, 0.24f, 0.18f, 1.f }, 26);
}

bool TacticalZoneIntro::trigger(std::string_view arenaPrefix) {
    if (!root_) buildUI();
    if (!root_) return false;

    const Texture* emblem = nullptr;
    const wchar_t* category = nullptr;
    const wchar_t* title = nullptr;
    XMFLOAT4 emblemColor{};
    Color accentColor{};

    if (arenaPrefix == "WallHobgoblin") {
        emblem = assetManager_->hobgoblinEmblem();
        category = L"전술 전투 구역 1";
        title = L"홉 고블린";
        emblemColor = { 1.f, 0.78f, 0.64f, 0.f };
        accentColor = { 0.85f, 0.20f, 0.12f, 0.f };
    }
    else if (arenaPrefix == "WallGrandbaum") {
        emblem = assetManager_->grandbaumEmblem();
        category = L"전술 전투 구역 2";
        title = L"그랜드밤";
        emblemColor = { 1.f, 0.76f, 0.38f, 0.f };
        accentColor = { 0.88f, 0.52f, 0.16f, 0.f };
    }
    else if (arenaPrefix == "WallIsys") {
        emblem = assetManager_->isysEmblem();
        category = L"전술 전투 구역 3";
        title = L"이시스";
        emblemColor = { 0.64f, 0.86f, 1.f, 0.f };
        accentColor = { 0.24f, 0.62f, 0.94f, 0.f };
    }
    else if (arenaPrefix == "WallBoss") {
        emblem = assetManager_->erdMoreEmblem();
        category = L"보스 전투 구역";
        title = L"에르드모어";
        emblemColor = { 1.f, 0.68f, 0.48f, 0.f };
        accentColor = { 0.94f, 0.08f, 0.05f, 0.f };
    }
    else {
        return false;
    }

    emblem_->texture = emblem;
    emblem_->colorMul = emblemColor;
    lineLeft_->colorTint = accentColor;
    lineRight_->colorTint = accentColor;
    category_->setText(category);
    title_->setText(title);

    hasWarning_ = (arenaPrefix == "WallBoss");
    warningScrim_->visible = hasWarning_;
    warningNoise_->visible = hasWarning_;
    warningGlitchRed_->visible = hasWarning_;
    warningGlitchCyan_->visible = hasWarning_;
    warningText_->visible = hasWarning_;
    warningTextRed_->visible = hasWarning_;
    warningTextCyan_->visible = hasWarning_;

    elapsed_ = 0.f;
    active_ = true;
    root_->visible = true;
    return true;
}

void TacticalZoneIntro::update(float deltaTimeSec) {
    if (!active_ || !root_) return;

    elapsed_ += std::max(0.f, deltaTimeSec);
    constexpr float kWarningDuration = 1.4f;
    const float warningT = elapsed_;

    if (hasWarning_ && warningT < kWarningDuration) {
        const float flicker = 0.55f + 0.45f * std::fabs(std::sin(warningT * 31.f));
        const float burst = std::max(0.f, std::sin(warningT * 47.f));
        const float edgeFade = std::min(
            std::clamp(warningT / 0.12f, 0.f, 1.f),
            std::clamp((kWarningDuration - warningT) / 0.18f, 0.f, 1.f)
        );
        const float warningAlpha = flicker * edgeFade;

        scrim_->colorTint.a = 0.f;
        banner_->colorMul.w = 0.f;
        emblem_->colorMul.w = 0.f;
        lineLeft_->colorTint.a = 0.f;
        lineRight_->colorTint.a = 0.f;
        category_->colorTint.a = 0.f;
        title_->colorTint.a = 0.f;

        warningScrim_->colorTint.a = 0.82f * edgeFade;
        warningNoise_->colorMul.w = (0.08f + 0.34f * burst) * edgeFade;
        warningNoise_->offsetX = DimValue::px(std::sin(warningT * 73.f) * 5.f);

        warningGlitchRed_->colorMul.w = (0.22f + 0.5f * burst) * edgeFade;
        warningGlitchCyan_->colorMul.w = (0.12f + 0.38f * burst) * edgeFade;
        warningGlitchRed_->offsetX = DimValue::px(-8.f + std::sin(warningT * 53.f) * 12.f);
        warningGlitchCyan_->offsetX = DimValue::px(8.f + std::cos(warningT * 59.f) * 12.f);

        const float jitterX = std::sin(warningT * 67.f) * (3.f + 9.f * burst);
        const float jitterY = std::cos(warningT * 43.f) * (1.f + 3.f * burst);
        warningText_->offsetX = DimValue::px(jitterX);
        warningText_->offsetY = DimValue::px(jitterY);
        warningText_->colorTint.a = warningAlpha;
        warningTextRed_->offsetX = DimValue::px(jitterX - 8.f);
        warningTextRed_->offsetY = DimValue::px(jitterY);
        warningTextRed_->colorTint.a = 0.42f * warningAlpha;
        warningTextCyan_->offsetX = DimValue::px(jitterX + 8.f);
        warningTextCyan_->offsetY = DimValue::px(jitterY + 1.f);
        warningTextCyan_->colorTint.a = 0.32f * warningAlpha;
        return;
    }

    if (hasWarning_) {
        warningScrim_->visible = false;
        warningNoise_->visible = false;
        warningGlitchRed_->visible = false;
        warningGlitchCyan_->visible = false;
        warningText_->visible = false;
        warningTextRed_->visible = false;
        warningTextCyan_->visible = false;
    }

    const float t = elapsed_ - (hasWarning_ ? kWarningDuration : 0.f);
    constexpr float kFadeInEnd = 0.42f;
    constexpr float kFadeOutStart = 3.15f;
    constexpr float kDuration = 4.25f;

    auto smooth01 = [](float v) {
        v = std::clamp(v, 0.f, 1.f);
        return v * v * (3.f - 2.f * v);
    };
    auto interval = [&](float begin, float end) {
        return smooth01((t - begin) / std::max(0.001f, end - begin));
    };

    const float fadeIn = interval(0.f, kFadeInEnd);
    const float fadeOut = 1.f - interval(kFadeOutStart, kDuration);
    const float globalAlpha = fadeIn * fadeOut;
    const float categoryAlpha = interval(0.18f, 0.62f) * fadeOut;
    const float titleAlpha = interval(0.38f, 0.88f) * fadeOut;
    const float lineAlpha = interval(0.08f, 0.48f) * fadeOut;

    scrim_->colorTint.a = 0.76f * globalAlpha;
    banner_->colorMul.w = 0.94f * globalAlpha;
    emblem_->colorMul.w = 0.62f * interval(0.12f, 0.62f) * fadeOut;
    lineLeft_->colorTint.a = 0.9f * lineAlpha;
    lineRight_->colorTint.a = 0.9f * lineAlpha;
    category_->colorTint.a = categoryAlpha;
    title_->colorTint.a = titleAlpha;

    const float bannerReveal = interval(0.f, 0.48f);
    banner_->width = DimValue::px(760.f + 220.f * bannerReveal);
    emblem_->width = DimValue::px(268.f - 20.f * interval(0.12f, 0.82f));
    emblem_->height = emblem_->width;
    category_->offsetY = DimValue::px(-62.f - 8.f * categoryAlpha);
    title_->offsetY = DimValue::px(10.f - 12.f * titleAlpha);

    const float lineWidth = 92.f * interval(0.08f, 0.52f);
    lineLeft_->width = DimValue::px(lineWidth);
    lineRight_->width = DimValue::px(lineWidth);

    if (t >= kDuration) {
        active_ = false;
        hasWarning_ = false;
        root_->visible = false;
    }
}

}   // namespace UI
