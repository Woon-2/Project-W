#ifndef __FINAL_SCOREBOARD_HPP
#define __FINAL_SCOREBOARD_HPP

#include <array>
#include <functional>
#include <string>
#include <vector>

struct Texture;

namespace UI {

class Label;
class UIElement;
class UIManager;

class FinalScoreboard {
public:
	static constexpr int kBossLastHitBonus = 50;

	enum class Easing {
		Linear,
		EaseIn,
		EaseOut
	};

	struct Entry {
		std::wstring nickname;
		int pickedItems = 0;
		int damageDealt = 0;
		int monsterKills = 0;
		int bossLastHitBonus = 0;

		int totalScore() const {
			return pickedItems + damageDealt + monsterKills + bossLastHitBonus;
		}
	};

	struct Style {
		const Texture* panelTexture = nullptr;
		const Texture* buttonTexture = nullptr;
		const Texture* titleBannerTexture = nullptr;
		Easing revealEasing = Easing::EaseOut;
	};

	void build(UIManager& manager, Style style, std::function<void()> onExit);

	// Entries are displayed in the order supplied by the game.
	bool show(const std::vector<Entry>& entries);
	void update(float deltaTimeSec);
	void hide();
	bool isVisible() const { return visible_; }

private:
	static float evaluateEasing(float t, Easing easing);
	void syncToScreen();
	float revealStartOffsetY() const;

	UIManager* manager_ = nullptr;
	UIElement* root_ = nullptr;  // owned by UIManager
	UIElement* card_ = nullptr;  // owned by root_
	std::array<UIElement*, 4> rowRoots_{};
	std::array<Label*, 4> nameLabels_{};
	std::array<Label*, 4> rankLabels_{};
	std::array<Label*, 4> totalLabels_{};
	std::array<Label*, 4> itemLabels_{};
	std::array<Label*, 4> damageLabels_{};
	std::array<Label*, 4> killLabels_{};
	std::array<Label*, 4> bossBonusLabels_{};
	std::array<UIElement*, 4> mvpBadges_{};
	std::array<UIElement*, 4> bossLastHitBadges_{};
	std::function<void()> onExit_;
	Easing revealEasing_ = Easing::EaseOut;
	float revealElapsedSec_ = 0.f;
	bool visible_ = false;
};

}  // namespace UI

#endif  // __FINAL_SCOREBOARD_HPP
