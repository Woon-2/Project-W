#include "pch.hpp"
#include "inventoryPanel.hpp"

#include "UIManager.hpp"
#include "uiBuild.hpp"
#include "../gfxUtil.hpp"
#include "widgets/Button.hpp"
#include "widgets/Image.hpp"
#include "widgets/Label.hpp"
#include "widgets/Panel.hpp"

namespace UI {
namespace {

constexpr int kColumns = 6;
constexpr float kSlotSize = 58.f;
constexpr float kSlotGap = 8.f;
constexpr float kPanelWidth = 436.f;
constexpr float kPanelHeight = 354.f;
constexpr float kGridX = 24.f;
constexpr float kGridY = 54.f;
constexpr float kTooltipWidth = 286.f;
constexpr float kTooltipHeight = 100.f;
constexpr float kContextWidth = 124.f;
constexpr float kContextHeight = 80.f;
constexpr std::size_t kNoSlot = static_cast<std::size_t>(-1);
constexpr const wchar_t* kInventoryFontFamily = L"넥슨Lv2고딕";

std::wstring utf8ToWide(std::string_view text) {
    constexpr char32_t replacement = 0xFFFD;
    std::wstring result;
    result.reserve(text.size());

    auto appendCodePoint = [&](char32_t codePoint) {
        if constexpr (sizeof(wchar_t) == 2) {
            if (codePoint <= 0xFFFF) {
                result.push_back(static_cast<wchar_t>(codePoint));
                return;
            }
            codePoint -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
            result.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
        } else {
            result.push_back(static_cast<wchar_t>(codePoint));
        }
    };

    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto first = static_cast<unsigned char>(text[offset]);
        char32_t codePoint = replacement;
        std::size_t length = 1;
        if (first < 0x80) codePoint = first;
        else if ((first & 0xE0) == 0xC0) { codePoint = first & 0x1F; length = 2; }
        else if ((first & 0xF0) == 0xE0) { codePoint = first & 0x0F; length = 3; }
        else if ((first & 0xF8) == 0xF0) { codePoint = first & 0x07; length = 4; }

        bool valid = length <= text.size() - offset;
        for (std::size_t i = 1; valid && i < length; ++i) {
            const auto continuation = static_cast<unsigned char>(text[offset + i]);
            valid = (continuation & 0xC0) == 0x80;
            if (valid) codePoint = (codePoint << 6) | (continuation & 0x3F);
        }
        const bool overlong =
            (length == 2 && codePoint < 0x80) ||
            (length == 3 && codePoint < 0x800) ||
            (length == 4 && codePoint < 0x10000);
        const bool invalidCodePoint =
            codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF);
        if (!valid || overlong || invalidCodePoint) {
            appendCodePoint(replacement);
            ++offset;
        } else {
            appendCodePoint(codePoint);
            offset += length;
        }
    }
    return result;
}

class InventorySlotButton final : public Button {
public:
    std::function<void(bool)> onHoverChanged;
    std::function<void()> onRightClick;

    void onMouseEnter() override {
        Button::onMouseEnter();
        if (onHoverChanged) onHoverChanged(true);
    }
    void onMouseLeave() override {
        Button::onMouseLeave();
        if (onHoverChanged) onHoverChanged(false);
    }
    void onMouseUp(MouseButton button, float localX, float localY) override {
        Button::onMouseUp(button, localX, localY);
        if (button == MouseButton::Right && onRightClick)
            onRightClick();
    }
};

Label* addText(UIElement* parent, const std::wstring& text,
               float x, float y, float w, float h, float size,
               TextHAlign align = TextHAlign::Leading) {
    auto* label = static_cast<Label*>(parent->addChild(std::make_unique<Label>()));
    Build::applyRect(label, Anchors::TopLeft, Pivots::TopLeft, x, y, w, h);
    label->setFontFamily(kInventoryFontFamily);
    label->setFontSize(size);
    label->setTextHAlign(align);
    label->setTextVAlign(TextVAlign::Center);
    label->setTextColor(0.94f, 0.96f, 1.f, 1.f);
    label->setText(text);
    return label;
}

void applyInventoryFont(Button* button) {
    if (!button || button->children().empty())
        return;

    static_cast<Label*>(button->children().front().get())
        ->setFontFamily(kInventoryFontFamily);
}

}  // namespace

