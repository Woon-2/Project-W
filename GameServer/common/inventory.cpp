#include "inventory.hpp"

#include "simpleJson.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace {

bool readFile(const std::filesystem::path& path, std::string& out, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open inventory config: " + path.string();
        return false;
    }

    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

bool readUnsigned(const json::Value* value, std::uint64_t maxValue,
                  std::uint64_t& out, std::string_view field, std::string& error) {
    if (!value || !value->isNumber()) {
        error = std::string(field) + " must be an unsigned integer";
        return false;
    }

    const double number = value->asNumber(-1.0);
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number
        || number > static_cast<double>(maxValue)) {
        error = std::string(field) + " is outside its valid integer range";
        return false;
    }

    out = static_cast<std::uint64_t>(number);
    return true;
}

bool readRequiredString(const json::Value& object, std::string_view field,
                        std::string& out, std::string& error) {
    const json::Value* value = object.find(field);
    if (!value || !value->isString() || value->asString().empty()) {
        error = std::string(field) + " must be a non-empty string";
        return false;
    }
    out = value->asString();
    return true;
}

std::string indexedField(std::string_view arrayName, std::size_t index,
                         std::string_view field) {
    std::ostringstream oss;
    oss << arrayName << '[' << index << "]." << field;
    return oss.str();
}

}  // namespace

bool ItemCatalog::load(const std::filesystem::path& path, std::string& error) {
    std::string text;
    if (!readFile(path, text, error))
        return false;

    json::Value root;
    std::string parseError;
    if (!json::parse(text, root, &parseError)) {
        error = "invalid inventory JSON: " + parseError;
        return false;
    }
    if (!root.isObject()) {
        error = "inventory JSON root must be an object";
        return false;
    }

    std::uint64_t slotCount = 0;
    if (!readUnsigned(root.find("slotCount"), std::numeric_limits<std::uint8_t>::max(),
                      slotCount, "slotCount", error)
        || slotCount == 0) {
        if (error.empty()) error = "slotCount must be greater than zero";
        return false;
    }

    const json::Value* itemsValue = root.find("items");
    if (!itemsValue || !itemsValue->isArray() || itemsValue->asArray().empty()) {
        error = "items must be a non-empty array";
        return false;
    }

    std::vector<ItemDefinition> items;
    std::unordered_map<ItemId, std::size_t> indexById;
    std::unordered_set<std::string> keys;
    items.reserve(itemsValue->asArray().size());

    for (std::size_t i = 0; i < itemsValue->asArray().size(); ++i) {
        const json::Value& value = itemsValue->asArray()[i];
        if (!value.isObject()) {
            error = indexedField("items", i, "") + " must be an object";
            return false;
        }

        ItemDefinition definition;
        std::uint64_t id = 0;
        const std::string idField = indexedField("items", i, "id");
        if (!readUnsigned(value.find("id"), std::numeric_limits<ItemId>::max(),
                          id, idField, error)
            || id == 0) {
            if (id == 0) error = idField + " must be non-zero";
            return false;
        }
        definition.id = static_cast<ItemId>(id);

        if (!readRequiredString(value, "key", definition.key, error)
            || !readRequiredString(value, "name", definition.name, error)
            || !readRequiredString(value, "description", definition.description, error)) {
            error = "items[" + std::to_string(i) + "]: " + error;
            return false;
        }

        if (const json::Value* icon = value.find("iconPath"); icon) {
            if (!icon->isString()) {
                error = indexedField("items", i, "iconPath") + " must be a string";
                return false;
            }
            definition.iconPath = icon->asString();
        }

        std::uint64_t maxStack = 0;
        const std::string maxStackField = indexedField("items", i, "maxStack");
        if (!readUnsigned(value.find("maxStack"), std::numeric_limits<std::uint16_t>::max(),
                          maxStack, maxStackField, error)
            || maxStack == 0) {
            if (maxStack == 0) error = maxStackField + " must be greater than zero";
            return false;
        }
        definition.maxStack = static_cast<std::uint16_t>(maxStack);

        if (const json::Value* use = value.find("use"); use) {
            if (!use->isObject()) {
                error = indexedField("items", i, "use") + " must be an object";
                return false;
            }
            std::string type;
            if (!readRequiredString(*use, "type", type, error)) {
                error = indexedField("items", i, "use.") + error;
                return false;
            }

            if (type == "heal_percent") {
                const json::Value* amount = use->find("value");
                if (!amount || !amount->isNumber()) {
                    error = indexedField("items", i, "use.value") + " must be a number";
                    return false;
                }
                const double fraction = amount->asNumber(-1.0);
                if (!std::isfinite(fraction) || fraction <= 0.0 || fraction > 1.0) {
                    error = indexedField("items", i, "use.value")
                        + " must be in the range (0, 1]";
                    return false;
                }
                definition.useKind = ItemUseKind::HealPercent;
                definition.useValue = static_cast<float>(fraction);
            } else if (type == "none") {
                definition.useKind = ItemUseKind::None;
            } else {
                error = indexedField("items", i, "use.type") + " is unknown";
                return false;
            }
        }

        if (!indexById.emplace(definition.id, items.size()).second) {
            error = idField + " is duplicated";
            return false;
        }
        if (!keys.emplace(definition.key).second) {
            error = indexedField("items", i, "key") + " is duplicated";
            return false;
        }
        items.push_back(std::move(definition));
    }

    const json::Value* starterValue = root.find("starter");
    if (!starterValue || !starterValue->isArray()) {
        error = "starter must be an array";
        return false;
    }

    std::vector<StarterItem> starterItems;
    std::unordered_set<std::uint8_t> starterSlots;
    starterItems.reserve(starterValue->asArray().size());

    for (std::size_t i = 0; i < starterValue->asArray().size(); ++i) {
        const json::Value& value = starterValue->asArray()[i];
        if (!value.isObject()) {
            error = indexedField("starter", i, "") + " must be an object";
            return false;
        }

        std::uint64_t slot = 0;
        std::uint64_t itemId = 0;
        std::uint64_t quantity = 0;
        if (!readUnsigned(value.find("slot"), slotCount - 1, slot,
                          indexedField("starter", i, "slot"), error)
            || !readUnsigned(value.find("itemId"), std::numeric_limits<ItemId>::max(),
                             itemId, indexedField("starter", i, "itemId"), error)
            || !readUnsigned(value.find("quantity"),
                             std::numeric_limits<std::uint16_t>::max(),
                             quantity, indexedField("starter", i, "quantity"), error)) {
            return false;
        }

        const auto definitionIt = indexById.find(static_cast<ItemId>(itemId));
        if (itemId == 0 || definitionIt == indexById.end()) {
            error = indexedField("starter", i, "itemId") + " is not a known item";
            return false;
        }
        if (quantity == 0 || quantity > items[definitionIt->second].maxStack) {
            error = indexedField("starter", i, "quantity")
                + " must be within the item's stack limit";
            return false;
        }
        if (!starterSlots.emplace(static_cast<std::uint8_t>(slot)).second) {
            error = indexedField("starter", i, "slot") + " is duplicated";
            return false;
        }

        starterItems.push_back(StarterItem{
            .slot = static_cast<std::uint8_t>(slot),
            .itemId = static_cast<ItemId>(itemId),
            .quantity = static_cast<std::uint16_t>(quantity),
        });
    }

    slotCount_ = static_cast<std::uint8_t>(slotCount);
    items_ = std::move(items);
    starterItems_ = std::move(starterItems);
    indexById_ = std::move(indexById);
    error.clear();
    return true;
}

