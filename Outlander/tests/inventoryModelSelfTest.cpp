#include "inventory.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void expectCatalogRejected(std::string_view json) {
    const std::filesystem::path path = "inventory-invalid-self-test.json";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output);
        output << json;
    }

    ItemCatalog catalog;
    std::string error;
    assert(!catalog.load(path, error));
    assert(!error.empty());
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    ItemCatalog catalog;
    std::string error;
    assert(catalog.load("resources/data/inventory.json", error));
    assert(catalog.slotCount() == 24);

    const ItemDefinition* potion = catalog.find(1);
    assert(potion);
    assert(potion->key == "ui_potion_0");
    assert(potion->iconPath == "../resources/UI/ui_potion_0.dds");
    assert(potion->maxStack == 20);
    assert(potion->useKind == ItemUseKind::HealPercent);
    assert(std::fabs(potion->useValue - 0.2f) < 0.0001f);

    Inventory inventory;
    assert(inventory.initialize(catalog, &error));
    assert(inventory.revision() == 1);
    const ItemStack fivePotions{ 1, 5 };
    const ItemStack fullPotionStack{ 1, 20 };
    const ItemStack onePotion{ 1, 1 };
    const ItemStack threePotions{ 1, 3 };
    const ItemStack fourPotions{ 1, 4 };
    assert(inventory.slot(0) && *inventory.slot(0) == fivePotions);

    const std::uint32_t initialRevision = inventory.revision();
    assert(inventory.add(catalog, 999, 3) == 3);
    assert(inventory.add(catalog, 1, 0) == 0);
    assert(inventory.revision() == initialRevision);

    assert(inventory.add(catalog, 1, 15) == 0);
    assert(*inventory.slot(0) == fullPotionStack);
    assert(inventory.revision() == initialRevision + 1);

    assert(inventory.add(catalog, 1, 21) == 0);
    assert(*inventory.slot(0) == fullPotionStack);
    assert(*inventory.slot(1) == fullPotionStack);
    assert(*inventory.slot(2) == onePotion);

    // Existing stacks must be filled before an earlier empty slot is used.
    Inventory mergeFirst;
    assert(mergeFirst.initializeEmpty(catalog, &error));
    std::vector<ItemStack> mergeSeed(catalog.slotCount());
    mergeSeed[1] = ItemStack{ 1, 19 };
    assert(mergeFirst.applySnapshot(catalog, 7, mergeSeed, &error));
    assert(mergeFirst.add(catalog, 1, 2) == 0);
    assert(*mergeFirst.slot(0) == onePotion);
    assert(*mergeFirst.slot(1) == fullPotionStack);
    assert(mergeFirst.revision() == 8);

    // A full 24-slot inventory reports the exact amount that did not fit.
    Inventory fullInventory;
    assert(fullInventory.initializeEmpty(catalog, &error));
    assert(fullInventory.add(catalog, 1, 481) == 1);
    for (const ItemStack& stack : fullInventory.slots())
        assert(stack == fullPotionStack);
    const std::uint32_t fullRevision = fullInventory.revision();
    assert(fullInventory.add(catalog, 1, 1) == 1);
    assert(fullInventory.revision() == fullRevision);

    const std::uint32_t beforeRemove = inventory.revision();
    assert(inventory.removeOne(2));
    assert(inventory.slot(2)->empty());
    assert(inventory.revision() == beforeRemove + 1);
    assert(!inventory.removeOne(2));
    assert(!inventory.removeOne(99));
    assert(inventory.revision() == beforeRemove + 1);

    std::vector<ItemStack> malformed(catalog.slotCount());
    malformed[0] = ItemStack{ 999, 1 };
    assert(!inventory.applySnapshot(catalog, inventory.revision() + 1, malformed, &error));
    malformed[0] = ItemStack{ 1, 21 };
    assert(!inventory.applySnapshot(catalog, inventory.revision() + 1, malformed, &error));
    malformed[0] = ItemStack{ 0, 1 };
    assert(!inventory.applySnapshot(catalog, inventory.revision() + 1, malformed, &error));
    malformed[0] = ItemStack{ 1, 0 };
    assert(!inventory.applySnapshot(catalog, inventory.revision() + 1, malformed, &error));

    Inventory mirror;
    assert(mirror.initializeEmpty(catalog, &error));
    assert(mirror.revision() == 0);
    assert(mirror.slotCount() == 24);
    for (const ItemStack& stack : mirror.slots())
        assert(stack.empty());
    assert(mirror.applySnapshot(catalog, 1, inventory.slots(), &error));
    assert(mirror.revision() == 1);
    assert(!mirror.applyAuthoritativeSlot(
        catalog, 2, catalog.slotCount(), ItemStack{ 1, 1 }, &error));
    assert(!mirror.applyAuthoritativeSlot(
        catalog, 2, 0, ItemStack{ 999, 1 }, &error));
    assert(mirror.applyAuthoritativeSlot(
        catalog, 2, 0, ItemStack{ 1, 3 }, &error));
    assert(mirror.revision() == 2);
    assert(!mirror.applyAuthoritativeSlot(
        catalog, 1, 0, ItemStack{ 1, 2 }, &error));
    assert(*mirror.slot(0) == threePotions);

    // RoomServer and standalone both execute this exact domain action path.
    Inventory actionInventory;
    assert(actionInventory.initialize(catalog, &error));
    const std::uint32_t actionInitialRevision = actionInventory.revision();

    InventoryCommandOutcome outcome = executeInventoryCommand(
        actionInventory, catalog, 0, InventoryCommand::Use, 100, 100);
    assert(outcome.result == InventoryCommandResult::FullHealth);
    assert(outcome.resultingHp == 100);
    assert(*actionInventory.slot(0) == fivePotions);
    assert(actionInventory.revision() == actionInitialRevision);

    outcome = executeInventoryCommand(
        actionInventory, catalog, 0, InventoryCommand::Use, 0, 100);
    assert(outcome.result == InventoryCommandResult::Dead);
    assert(outcome.resultingHp == 0);
    assert(*actionInventory.slot(0) == fivePotions);
    assert(actionInventory.revision() == actionInitialRevision);

    outcome = executeInventoryCommand(
        actionInventory, catalog, 24, InventoryCommand::Use, 50, 100);
    assert(outcome.result == InventoryCommandResult::InvalidSlot);
    assert(*actionInventory.slot(0) == fivePotions);
    assert(actionInventory.revision() == actionInitialRevision);

    outcome = executeInventoryCommand(
        actionInventory, catalog, 0,
        static_cast<InventoryCommand>(255), 50, 100);
    assert(outcome.result == InventoryCommandResult::NotUsable);
    assert(*actionInventory.slot(0) == fivePotions);
    assert(actionInventory.revision() == actionInitialRevision);

    outcome = executeInventoryCommand(
        actionInventory, catalog, 0, InventoryCommand::Use, 50, 100);
    assert(outcome.result == InventoryCommandResult::Success);
    assert(outcome.resultingHp == 70);
    assert(*actionInventory.slot(0) == fourPotions);
    assert(actionInventory.revision() == actionInitialRevision + 1);

    outcome = executeInventoryCommand(
        actionInventory, catalog, 0, InventoryCommand::DiscardOne, 70, 100);
    assert(outcome.result == InventoryCommandResult::Success);
    assert(outcome.resultingHp == 70);
    assert(*actionInventory.slot(0) == threePotions);
    assert(actionInventory.revision() == actionInitialRevision + 2);

    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{"id":0,"key":"bad","name":"bad","description":"bad","maxStack":20}],
        "starter": []
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [
            {"id":1,"key":"a","name":"a","description":"a","maxStack":20},
            {"id":1,"key":"b","name":"b","description":"b","maxStack":20}
        ],
        "starter": []
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{"id":1,"key":"a","name":"a","description":"a","maxStack":20}],
        "starter": [{"slot":24,"itemId":1,"quantity":1}]
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{"id":1,"key":"a","name":"a","description":"a","maxStack":20}],
        "starter": [{"slot":0,"itemId":1,"quantity":21}]
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{
            "id":1,"key":"a","name":"a","description":"a","maxStack":20,
            "use":{"type":"heal_percent","value":0}
        }],
        "starter": []
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{
            "id":1,"key":"a","name":"a","description":"a","maxStack":20,
            "use":{"type":"heal_percent","value":1.01}
        }],
        "starter": []
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{"id":1,"key":"a","name":"a","description":"a","maxStack":20}],
        "starter": [{"slot":0,"itemId":2,"quantity":1}]
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{"id":1,"key":"a","name":"a","description":"a","maxStack":20}],
        "starter": [{"slot":0,"itemId":1,"quantity":0}]
    })");
    expectCatalogRejected(R"({
        "slotCount": 24,
        "items": [{
            "id":1,"key":"a","name":"a","description":"a","iconPath":7,"maxStack":20
        }],
        "starter": []
    })");

    std::cout << "inventory model self-test passed\n";
    return 0;
}