void InventoryPanel::build(UIManager& manager, const ItemCatalog& catalog,
                           Callbacks callbacks) {
    build(manager, catalog, std::move(callbacks), Style{});
}

void InventoryPanel::build(UIManager& manager, const ItemCatalog& catalog,
                           Callbacks callbacks, Style style) {
    manager_ = &manager;
    catalog_ = &catalog;
    callbacks_ = std::move(callbacks);
    style_ = std::move(style);

    modalBlocker_ = static_cast<Panel*>(
        manager.root()->addChild(std::make_unique<Panel>()));
    modalBlocker_->name = "inventoryModalBlocker";
    modalBlocker_->anchor = Anchors::TopLeft;
    modalBlocker_->pivot = Pivots::TopLeft;
    modalBlocker_->drawSolidBackground = true;
    modalBlocker_->colorTint = { 0.01f, 0.016f, 0.025f, 0.34f };
    modalBlocker_->interactive = true;
    modalBlocker_->zOrder = 1000;
    modalBlocker_->visible = false;

    root_ = static_cast<Button*>(
        manager.root()->addChild(std::make_unique<Button>()));
    root_->name = "inventoryPanel";
    root_->anchor = Anchors::Center;
    root_->pivot = Pivots::Center;
    root_->width = DimValue::px(kPanelWidth);
    root_->height = DimValue::px(kPanelHeight);
    root_->bgColor = { 0.035f, 0.047f, 0.070f, 0.96f };
    root_->interactive = false;
    root_->zOrder = 1010;
    root_->visible = false;
    const bool texturedPanel = style_.panelTexture && style_.panelTexture->res;
    if (texturedPanel) {
        root_->texNormal = style_.panelTexture;
        root_->sliceUvBorderX = 0.30f;
        root_->sliceUvBorderY = 0.30f;
        root_->sliceCornerX = 40.f;
        root_->sliceCornerY = 40.f;
        root_->texTint = { 1.f, 1.f, 1.f, 0.97f };
        root_->texTintHovered = root_->texTint;
        root_->texTintPressed = root_->texTint;
    }

    auto* title = addText(root_, L"인벤토리", 24.f, 10.f, kPanelWidth - 48.f, 34.f,
                          24.f, TextHAlign::Center);
    title->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
    if (texturedPanel)
        title->setTextColor(0.07f, 0.12f, 0.18f, 1.f);

    slotButtons_.reserve(catalog.slotCount());
    iconBlocks_.reserve(catalog.slotCount());
    iconImages_.reserve(catalog.slotCount());
    iconLabels_.reserve(catalog.slotCount());
    quantityLabels_.reserve(catalog.slotCount());
    displayedSlots_.assign(catalog.slotCount(), ItemStack{});

    for (std::size_t i = 0; i < catalog.slotCount(); ++i) {
        const int row = static_cast<int>(i) / kColumns;
        const int column = static_cast<int>(i) % kColumns;
        const float x = kGridX + column * (kSlotSize + kSlotGap);
        const float y = kGridY + row * (kSlotSize + kSlotGap);

        auto* slot = static_cast<InventorySlotButton*>(
            root_->addChild(std::make_unique<InventorySlotButton>()));
        Build::applyRect(slot, Anchors::TopLeft, Pivots::TopLeft,
                         x, y, kSlotSize, kSlotSize);
        slot->name = "inventorySlot" + std::to_string(i);
        slot->bgColor = { 0.09f, 0.115f, 0.155f, 1.f };
        slot->bgColorHovered = { 0.16f, 0.23f, 0.32f, 1.f };
        slot->bgColorPressed = { 0.07f, 0.09f, 0.12f, 1.f };
        slot->zOrder = 1020;
        slot->onHoverChanged = [this, i](bool hovered) { onSlotHover(i, hovered); };
        slot->onRightClick = [this, i]() { onSlotRightClick(i); };
        slotButtons_.push_back(slot);

        auto* icon = Build::addSolid(slot, "inventoryIconPlaceholder",
                                     7.f, 7.f, 44.f, 44.f,
                                     { 0.55f, 0.12f, 0.18f, 1.f });
        icon->visible = false;
        iconBlocks_.push_back(icon);

        auto* iconImage = static_cast<Image*>(
            slot->addChild(std::make_unique<Image>()));
        Build::applyRect(iconImage, Anchors::TopLeft, Pivots::TopLeft,
                         7.f, 7.f, 44.f, 44.f);
        iconImage->zOrder = 1;
        iconImage->visible = false;
        iconImages_.push_back(iconImage);

        auto* iconLabel = addText(icon, L"HP", 0.f, 0.f, 44.f, 44.f,
                                  16.f, TextHAlign::Center);
        iconLabel->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
        iconLabels_.push_back(iconLabel);

        auto* quantity = addText(slot, L"", 5.f, 35.f, 48.f, 20.f,
                                 15.f, TextHAlign::Trailing);
        quantity->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
        quantity->zOrder = 2;
        quantityLabels_.push_back(quantity);
    }

    statusLabel_ = addText(root_, L"", 24.f, 318.f, kPanelWidth - 48.f, 26.f,
                           14.f, TextHAlign::Center);
    statusLabel_->setTextColor(1.0f, 0.0f, 0.0f, 1.f);

    tooltip_ = static_cast<Panel*>(
        manager.root()->addChild(std::make_unique<Panel>()));
    Build::applyRect(tooltip_, Anchors::TopLeft, Pivots::TopLeft,
                     0.f, 0.f, kTooltipWidth, kTooltipHeight);
    tooltip_->drawSolidBackground = true;
    tooltip_->colorTint = { 0.025f, 0.032f, 0.048f, 0.98f };
    tooltip_->zOrder = 1100;
    tooltip_->visible = false;
    tooltipName_ = addText(tooltip_, L"", 12.f, 8.f, kTooltipWidth - 24.f, 28.f, 18.f);
    tooltipName_->setFontWeight(DWRITE_FONT_WEIGHT_BOLD);
    tooltipDescription_ = addText(tooltip_, L"", 12.f, 38.f,
                                  kTooltipWidth - 24.f, 50.f, 14.f);
    tooltipDescription_->setTextVAlign(TextVAlign::Top);
    tooltipDescription_->setTextColor(0.74f, 0.79f, 0.88f, 1.f);

    contextMenu_ = static_cast<Panel*>(
        manager.root()->addChild(std::make_unique<Panel>()));
    Build::applyRect(contextMenu_, Anchors::TopLeft, Pivots::TopLeft,
                     0.f, 0.f, kContextWidth, kContextHeight);
    contextMenu_->drawSolidBackground = true;
    contextMenu_->colorTint = { 0.025f, 0.032f, 0.048f, 0.99f };
    contextMenu_->interactive = true;
    contextMenu_->zOrder = 1200;
    contextMenu_->visible = false;

    const XMFLOAT4 normal{ 0.12f, 0.16f, 0.22f, 1.f };
    const XMFLOAT4 hovered{ 0.22f, 0.32f, 0.44f, 1.f };
    const XMFLOAT4 pressed{ 0.08f, 0.11f, 0.16f, 1.f };
    const bool texturedButton = style_.buttonTexture && style_.buttonTexture->res;
    const XMFLOAT4 text = texturedButton
        ? XMFLOAT4{ 0.07f, 0.12f, 0.18f, 1.f }
        : XMFLOAT4{ 0.95f, 0.97f, 1.f, 1.f };
    useButton_ = Build::addButton(
        contextMenu_, L"사용", 6.f, 6.f, 112.f, 31.f,
        normal, hovered, pressed, 15.f,
        [this]() { invokeAction(InventoryAction::Use); }, text);
    discardButton_ = Build::addButton(
        contextMenu_, L"버리기", 6.f, 43.f, 112.f, 31.f,
        normal, hovered, pressed, 15.f,
        [this]() { invokeAction(InventoryAction::DiscardOne); }, text);
    applyInventoryFont(useButton_);
    applyInventoryFont(discardButton_);
    useButton_->zOrder = 1210;
    discardButton_->zOrder = 1210;
    if (texturedButton) {
        for (Button* button : { useButton_, discardButton_ }) {
            button->texNormal = style_.buttonTexture;
            button->sliceUvBorderX = 0.40f;
            button->sliceUvBorderY = 0.40f;
            button->sliceCornerX = 18.f;
            button->sliceCornerY = 18.f;
            button->texTint = { 1.f, 1.f, 1.f, 1.f };
            button->texTintHovered = { 0.90f, 1.02f, 1.08f, 1.f };
            button->texTintPressed = { 0.78f, 0.90f, 0.98f, 1.f };
        }
    }
}

