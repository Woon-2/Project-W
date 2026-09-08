#ifndef __UI_TACTICAL_DIALOGUE_OVERLAY_HPP
#define __UI_TACTICAL_DIALOGUE_OVERLAY_HPP

#include "../UIManager.hpp"
#include "../widgets/Image.hpp"
#include "../widgets/Label.hpp"
#include "../widgets/Panel.hpp"
#include "../../../ServerEngine/protocol.hpp"

class AssetManager;

namespace UI {

class TacticalDialogueOverlay {
public:
    void init(UIManager& uiManager, AssetManager& assetManager);
    bool trigger(TacticalDialogueId dialogueId);
    void update(float deltaTimeSec);

    bool active() const { return active_; }

private:
    void buildUI();
    void applyAlpha(float alpha);

    UIManager* uiManager_ = nullptr;
    AssetManager* assetManager_ = nullptr;

    Panel* root_ = nullptr;
    Image* banner_ = nullptr;
    Panel* accent_ = nullptr;
    Image* emblem_ = nullptr;
    Label* category_ = nullptr;
    Label* speaker_ = nullptr;
    Label* dialogue_ = nullptr;

    Color activeAccent_ = { 1.f, 1.f, 1.f, 1.f };

    float elapsed_ = 0.f;
    bool active_ = false;
    bool hasActiveEmblem_ = true;
};

} // namespace UI

#endif // __UI_TACTICAL_DIALOGUE_OVERLAY_HPP
