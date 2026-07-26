#include "pch.hpp"
#include "finalScoreboard.hpp"

#include "UIManager.hpp"
#include "widgets/Button.hpp"
#include "widgets/Label.hpp"
#include "widgets/Panel.hpp"
#include "../gfx.hpp"

namespace UI {
namespace {

constexpr float kCardWidth = 600.f;
constexpr float kCardHeight = 568.f;
constexpr float kContentX = 58.f;
constexpr float kContentWidth = kCardWidth - kContentX * 2.f;
constexpr float kScoreColumnWidth = 150.f;
constexpr float kRowTop = 180.f;
constexpr float kRowHeight = 61.f;

Label* addLabel(UIElement* parent, const std::wstring& text,
	float x, float y, float w, float h, float fontSize,
	TextHAlign hAlign, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR) {
	auto* label = static_cast<Label*>(
		parent->addChild(std::make_unique<Label>()));
	label->anchor = Anchors::TopLeft;
	label->pivot = Pivots::TopLeft;
	label->offsetX = DimValue::px(x);
	label->offsetY = DimValue::px(y);
	label->width = DimValue::px(w);
	label->height = DimValue::px(h);
	label->setText(text);
	label->setFontSize(fontSize);
	label->setFontWeight(weight);
	label->setTextColor(0.07f, 0.11f, 0.17f, 1.f);
	label->setTextHAlign(hAlign);
	label->setTextVAlign(TextVAlign::Center);
	label->zOrder = 2;
	return label;
}

}  // namespace

void FinalScoreboard::build(UIManager& manager, Style style,
	std::function<void()> onExit) {
	if (root_) {
		root_->visible = false;
	}

	manager_ = &manager;
	onExit_ = std::move(onExit);
	visible_ = false;
	nameLabels_.fill(nullptr);
	valueLabels_.fill(nullptr);

	auto* overlay = static_cast<Panel*>(
		manager.root()->addChild(std::make_unique<Panel>()));
	root_ = overlay;
	overlay->name = "finalScoreboardRoot";
	overlay->anchor = Anchors::Center;
	overlay->pivot = Pivots::Center;
	overlay->drawSolidBackground = true;
	overlay->colorTint = { 0.01f, 0.015f, 0.025f, 0.72f };
	overlay->zOrder = 4000;
	overlay->visible = false;
	syncToScreen();

	auto* card = static_cast<Button*>(
		overlay->addChild(std::make_unique<Button>()));
	card->name = "finalScoreboardCard";
	card->anchor = Anchors::Center;
	card->pivot = Pivots::Center;
	card->width = DimValue::px(kCardWidth);
	card->height = DimValue::px(kCardHeight);
	card->interactive = false;
	card->bgColor = { 0.96f, 0.975f, 0.99f, 0.98f };
	if (style.panelTexture && style.panelTexture->res) {
		card->texNormal = style.panelTexture;
		card->sliceUvBorderX = 0.30f;
		card->sliceUvBorderY = 0.30f;
		card->sliceCornerX = 42.f;
		card->sliceCornerY = 42.f;
		card->texTint = { 1.f, 1.f, 1.f, 0.99f };
		card->texTintHovered = card->texTint;
		card->texTintPressed = card->texTint;
	}

	addLabel(card, L"SCORE", 0.f, 28.f, kCardWidth, 62.f, 44.f,
		TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD);
	addLabel(card, L"닉네임", kContentX, 116.f,
		kContentWidth - kScoreColumnWidth, 38.f, 19.f,
		TextHAlign::Leading, DWRITE_FONT_WEIGHT_BOLD);
	addLabel(card, L"처치 수", kContentX + kContentWidth - kScoreColumnWidth, 116.f,
		kScoreColumnWidth, 38.f, 19.f,
		TextHAlign::Trailing, DWRITE_FONT_WEIGHT_BOLD);

	auto* divider = static_cast<Panel*>(
		card->addChild(std::make_unique<Panel>()));
	divider->anchor = Anchors::TopLeft;
	divider->pivot = Pivots::TopLeft;
	divider->offsetX = DimValue::px(kContentX);
	divider->offsetY = DimValue::px(160.f);
	divider->width = DimValue::px(kContentWidth);
	divider->height = DimValue::px(2.f);
	divider->drawSolidBackground = true;
	divider->colorTint = { 0.12f, 0.28f, 0.40f, 0.35f };
	divider->zOrder = 1;

	for (std::size_t i = 0; i < nameLabels_.size(); ++i) {
		const float rowY = kRowTop + static_cast<float>(i) * kRowHeight;

		auto* rowBg = static_cast<Panel*>(
			card->addChild(std::make_unique<Panel>()));
		rowBg->anchor = Anchors::TopLeft;
		rowBg->pivot = Pivots::TopLeft;
		rowBg->offsetX = DimValue::px(kContentX - 12.f);
		rowBg->offsetY = DimValue::px(rowY);
		rowBg->width = DimValue::px(kContentWidth + 24.f);
		rowBg->height = DimValue::px(kRowHeight - 5.f);
		rowBg->drawSolidBackground = true;
		rowBg->colorTint = (i % 2 == 0)
			? Color{ 0.76f, 0.88f, 0.95f, 0.22f }
			: Color{ 0.88f, 0.93f, 0.97f, 0.12f };
		rowBg->zOrder = 0;

		nameLabels_[i] = addLabel(card, L"", kContentX, rowY,
			kContentWidth - kScoreColumnWidth, kRowHeight - 5.f, 24.f,
			TextHAlign::Leading, DWRITE_FONT_WEIGHT_SEMI_BOLD);
		valueLabels_[i] = addLabel(card, L"",
			kContentX + kContentWidth - kScoreColumnWidth, rowY,
			kScoreColumnWidth, kRowHeight - 5.f, 26.f,
			TextHAlign::Trailing, DWRITE_FONT_WEIGHT_BOLD);
	}

	auto* exitButton = static_cast<Button*>(
		card->addChild(std::make_unique<Button>()));
	exitButton->name = "finalScoreboardExitButton";
	exitButton->anchor = Anchors::TopLeft;
	exitButton->pivot = Pivots::TopLeft;
	exitButton->offsetX = DimValue::px(kCardWidth - kContentX - 168.f);
	exitButton->offsetY = DimValue::px(kCardHeight - 78.f);
	exitButton->width = DimValue::px(168.f);
	exitButton->height = DimValue::px(50.f);
	exitButton->zOrder = 4010;
	exitButton->bgColor = { 0.08f, 0.36f, 0.55f, 1.f };
	exitButton->bgColorHovered = { 0.055f, 0.286f, 0.435f, 1.f };
	exitButton->bgColorPressed = { 0.035f, 0.22f, 0.34f, 1.f };
	if (style.buttonTexture && style.buttonTexture->res) {
		exitButton->texNormal = style.buttonTexture;
		exitButton->sliceUvBorderX = 0.40f;
		exitButton->sliceUvBorderY = 0.40f;
		exitButton->sliceCornerX = 22.f;
		exitButton->sliceCornerY = 22.f;
	}
	auto* exitLabel = addLabel(exitButton, L"나가기", 0.f, 0.f, 168.f, 50.f, 22.f,
		TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD);
	exitLabel->setTextColor(0.04f, 0.11f, 0.17f, 1.f);
	exitButton->onClick = [this]() {
		if (onExit_) {
			onExit_();
		}
	};
}

bool FinalScoreboard::show(const std::vector<Entry>& entries) {
	if (!root_ || visible_) {
		return false;
	}

	for (std::size_t i = 0; i < nameLabels_.size(); ++i) {
		const bool hasEntry = i < entries.size();
		if (nameLabels_[i]) {
			nameLabels_[i]->visible = hasEntry;
			nameLabels_[i]->setText(hasEntry ? entries[i].nickname : L"");
		}
		if (valueLabels_[i]) {
			valueLabels_[i]->visible = hasEntry;
			valueLabels_[i]->setText(
				hasEntry ? std::to_wstring(entries[i].monsterKills) : L"");
		}
	}

	syncToScreen();
	visible_ = true;
	root_->visible = true;
	return true;
}

void FinalScoreboard::hide() {
	visible_ = false;
	if (root_) {
		root_->visible = false;
	}
}

void FinalScoreboard::syncToScreen() {
	if (!manager_ || !root_) {
		return;
	}

	root_->width = DimValue::px(manager_->screenWidth() / manager_->uiScale());
	root_->height = DimValue::px(manager_->screenHeight() / manager_->uiScale());
}

}  // namespace UI