void InventoryPanel::update(float deltaTimeSec) {
    if (statusSeconds_ <= 0.f)
        return;
    statusSeconds_ = std::max(0.f, statusSeconds_ - deltaTimeSec);
    if (statusSeconds_ == 0.f && statusLabel_)
        statusLabel_->setText(L"");
}

void InventoryPanel::open() {
    if (!root_) return;
    manager_->resetInteractionState();
    const float scale = std::max(0.01f, manager_->uiScale());
    modalBlocker_->offsetX = DimValue::px(manager_->screenToLayoutX(0.f));
    modalBlocker_->offsetY = DimValue::px(manager_->screenToLayoutY(0.f));
    modalBlocker_->width = DimValue::px(manager_->screenWidth() / scale);
    modalBlocker_->height = DimValue::px(manager_->screenHeight() / scale);
    modalBlocker_->visible = true;
    root_->visible = true;
    setStatus(L"", 0.f);
}

void InventoryPanel::close() {
    if (!root_) return;
    manager_->resetInteractionState();
    modalBlocker_->visible = false;
    root_->visible = false;
    closeContextMenu();
    hideTooltip();
    pending_ = false;
    setStatus(L"", 0.f);
}

void InventoryPanel::toggle() {
    isOpen() ? close() : open();
}

bool InventoryPanel::isOpen() const {
    return root_ && root_->visible;
}

