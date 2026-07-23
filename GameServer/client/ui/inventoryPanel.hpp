#ifndef __INVENTORY_PANEL_HPP
#define __INVENTORY_PANEL_HPP

#include "../../common/inventory.hpp"
#include "protocol.hpp"
#include <functional>
#include <string>
#include <vector>

struct Texture;

namespace UI {

class Button;
class Image;
class Label;
class Panel;
class UIElement;
class UIManager;

class InventoryPanel {
public:
    struct Callbacks {
        std::function<void(std::uint8_t, InventoryAction)> onAction;
    };
    struct Style {
        const Texture* panelTexture = nullptr;
        const Texture* buttonTexture = nullptr;
        std::function<const Texture*(ItemId)> resolveItemIcon;
    };

    void build(UIManager& manager, const ItemCatalog& catalog, Callbacks callbacks);
    void build(UIManager& manager, const ItemCatalog& catalog,
               Callbacks callbacks, Style style);
    void update(float deltaTimeSec);

    void open();
    void close();
    void toggle();
    bool isOpen() const;

    void setInventory(const Inventory& inventory);
    void setPending(bool pending);
    void showActionResult(InventoryActionResult result);

    // Called before UIManager routes a mouse-down message. This closes a context
    // menu when the user clicks anywhere outside it.
    void handleGlobalPointerDown(float screenX, float screenY);

private:
    void onSlotHover(std::size_t slotIndex, bool hovered);
    void onSlotRightClick(std::size_t slotIndex);
    void invokeAction(InventoryAction action);
    void closeContextMenu();
    void hideTooltip();
    void positionBesideSlot(UIElement* popup, std::size_t slotIndex,
                            float popupWidth, float popupHeight);
    void setStatus(const std::wstring& text, float seconds);

    UIManager* manager_ = nullptr;
    const ItemCatalog* catalog_ = nullptr;
    Callbacks callbacks_{};
    Style style_{};

    Panel* modalBlocker_ = nullptr;
    Button* root_ = nullptr;
    std::vector<Button*> slotButtons_;
    std::vector<UIElement*> iconBlocks_;
    std::vector<Image*> iconImages_;
    std::vector<Label*> iconLabels_;
    std::vector<Label*> quantityLabels_;
    std::vector<ItemStack> displayedSlots_;

    Panel* tooltip_ = nullptr;
    Label* tooltipName_ = nullptr;
    Label* tooltipDescription_ = nullptr;

    Panel* contextMenu_ = nullptr;
    Button* useButton_ = nullptr;
    Button* discardButton_ = nullptr;
    Label* statusLabel_ = nullptr;

    std::size_t contextSlot_ = static_cast<std::size_t>(-1);
    bool pending_ = false;
    float statusSeconds_ = 0.f;
};

}  // namespace UI

#endif  // __INVENTORY_PANEL_HPP