const ItemDefinition* ItemCatalog::find(ItemId id) const {
    const auto it = indexById_.find(id);
    return (it == indexById_.end()) ? nullptr : &items_[it->second];
}

bool Inventory::initialize(const ItemCatalog& catalog, std::string* error) {
    if (!initializeEmpty(catalog, error))
        return false;
    for (const StarterItem& starter : catalog.starterItems()) {
        if (starter.slot >= slots_.size()) {
            if (error) *error = "starter slot is outside the inventory";
            return false;
        }
        const ItemDefinition* definition = catalog.find(starter.itemId);
        if (!definition || starter.quantity == 0 || starter.quantity > definition->maxStack) {
            if (error) *error = "starter item violates the catalog";
            return false;
        }
        slots_[starter.slot] = ItemStack{ starter.itemId, starter.quantity };
    }

    revision_ = 1;
    if (error) error->clear();
    return true;
}

bool Inventory::initializeEmpty(const ItemCatalog& catalog, std::string* error) {
    if (catalog.slotCount() == 0) {
        if (error) *error = "catalog has no inventory slots";
        return false;
    }

    slots_.assign(catalog.slotCount(), ItemStack{});
    revision_ = 0;
    if (error) error->clear();
    return true;
}

const ItemStack* Inventory::slot(std::size_t index) const {
    return (index < slots_.size()) ? &slots_[index] : nullptr;
}

std::uint32_t Inventory::add(const ItemCatalog& catalog, ItemId itemId,
                             std::uint32_t quantity) {
    const ItemDefinition* definition = catalog.find(itemId);
    if (!definition || quantity == 0 || slots_.empty())
        return quantity;

    const std::uint32_t original = quantity;
    for (ItemStack& stack : slots_) {
        if (stack.itemId != itemId || stack.quantity >= definition->maxStack)
            continue;
        const std::uint32_t room = definition->maxStack - stack.quantity;
        const std::uint32_t moved = std::min(room, quantity);
        stack.quantity = static_cast<std::uint16_t>(stack.quantity + moved);
        quantity -= moved;
        if (quantity == 0) break;
    }

    for (ItemStack& stack : slots_) {
        if (quantity == 0) break;
        if (!stack.empty()) continue;
        const std::uint32_t moved = std::min<std::uint32_t>(definition->maxStack, quantity);
        stack = ItemStack{ itemId, static_cast<std::uint16_t>(moved) };
        quantity -= moved;
    }

    if (quantity != original)
        ++revision_;
    return quantity;
}

