#include "../../pch.hpp"
#include "TacticalDialogueOverlay.hpp"
#include "../../AssetManager.hpp"

namespace UI {
namespace {

constexpr const wchar_t* kTacticalDialogueFontFamily = L"넥슨Lv2고딕";

struct TacticalDialogueDefinition {
    const wchar_t* speaker;
    const wchar_t* dialogue;
    const Texture* (AssetManager::*emblem)() const;
    Color accent;
};

const TacticalDialogueDefinition* definitionFor(TacticalDialogueId dialogueId) {
    static const TacticalDialogueDefinition definitions[] = {
        {
            L"홉 고블린",
            L"포위해!",
            &AssetManager::hobgoblinEmblem,
            { 0.85f, 0.20f, 0.12f, 1.f },
        },
        {
            L"그랜드밤",
            L"더이상 다가올 수 없다!",
            &AssetManager::grandbaumEmblem,
            { 0.88f, 0.52f, 0.16f, 1.f },
        },
        {
            L"이시스",
            L"돌격준비!",
            &AssetManager::isysEmblem,
            { 0.24f, 0.62f, 0.94f, 1.f },
        },
    };
    static_assert(std::size(definitions) ==
        static_cast<size_t>(TacticalDialogueId::Count));

    const auto index = static_cast<size_t>(dialogueId);
    if (index >= std::size(definitions) ||
        dialogueId == TacticalDialogueId::Count) {
        return nullptr;
    }
    return &definitions[index];
}

float smooth01(float value) {
    value = std::clamp(value, 0.f, 1.f);
    return value * value * (3.f - 2.f * value);
}

} // namespace

void TacticalDialogueOverlay::init(UIManager& uiManager, AssetManager& assetManager) {
    uiManager_ = &uiManager;
    assetManager_ = &assetManager;
    buildUI();
}

void TacticalDialogueOverlay::buildUI() {
    if (root_ || !uiManager_ || !assetManager_) return;

    root_ = static_cast<Panel*>(
        uiManager_->root()->addChild(std::make_unique<Panel>())
    );
    root_->name = "tacticalDialogueOverlay";
    root_->anchor = Anchors::TopCenter;
    root_->pivot = Pivots::TopCenter;
    root_->offsetY = DimValue::px(38.f);
    root_->width = DimValue::px(860.f);
    root_->height = DimValue::px(176.f);
    root_->zOrder = 950;
    root_->visible = false;
    root_->interactive = false;

    banner_ = static_cast<Image*>(
        root_->addChild(std::make_unique<Image>())
    );
    banner_->name = "tacticalDialogueBanner";
    banner_->anchor = Anchors::Center;
    banner_->pivot = Pivots::Center;
    banner_->width = DimValue::px(860.f);
    banner_->height = DimValue::px(287.f);
    banner_->texture = assetManager_->tacticalZoneTitleBanner();
    banner_->colorMul = { 0.075f, 0.082f, 0.095f, 0.0f };
    banner_->zOrder = 0;

    accent_ = static_cast<Panel*>(
        root_->addChild(std::make_unique<Panel>())
    );

    emblem_ = static_cast<Image*>(
        root_->addChild(std::make_unique<Image>())
    );
    emblem_->name = "tacticalDialogueEmblem";
    emblem_->anchor = Anchors::TopLeft;
    emblem_->pivot = Pivots::TopLeft;
    emblem_->offsetX = DimValue::px(45.f);
    emblem_->offsetY = DimValue::px(24.f);
    emblem_->width = DimValue::px(128.f);
    emblem_->height = DimValue::px(128.f);
    emblem_->zOrder = 2;

    category_ = static_cast<Label*>(
        root_->addChild(std::make_unique<Label>())
    );
    category_->name = "tacticalDialogueCategory";
    category_->anchor = Anchors::TopLeft;
    category_->pivot = Pivots::TopLeft;
    category_->offsetX = DimValue::px(198.f);
    category_->offsetY = DimValue::px(24.f);
    category_->width = DimValue::px(130.f);
    category_->height = DimValue::px(28.f);
    category_->setText(L"전술 지령");
    category_->setFontFamily(kTacticalDialogueFontFamily);
    category_->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
    category_->setFontSize(15.f);
    category_->setTextHAlign(TextHAlign::Leading);
    category_->setTextVAlign(TextVAlign::Center);
    category_->zOrder = 3;

    speaker_ = static_cast<Label*>(
        root_->addChild(std::make_unique<Label>())
    );
    speaker_->name = "tacticalDialogueSpeaker";
    speaker_->anchor = Anchors::TopLeft;
    speaker_->pivot = Pivots::TopLeft;
    speaker_->offsetX = DimValue::px(332.f);
    speaker_->offsetY = DimValue::px(24.f);
    speaker_->width = DimValue::px(448.f);
    speaker_->height = DimValue::px(28.f);
    speaker_->setFontFamily(kTacticalDialogueFontFamily);
    speaker_->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
    speaker_->setFontSize(17.f);
    speaker_->setTextHAlign(TextHAlign::Leading);
    speaker_->setTextVAlign(TextVAlign::Center);
    speaker_->setTextColor(0.76f, 0.78f, 0.82f, 1.f);
    speaker_->zOrder = 3;

    dialogue_ = static_cast<Label*>(
        root_->addChild(std::make_unique<Label>())
    );
    dialogue_->name = "tacticalDialogueText";
    dialogue_->anchor = Anchors::Center;
    dialogue_->pivot = Pivots::Center;
    //dialogue_->offsetX = DimValue::px(100.f);
    //dialogue_->offsetY = DimValue::px(70.f);
    dialogue_->width = DimValue::px(582.f);
    dialogue_->height = DimValue::px(74.f);
    dialogue_->setFontFamily(kTacticalDialogueFontFamily);
    dialogue_->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
    dialogue_->setFontSize(25.f);
    dialogue_->setTextHAlign(TextHAlign::Center);
    dialogue_->setTextVAlign(TextVAlign::Center);
    dialogue_->setTextColor(0.96f, 0.95f, 0.92f, 1.f);
    dialogue_->zOrder = 3;
}

bool TacticalDialogueOverlay::trigger(TacticalDialogueId dialogueId) {
    if (!root_) buildUI();
    if (!root_ || !assetManager_) return false;

    const TacticalDialogueDefinition* definition = definitionFor(dialogueId);
    if (!definition) return false;

    emblem_->texture = (assetManager_->*(definition->emblem))();
    emblem_->colorMul = { 1.f, 1.f, 1.f, 0.f };
    activeAccent_ = definition->accent;
    accent_->colorTint = definition->accent;
    category_->setTextColor(
        definition->accent.r,
        definition->accent.g,
        definition->accent.b,
        1.f);
    speaker_->setText(definition->speaker);
    dialogue_->setText(definition->dialogue);

    elapsed_ = 0.f;
    active_ = true;
    root_->visible = true;
    applyAlpha(0.f);
    return true;
}

void TacticalDialogueOverlay::update(float deltaTimeSec) {
    if (!active_ || !root_) return;

    constexpr float kFadeInDuration = 0.2f;
    constexpr float kFadeOutStart = 2.5f;
    constexpr float kDuration = 3.0f;

    elapsed_ += std::max(0.f, deltaTimeSec);
    float alpha = smooth01(elapsed_ / kFadeInDuration);
    if (elapsed_ >= kFadeOutStart) {
        alpha *= 1.f - smooth01(
            (elapsed_ - kFadeOutStart) / (kDuration - kFadeOutStart));
    }
    applyAlpha(alpha);

    if (elapsed_ >= kDuration) {
        active_ = false;
        root_->visible = false;
    }
}

void TacticalDialogueOverlay::applyAlpha(float alpha) {
    if (!root_) return;
    alpha = std::clamp(alpha, 0.f, 1.f);

    const float reveal = smooth01(std::min(elapsed_ / 0.28f, 1.f));
    banner_->width = DimValue::px(720.f + 140.f * reveal);
    banner_->colorMul.w = 0.54f * alpha;
    emblem_->colorMul.w = alpha;
    category_->colorTint.a = alpha;
    speaker_->colorTint.a = alpha;
    dialogue_->colorTint.a = alpha;

    const float textOffset = 12.f * (1.f - reveal);
    category_->offsetX = DimValue::px(198.f + textOffset);
    speaker_->offsetX = DimValue::px(332.f + textOffset);
    // Keep the dialogue label's configured base position.  Writing offsetX
    // here would overwrite buildUI()'s value on every frame.
}

} // namespace UI
