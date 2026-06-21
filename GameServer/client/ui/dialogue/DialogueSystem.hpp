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
    Color textColor{ 1.f, 1.f, 1.f, 1.f };
    float padding = 24.f;
    std::wstring fontFamily = L"Malgun Gothic";
    float fontSize = 24.f;
    float fadeOutSeconds = 0.45f;
};

struct DialogueDefinition {
    std::string eventId;
    DialogueStyle style;
    std::vector<std::wstring> pages;
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
    void applyAlpha(float alpha);
    void hide();

    UIManager* uiManager_ = nullptr;
    Panel* panel_ = nullptr;
    Label* label_ = nullptr;
    std::filesystem::path jsonPath_;
    std::unordered_map<std::string, DialogueDefinition> definitions_;

    State state_ = State::Hidden;
    std::string activeEventId_;
    const DialogueDefinition* activeDefinition_ = nullptr;
    std::size_t pageIndex_ = 0;
    float fadeElapsed_ = 0.f;
};

} // namespace UI

#endif // __UI_DIALOGUE_SYSTEM_HPP