void InventoryPanel::setInventory(const Inventory& inventory) {
    displayedSlots_ = inventory.slots();
    const std::size_t count = std::min(displayedSlots_.size(), slotButtons_.size());
    for (std::size_t i = 0; i < slotButtons_.size(); ++i) {
        const bool occupied = i < count && !displayedSlots_[i].empty();
        const Texture* itemIcon = nullptr;
        if (occupied && style_.resolveItemIcon)
            itemIcon = style_.resolveItemIcon(displayedSlots_[i].itemId);
        const bool hasLoadedIcon = itemIcon && itemIcon->res;
        iconImages_[i]->texture = hasLoadedIcon ? itemIcon : nullptr;
        iconImages_[i]->visible = hasLoadedIcon;
        iconBlocks_[i]->visible = occupied && !hasLoadedIcon;
        quantityLabels_[i]->setText(
            occupied ? std::to_wstring(displayedSlots_[i].quantity) : L"");
        slotButtons_[i]->bgColor = occupied
            ? XMFLOAT4{ 0.11f, 0.15f, 0.20f, 1.f }
            : XMFLOAT4{ 0.065f, 0.080f, 0.105f, 1.f };
    }

    if (contextSlot_ != kNoSlot
        && (contextSlot_ >= displayedSlots_.size() || displayedSlots_[contextSlot_].empty())) {
        closeContextMenu();
    }
}

void InventoryPanel::setPending(bool pending) {
    const bool wasPending = pending_;
    pending_ = pending;
    if (pending)
        setStatus(L"처리 중...", 0.f);
    else if (wasPending)
        setStatus(L"", 0.f);
}

void InventoryPanel::showActionResult(InventoryActionResult result) {
    pending_ = false;
    switch (result) {
    case InventoryActionResult::Success:
        setStatus(L"", 0.f);
        break;
    case InventoryActionResult::InvalidSlot:
        setStatus(L"잘못된 슬롯입니다.", 2.f);
        break;
    case InventoryActionResult::EmptySlot:
        setStatus(L"빈 슬롯입니다.", 2.f);
        break;
    case InventoryActionResult::UnknownItem:
        setStatus(L"알 수 없는 아이템입니다.", 2.f);
        break;
    case InventoryActionResult::NotUsable:
        setStatus(L"사용할 수 없는 아이템입니다.", 2.f);
        break;
    case InventoryActionResult::FullHealth:
        setStatus(L"체력이 가득 찼습니다.", 2.f);
        break;
    case InventoryActionResult::Dead:
        setStatus(L"사망 상태에서는 사용할 수 없습니다.", 2.f);
        break;
    case InventoryActionResult::StaleRevision:
        setStatus(L"인벤토리를 다시 동기화합니다.", 2.f);
        break;
    }
}

