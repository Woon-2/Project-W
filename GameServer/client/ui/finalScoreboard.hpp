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
	struct Entry {
		std::wstring nickname;
		int monsterKills = 0;
	};

	struct Style {
		const Texture* panelTexture = nullptr;
		const Texture* buttonTexture = nullptr;
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
	std::array<Label*, 4> nameLabels_{};
	std::array<Label*, 4> valueLabels_{};
	std::function<void()> onExit_;
	bool visible_ = false;
};

}  // namespace UI

#endif  // __FINAL_SCOREBOARD_HPP