bool Inventory::canFit(const ItemCatalog& catalog, ItemId itemId,
                       std::uint32_t quantity) const {
    if (quantity == 0)
        return true;

    const ItemDefinition* definition = catalog.find(itemId);
    if (!definition || slots_.empty())
        return false;

    std::uint32_t remaining = quantity;
    for (const ItemStack& stack : slots_) {
        std::uint32_t room = 0;
        if (stack.empty())
            room = definition->maxStack;
        else if (stack.itemId == itemId && stack.quantity < definition->maxStack)
            room = definition->maxStack - stack.quantity;

        if (room >= remaining)
            return true;
        remaining -= room;
    }
    return false;
}

bool Inventory::removeOne(std::size_t slotIndex) {
    if (slotIndex >= slots_.size() || slots_[slotIndex].empty())
        return false;

    ItemStack& stack = slots_[slotIndex];
    --stack.quantity;
    if (stack.quantity == 0)
        stack = ItemStack{};
    ++revision_;
    return true;
}

bool Inventory::validateStack(const ItemCatalog& catalog, const ItemStack& stack,
                              std::string* error) {
    if (stack.itemId == 0 && stack.quantity == 0)
        return true;
    if (stack.itemId == 0 || stack.quantity == 0) {
        if (error) *error = "empty stack fields must both be zero";
        return false;
    }

    const ItemDefinition* definition = catalog.find(stack.itemId);
    if (!definition) {
        if (error) *error = "stack references an unknown item";
        return false;
    }
    if (stack.quantity > definition->maxStack) {
        if (error) *error = "stack quantity exceeds maxStack";
        return false;
    }
    return true;
}

bool Inventory::applySnapshot(const ItemCatalog& catalog, std::uint32_t revision,
                              const std::vector<ItemStack>& slots, std::string* error) {
    if (slots.size() != catalog.slotCount()) {
        if (error) *error = "snapshot slot count does not match the catalog";
        return false;
    }
    for (const ItemStack& stack : slots) {
        if (!validateStack(catalog, stack, error))
            return false;
    }
    if (revision < revision_) {
        if (error) *error = "snapshot revision is older than the local revision";
        return false;
    }

    slots_ = slots;
    revision_ = revision;
    if (error) error->clear();
    return true;
}

bool Inventory::applyAuthoritativeSlot(const ItemCatalog& catalog, std::uint32_t revision,
                                       std::size_t slotIndex, ItemStack stack,
                                       std::string* error) {
    if (slotIndex >= slots_.size()) {
        if (error) *error = "authoritative slot is outside the inventory";
        return false;
    }
    if (revision < revision_) {
        if (error) *error = "authoritative slot revision is older than the local revision";
        return false;
    }
    if (!validateStack(catalog, stack, error))
        return false;

    slots_[slotIndex] = stack;
    revision_ = revision;
    if (error) error->clear();
    return true;
}

InventoryCommandOutcome executeInventoryCommand(
    Inventory& inventory, const ItemCatalog& catalog, std::size_t slotIndex,
    InventoryCommand command, std::int32_t currentHp, std::int32_t maxHp) {
    auto outcome = [&](InventoryCommandResult result, std::int32_t hp) {
        return InventoryCommandOutcome{ result, hp };
    };

    const ItemStack* stack = inventory.slot(slotIndex);
    if (!stack)
        return outcome(InventoryCommandResult::InvalidSlot, currentHp);
    if (stack->empty())
        return outcome(InventoryCommandResult::EmptySlot, currentHp);

    const ItemDefinition* definition = catalog.find(stack->itemId);
    if (!definition)
        return outcome(InventoryCommandResult::UnknownItem, currentHp);

    if (command == InventoryCommand::DiscardOne) {
        if (!inventory.removeOne(slotIndex))
            return outcome(InventoryCommandResult::EmptySlot, currentHp);
        return outcome(InventoryCommandResult::Success, currentHp);
    }

    if (command != InventoryCommand::Use
        || definition->useKind != ItemUseKind::HealPercent) {
        return outcome(InventoryCommandResult::NotUsable, currentHp);
    }
    if (currentHp <= 0)
        return outcome(InventoryCommandResult::Dead, currentHp);

    const std::int32_t effectiveMaxHp = std::max<std::int32_t>(1, maxHp);
    if (currentHp >= effectiveMaxHp)
        return outcome(InventoryCommandResult::FullHealth, currentHp);

    const std::int32_t healAmount = std::max<std::int32_t>(
        1, static_cast<std::int32_t>(
            std::ceil(effectiveMaxHp * definition->useValue)));
    const std::int32_t healedHp = std::min(effectiveMaxHp, currentHp + healAmount);
    if (!inventory.removeOne(slotIndex))
        return outcome(InventoryCommandResult::EmptySlot, currentHp);

    return outcome(InventoryCommandResult::Success, healedHp);
}
