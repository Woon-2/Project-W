#include "pch.hpp"
#include "finalScoreboard.hpp"

#include "UIManager.hpp"
#include "widgets/Button.hpp"
#include "widgets/Image.hpp"
#include "widgets/Label.hpp"
#include "widgets/Panel.hpp"
#include "../gfx.hpp"

namespace UI {
namespace {

constexpr float kCardWidth = 900.f;
constexpr float kCardHeight = 720.f;
constexpr float kContentX = 48.f;
constexpr float kContentWidth = kCardWidth - kContentX * 2.f;
constexpr float kRowTop = 179.f;
constexpr float kRowHeight = 110.f;
constexpr float kRowStride = 118.f;
constexpr float kBodyX = 58.f;
constexpr float kBodyWidth = 590.f;
constexpr float kTotalX = 664.f;
constexpr float kTotalWidth = kContentWidth - kTotalX;
constexpr float kMetricWidth = 142.f;
constexpr float kMetricGap = 5.f;

constexpr Color kInk{ 0.055f, 0.10f, 0.15f, 1.f };
constexpr Color kMutedInk{ 0.26f, 0.34f, 0.40f, 1.f };
constexpr Color kBlue{ 0.07f, 0.34f, 0.53f, 1.f };
constexpr Color kGold{ 0.64f, 0.43f, 0.09f, 1.f };

Panel* addPanel(UIElement* parent, const char* name,
	float x, float y, float w, float h, const Color& color, int zOrder = 0) {
	auto* panel = static_cast<Panel*>(
		parent->addChild(std::make_unique<Panel>()));
	panel->name = name;
	panel->anchor = Anchors::TopLeft;
	panel->pivot = Pivots::TopLeft;
	panel->offsetX = DimValue::px(x);
	panel->offsetY = DimValue::px(y);
	panel->width = DimValue::px(w);
	panel->height = DimValue::px(h);
	panel->drawSolidBackground = true;
	panel->colorTint = color;
	panel->zOrder = zOrder;
	return panel;
}

Label* addLabel(UIElement* parent, const std::wstring& text,
	float x, float y, float w, float h, float fontSize,
	TextHAlign hAlign, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR,
	const Color& color = kInk, int zOrder = 2) {
	auto* label = static_cast<Label*>(
		parent->addChild(std::make_unique<Label>()));
	label->anchor = Anchors::TopLeft;
	label->pivot = Pivots::TopLeft;
	label->offsetX = DimValue::px(x);
	label->offsetY = DimValue::px(y);
	label->width = DimValue::px(w);
	label->height = DimValue::px(h);
	label->setText(text);
	label->setFontFamily(L"Malgun Gothic");
	label->setFontSize(fontSize);
	label->setFontWeight(weight);
	label->setTextColor(color.r, color.g, color.b, color.a);
	label->setTextHAlign(hAlign);
	label->setTextVAlign(TextVAlign::Center);
	label->zOrder = zOrder;
	return label;
}

UIElement* addBadge(UIElement* parent, const char* name, const std::wstring& text,
	float x, float y, float w, const Color& background, const Color& foreground) {
	auto* root = parent->addChild(std::make_unique<UIElement>());
	root->name = name;
	root->anchor = Anchors::TopLeft;
	root->pivot = Pivots::TopLeft;
	root->offsetX = DimValue::px(x);
	root->offsetY = DimValue::px(y);
	root->width = DimValue::px(w);
	root->height = DimValue::px(21.f);
	root->zOrder = 3;

	addPanel(root, "background", 0.f, 0.f, w, 21.f, background);
	addLabel(root, text, 0.f, 0.f, w, 21.f, 11.f,
		TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD, foreground);
	return root;
}

Label* addMetricCard(UIElement* row, std::size_t metricIndex,
	const wchar_t* code, const wchar_t* caption, bool bonus) {
	const float x = kBodyX + static_cast<float>(metricIndex) * (kMetricWidth + kMetricGap);
	const Color background = bonus
		? Color{ 0.91f, 0.87f, 0.70f, 0.48f }
		: Color{ 0.75f, 0.86f, 0.92f, 0.46f };
	auto* panel = addPanel(row, "scoreMetric", x, 47.f,
		kMetricWidth, 53.f, background, 1);

	addLabel(panel, code, 7.f, 3.f, 38.f, 18.f, 11.f,
		TextHAlign::Leading, DWRITE_FONT_WEIGHT_BOLD,
		bonus ? kGold : kBlue);
	addLabel(panel, caption, 42.f, 3.f, kMetricWidth - 49.f, 18.f, 11.f,
		TextHAlign::Trailing, DWRITE_FONT_WEIGHT_SEMI_BOLD, kMutedInk);
	return addLabel(panel, L"0", 7.f, 21.f, kMetricWidth - 14.f, 28.f, 18.f,
		TextHAlign::Trailing, DWRITE_FONT_WEIGHT_BOLD, bonus ? kGold : kInk);
}

std::wstring formatNumber(int value) {
	std::wstring result = std::to_wstring(std::max(value, 0));
	for (std::size_t insertAt = result.size(); insertAt > 3; insertAt -= 3) {
		result.insert(insertAt - 3, 1, L',');
	}
	return result;
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
	rowRoots_.fill(nullptr);
	nameLabels_.fill(nullptr);
	rankLabels_.fill(nullptr);
	totalLabels_.fill(nullptr);
	itemLabels_.fill(nullptr);
	damageLabels_.fill(nullptr);
	killLabels_.fill(nullptr);
	bossBonusLabels_.fill(nullptr);
	mvpBadges_.fill(nullptr);
	bossLastHitBadges_.fill(nullptr);

	auto* overlay = static_cast<Panel*>(
		manager.root()->addChild(std::make_unique<Panel>()));
	root_ = overlay;
	overlay->name = "finalScoreboardRoot";
	overlay->anchor = Anchors::Center;
	overlay->pivot = Pivots::Center;
	overlay->drawSolidBackground = true;
	overlay->colorTint = { 0.01f, 0.015f, 0.025f, 0.76f };
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
	card->bgColor = { 0.94f, 0.965f, 0.98f, 0.99f };
	if (style.panelTexture && style.panelTexture->res) {
		card->texNormal = style.panelTexture;
		card->sliceUvBorderX = 0.30f;
		card->sliceUvBorderY = 0.30f;
		card->sliceCornerX = 48.f;
		card->sliceCornerY = 48.f;
		card->texTint = { 1.f, 1.f, 1.f, 0.99f };
		card->texTintHovered = card->texTint;
		card->texTintPressed = card->texTint;
	}

	if (style.titleBannerTexture) {
		auto* banner = static_cast<Image*>(
			card->addChild(std::make_unique<Image>()));
		banner->name = "finalScoreboardTitleBanner";
		banner->anchor = Anchors::TopCenter;
		banner->pivot = Pivots::TopCenter;
		banner->offsetY = DimValue::px(18.f);
		banner->width = DimValue::px(500.f);
		banner->height = DimValue::px(92.f);
		banner->texture = style.titleBannerTexture;
		banner->colorMul = { 0.13f, 0.36f, 0.51f, 0.12f };
		banner->zOrder = 0;
	}

	addLabel(card, L"FINAL BOSS DEFEATED", 0.f, 21.f, kCardWidth, 28.f, 15.f,
		TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD, kBlue, 2);
	addLabel(card, L"SCORE", 0.f, 43.f, kCardWidth, 58.f, 42.f,
		TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD, kInk, 2);

	const std::array<const wchar_t*, 4> formulaLabels{
		L"아이템", L"대미지", L"처치", L"보스 막타"
	};
	const std::array<float, 4> formulaWidths{ 72.f, 84.f, 66.f, 104.f };
	float formulaX = 260.f;
	for (std::size_t i = 0; i < formulaLabels.size(); ++i) {
		auto* chip = addPanel(card, "scoreFormulaChip", formulaX, 108.f,
			formulaWidths[i], 25.f, { 0.74f, 0.86f, 0.92f, 0.65f }, 1);
		addLabel(chip, formulaLabels[i], 0.f, 0.f, formulaWidths[i], 25.f, 12.f,
			TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD, kMutedInk);
		formulaX += formulaWidths[i];
		if (i + 1 < formulaLabels.size()) {
			addLabel(card, L"+", formulaX, 108.f, 18.f, 25.f, 14.f,
				TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD, kBlue);
			formulaX += 18.f;
		}
	}

	addLabel(card, L"닉네임 / 점수 상세", kContentX + kBodyX, 141.f,
		kBodyWidth, 26.f, 14.f, TextHAlign::Leading,
		DWRITE_FONT_WEIGHT_BOLD, kMutedInk);
	addLabel(card, L"총점", kContentX + kTotalX, 141.f,
		kTotalWidth, 26.f, 14.f, TextHAlign::Trailing,
		DWRITE_FONT_WEIGHT_BOLD, kMutedInk);
	addPanel(card, "finalScoreboardHeaderDivider", kContentX, 169.f,
		kContentWidth, 2.f, { 0.08f, 0.31f, 0.48f, 0.42f }, 1);

	for (std::size_t i = 0; i < rowRoots_.size(); ++i) {
		auto* row = card->addChild(std::make_unique<UIElement>());
		rowRoots_[i] = row;
		row->name = "finalScoreboardRow";
		row->anchor = Anchors::TopLeft;
		row->pivot = Pivots::TopLeft;
		row->offsetX = DimValue::px(kContentX);
		row->offsetY = DimValue::px(kRowTop + static_cast<float>(i) * kRowStride);
		row->width = DimValue::px(kContentWidth);
		row->height = DimValue::px(kRowHeight);
		row->zOrder = 1;

		const bool winnerRow = i == 0;
		addPanel(row, "scoreRowBackground", 0.f, 0.f,
			kContentWidth, kRowHeight,
			winnerRow
				? Color{ 0.90f, 0.84f, 0.61f, 0.34f }
				: Color{ 0.76f, 0.87f, 0.93f, i % 2 == 0 ? 0.30f : 0.20f });
		addPanel(row, "scoreRowAccent", 0.f, 0.f, 4.f, kRowHeight,
			winnerRow ? kGold : kBlue, 1);

		rankLabels_[i] = addLabel(row, L"00", 8.f, 0.f, 43.f, kRowHeight, 18.f,
			TextHAlign::Center, DWRITE_FONT_WEIGHT_BOLD,
			winnerRow ? kGold : kBlue, 2);
		nameLabels_[i] = addLabel(row, L"", kBodyX, 6.f, 250.f, 34.f, 21.f,
			TextHAlign::Leading, DWRITE_FONT_WEIGHT_BOLD, kInk, 2);
		mvpBadges_[i] = addBadge(row, "finalScoreboardMvpBadge", L"MVP",
			kBodyX + 260.f, 9.f, 54.f,
			{ 0.70f, 0.48f, 0.10f, 0.92f }, { 1.f, 0.97f, 0.83f, 1.f });
		bossLastHitBadges_[i] = addBadge(row, "finalScoreboardBossBadge",
			L"BOSS LAST HIT", kBodyX + 321.f, 9.f, 121.f,
			{ 0.09f, 0.32f, 0.46f, 0.94f }, { 0.91f, 0.97f, 1.f, 1.f });

		itemLabels_[i] = addMetricCard(row, 0, L"ITM", L"주운 아이템", false);
		damageLabels_[i] = addMetricCard(row, 1, L"DMG", L"누적 대미지", false);
		killLabels_[i] = addMetricCard(row, 2, L"KILL", L"몬스터 처치", false);
		bossBonusLabels_[i] = addMetricCard(row, 3, L"BOSS", L"막타 보너스", true);

		addPanel(row, "scoreTotalDivider", kTotalX - 9.f, 17.f,
			2.f, kRowHeight - 34.f,
			{ 0.08f, 0.31f, 0.48f, 0.25f }, 1);
		addLabel(row, L"TOTAL", kTotalX, 14.f, kTotalWidth, 20.f, 11.f,
			TextHAlign::Trailing, DWRITE_FONT_WEIGHT_BOLD, kMutedInk);
		totalLabels_[i] = addLabel(row, L"0", kTotalX, 31.f,
			kTotalWidth, 37.f, 24.f, TextHAlign::Trailing,
			DWRITE_FONT_WEIGHT_BOLD, winnerRow ? kGold : kInk);
		addLabel(row, L"PTS", kTotalX, 66.f, kTotalWidth, 18.f, 10.f,
			TextHAlign::Trailing, DWRITE_FONT_WEIGHT_BOLD, kMutedInk);
	}

	addPanel(card, "finalScoreboardFooterDivider", kContentX, 651.f,
		kContentWidth, 2.f, { 0.08f, 0.31f, 0.48f, 0.32f }, 1);
	addLabel(card, L"점수 = 아이템 + 대미지 + 처치 + 보스 막타 보너스(50)",
		kContentX, 657.f, 590.f, 44.f, 13.f, TextHAlign::Leading,
		DWRITE_FONT_WEIGHT_SEMI_BOLD, kMutedInk);

	auto* exitButton = static_cast<Button*>(
		card->addChild(std::make_unique<Button>()));
	exitButton->name = "finalScoreboardExitButton";
	exitButton->anchor = Anchors::TopLeft;
	exitButton->pivot = Pivots::TopLeft;
	exitButton->offsetX = DimValue::px(kCardWidth - kContentX - 158.f);
	exitButton->offsetY = DimValue::px(658.f);
	exitButton->width = DimValue::px(158.f);
	exitButton->height = DimValue::px(46.f);
	exitButton->zOrder = 10;
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
	auto* exitLabel = addLabel(exitButton, L"나가기   →", 0.f, 0.f,
		158.f, 46.f, 18.f, TextHAlign::Center,
		DWRITE_FONT_WEIGHT_BOLD, kInk);
	exitLabel->interactive = false;
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

	for (std::size_t i = 0; i < rowRoots_.size(); ++i) {
		const bool hasEntry = i < entries.size();
		if (rowRoots_[i]) {
			rowRoots_[i]->visible = hasEntry;
		}
		if (!hasEntry) {
			continue;
		}

		const Entry& entry = entries[i];
		rankLabels_[i]->setText(i < 9
			? L"0" + std::to_wstring(i + 1)
			: std::to_wstring(i + 1));
		nameLabels_[i]->setText(entry.nickname);
		itemLabels_[i]->setText(formatNumber(entry.pickedItems));
		damageLabels_[i]->setText(formatNumber(entry.damageDealt));
		killLabels_[i]->setText(formatNumber(entry.monsterKills));
		bossBonusLabels_[i]->setText(entry.bossLastHitBonus > 0
			? L"+" + formatNumber(entry.bossLastHitBonus)
			: L"0");
		bossBonusLabels_[i]->setTextColor(
			entry.bossLastHitBonus > 0 ? kGold.r : kMutedInk.r,
			entry.bossLastHitBonus > 0 ? kGold.g : kMutedInk.g,
			entry.bossLastHitBonus > 0 ? kGold.b : kMutedInk.b,
			1.f);
		totalLabels_[i]->setText(formatNumber(entry.totalScore()));
		mvpBadges_[i]->visible = i == 0;
		bossLastHitBadges_[i]->visible = entry.bossLastHitBonus > 0;
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
