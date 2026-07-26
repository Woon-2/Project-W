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
	};

	void build(UIManager& manager, Style style, std::function<void()> onExit);

	// Entries are displayed in the order supplied by the game.
	bool show(const std::vector<Entry>& entries);
	void hide();
	bool isVisible() const { return visible_; }

private:
	void syncToScreen();

	UIManager* manager_ = nullptr;
	UIElement* root_ = nullptr;  // owned by UIManager
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
	bool visible_ = false;
};

}  // namespace UI

#endif  // __FINAL_SCOREBOARD_HPP
