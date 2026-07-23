#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using ItemId = std::uint32_t;

enum class ItemUseKind : std::uint8_t {
    None = 0,
    HealPercent,
};

struct ItemDefinition {
    ItemId       id = 0;
    std::string  key;
    std::string  name;
    std::string  description;
    std::string  iconPath;
    std::uint16_t maxStack = 1;
    ItemUseKind  useKind = ItemUseKind::None;
    float        useValue = 0.f;
};

struct ItemStack {
    ItemId        itemId = 0;
    std::uint16_t quantity = 0;

    bool empty() const { return itemId == 0 || quantity == 0; }

    friend bool operator==(const ItemStack&, const ItemStack&) = default;
};

struct StarterItem {
    std::uint8_t  slot = 0;
    ItemId        itemId = 0;
    std::uint16_t quantity = 0;
};

class ItemCatalog {
public:
    bool load(const std::filesystem::path& path, std::string& error);

    std::uint8_t slotCount() const { return slotCount_; }
    const ItemDefinition* find(ItemId id) const;
    const std::vector<ItemDefinition>& items() const { return items_; }
    const std::vector<StarterItem>& starterItems() const { return starterItems_; }

private:
    std::uint8_t slotCount_ = 0;
    std::vector<ItemDefinition> items_;
    std::vector<StarterItem> starterItems_;
    std::unordered_map<ItemId, std::size_t> indexById_;
};

class Inventory {
public:
    bool initialize(const ItemCatalog& catalog, std::string* error = nullptr);
    bool initializeEmpty(const ItemCatalog& catalog, std::string* error = nullptr);

    std::size_t slotCount() const { return slots_.size(); }
    std::uint32_t revision() const { return revision_; }
    const std::vector<ItemStack>& slots() const { return slots_; }
    const ItemStack* slot(std::size_t index) const;

    // Fills existing stacks from the lowest slot first, then empty slots.
    // Returns the amount that did not fit.
    std::uint32_t add(const ItemCatalog& catalog, ItemId itemId, std::uint32_t quantity);
    bool removeOne(std::size_t slotIndex);

    // Client mirror operations. These never increment the supplied authoritative
    // revision and reject malformed slot data.
    bool applySnapshot(const ItemCatalog& catalog, std::uint32_t revision,
                       const std::vector<ItemStack>& slots, std::string* error = nullptr);
    bool applyAuthoritativeSlot(const ItemCatalog& catalog, std::uint32_t revision,
                                std::size_t slotIndex, ItemStack stack,
                                std::string* error = nullptr);

private:
    static bool validateStack(const ItemCatalog& catalog, const ItemStack& stack,
                              std::string* error);

    std::vector<ItemStack> slots_;
    std::uint32_t revision_ = 0;
};

// Domain-level action types shared by the authoritative RoomServer and the
// synchronous standalone adapter. Protocol enums are deliberately kept out of
// this layer so the inventory model stays transport-independent.
enum class InventoryCommand : std::uint8_t {
    Use = 0,
    DiscardOne,
};

enum class InventoryCommandResult : std::uint8_t {
    Success = 0,
    InvalidSlot,
    EmptySlot,
    UnknownItem,
    NotUsable,
    FullHealth,
    Dead,
};

struct InventoryCommandOutcome {
    InventoryCommandResult result = InventoryCommandResult::NotUsable;
    std::int32_t resultingHp = 0;
};

// Executes one validated domain action. Failed actions never change HP,
// quantity, or revision. A successful heal consumes exactly one item.
InventoryCommandOutcome executeInventoryCommand(
    Inventory& inventory, const ItemCatalog& catalog, std::size_t slotIndex,
    InventoryCommand command, std::int32_t currentHp, std::int32_t maxHp);
