#ifndef __UI_DIALOGUE_SYSTEM_HPP
#define __UI_DIALOGUE_SYSTEM_HPP

#include "../UIManager.hpp"
#include "../widgets/Label.hpp"
#include "../widgets/Panel.hpp"

namespace UI {

struct DialogueStyle {
    float x = 128.f;
    float y = 504.f;
    float width = 768.f;
    float height = 176.f;
    Color background{ 0.03f, 0.05f, 0.08f, 0.82f };
    Color borderColor{ 0.48f, 0.62f, 0.76f, 0.90f };
    float borderWidth = 2.f;
    Color textColor{ 1.f, 1.f, 1.f, 1.f };
    Color hintColor{ 0.66f, 0.80f, 0.92f, 0.92f };
    float padding = 24.f;
    std::wstring fontFamily = L"Malgun Gothic";
    float fontSize = 24.f;
    float hintFontSize = 15.f;
    float charactersPerSecond = 30.f;
    float fadeOutSeconds = 0.45f;
};

struct DialogueDefinition {
    std::string eventId;
    DialogueStyle style;
    std::vector<std::wstring> pages;
    std::wstring advanceHint = L"클릭하여 계속  ▶";
    std::wstring closeHint = L"클릭하여 닫기  ▶";
};

class DialogueSystem {
public:
    bool init(UIManager& uiManager, const std::filesystem::path& jsonPath);
    bool reload();

    bool show(std::string_view eventId);
    void advance();
    void update(float deltaTimeSec);
    bool handleWndMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    bool active() const { return state_ != State::Hidden; }
    std::string_view activeEventId() const { return activeEventId_; }
    std::size_t pageIndex() const { return pageIndex_; }

private:
    enum class State { Hidden, Reading, Fading };

    bool loadDefinitions();
    void applyDefinition(const DialogueDefinition& definition);
    void beginPage();
    void updateTypewriter(float deltaTimeSec);
    void revealCurrentPage();
    bool currentPageFullyRevealed() const;
    void updateHintText();
    void applyAlpha(float alpha);
    void hide();

    UIManager* uiManager_ = nullptr;
    Panel* panel_ = nullptr;
    Label* label_ = nullptr;
    Label* hintLabel_ = nullptr;
    std::array<Panel*, 4> borderPanels_{};
    std::filesystem::path jsonPath_;
    std::unordered_map<std::string, DialogueDefinition> definitions_;

    State state_ = State::Hidden;
    std::string activeEventId_;
    const DialogueDefinition* activeDefinition_ = nullptr;
    std::size_t pageIndex_ = 0;
    float fadeElapsed_ = 0.f;
    float hintPulseElapsed_ = 0.f;
    std::size_t revealedTextLength_ = 0;
    float typewriterAccumulator_ = 0.f;
};

} // namespace UI

#endif // __UI_DIALOGUE_SYSTEM_HPP