void InventoryPanel::handleGlobalPointerDown(float screenX, float screenY) {
    if (!isOpen())
        return;
    hideTooltip();
    if (contextMenu_ && contextMenu_->visible
        && !contextMenu_->resolvedRect().contains(screenX, screenY)) {
        closeContextMenu();
    }
}

void InventoryPanel::onSlotHover(std::size_t slotIndex, bool hovered) {
    if (!hovered || !isOpen() || contextMenu_->visible || slotIndex >= displayedSlots_.size()
        || displayedSlots_[slotIndex].empty()) {
        hideTooltip();
        return;
    }

    const ItemDefinition* definition = catalog_->find(displayedSlots_[slotIndex].itemId);
    if (!definition) {
        hideTooltip();
        return;
    }
    tooltipName_->setText(utf8ToWide(definition->name));
    tooltipDescription_->setText(utf8ToWide(definition->description));
    positionBesideSlot(tooltip_, slotIndex, kTooltipWidth, kTooltipHeight);
    tooltip_->visible = true;
}

void InventoryPanel::onSlotRightClick(std::size_t slotIndex) {
    hideTooltip();
    if (!isOpen() || pending_ || slotIndex >= displayedSlots_.size()
        || displayedSlots_[slotIndex].empty()) {
        closeContextMenu();
        return;
    }
    contextSlot_ = slotIndex;
    // Items with no use effect (gems) hide the "use" entry: the server would only
    // reject it with NotUsable, so leaving it in the menu makes the UX lie.
    const ItemDefinition* definition =
        catalog_ ? catalog_->find(displayedSlots_[slotIndex].itemId) : nullptr;
    const bool usable = definition && definition->useKind != ItemUseKind::None;
    if (useButton_) useButton_->visible = usable;
    if (discardButton_)
        discardButton_->offsetY = DimValue::px(usable ? 43.f : 6.f);
    positionBesideSlot(contextMenu_, slotIndex,
                       kContextWidth, usable ? kContextHeight : kContextHeight - 37.f);
    contextMenu_->height = DimValue::px(usable ? kContextHeight : kContextHeight - 37.f);
    contextMenu_->visible = true;
}

void InventoryPanel::invokeAction(InventoryAction action) {
    if (pending_ || contextSlot_ == kNoSlot || !callbacks_.onAction)
        return;
    const std::size_t slot = contextSlot_;
    closeContextMenu();
    hideTooltip();
    setPending(true);
    callbacks_.onAction(static_cast<std::uint8_t>(slot), action);
}

void InventoryPanel::closeContextMenu() {
    if (contextMenu_) contextMenu_->visible = false;
    contextSlot_ = kNoSlot;
}

void InventoryPanel::hideTooltip() {
    if (tooltip_) tooltip_->visible = false;
}

void InventoryPanel::positionBesideSlot(UIElement* popup, std::size_t slotIndex,
                                        float popupWidth, float popupHeight) {
    if (!popup || slotIndex >= slotButtons_.size())
        return;

    const Rect& slotRect = slotButtons_[slotIndex]->resolvedRect();
    float x = manager_->screenToLayoutX(slotRect.x + slotRect.width) + 10.f;
    float y = manager_->screenToLayoutY(slotRect.y);
    if (x + popupWidth > manager_->layoutWidth() - 8.f)
        x = manager_->screenToLayoutX(slotRect.x) - popupWidth - 10.f;
    x = std::clamp(x, 8.f, manager_->layoutWidth() - popupWidth - 8.f);
    y = std::clamp(y, 8.f, manager_->layoutHeight() - popupHeight - 8.f);
    popup->offsetX = DimValue::px(x);
    popup->offsetY = DimValue::px(y);
}

void InventoryPanel::setStatus(const std::wstring& text, float seconds) {
    if (statusLabel_) statusLabel_->setText(text);
    statusSeconds_ = seconds;
}

}  // namespace UI
