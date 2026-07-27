# LuaUtils

## ItemManager additions (vanilla global, extended)

Queries:

| Function | Returns |
| --- | --- |
| `ItemManager.GetItemEx(itemId)` | `{ id, className, type, amount, health, condition, quality, maxQuality, isEquipped, owner, entity }` |
| `ItemManager.IsItemEquipped(itemId)` | `bool` — per-instance equipped flag (C_Item flags bit 0) |
| `ItemManager.GetItemCondition(itemId)` | `float 0..1` — effective durability (runtime-data aware; raw `health` is wrong for equippables) |
| `ItemManager.GetItemQuality(itemId)` / `GetItemMaxQuality(itemId)` | `int` |
| `ItemManager.GetItemPrices(itemId)` | `{ unit, stack, newUnit, newStack }` |

Setters (return `true` on success, `nil` otherwise):

| Function | Notes |
| --- | --- |
| `ItemManager.SetItemHealth(itemId, health01)` | engine `SetItemHealth` (clamped 0–1, full notify) |
| `ItemManager.SetItemCondition(itemId, condition01)` | runtime-data condition for equippables, health for the rest |
| `ItemManager.SetItemQuality(itemId, quality)` | equippable-type items only |
| `ItemManager.SetItemAmount(itemId, amount)` | listener-correct amount change; `0` deletes the stack |
| `ItemManager.SetItemOwner(itemId, ownerId[, contextId])` | sanctioned owner write + owner-index update |
| `ItemManager.WashItem(itemId[, maxEffect])` | per-item dirt (default full wash) |
| `ItemManager.SetItemPhaseId(itemId, phaseId)` | food/torch phase |
| `ItemManager.SetItemPhase(itemId, phase01)` / `AdvanceItemPhase(itemId, amount01)` | phase progress |
| `ItemManager.MoveItem(itemId, dstInventoryId[, count])` | engine move core (split/merge aware); returns the surviving itemId; `count` 0/absent = whole stack |

## EquipmentManager (new global, entity-keyed)

| Function | Returns |
| --- | --- |
| `EquipmentManager.GetEquippedItems(entityId)` | array of equipped itemIds |
| `EquipmentManager.GetEquippedClothing(entityId)` | `{ [equipmentSlotId] = itemId }` |
| `EquipmentManager.GetHandSlots(entityId)` | `{ [1..8] = itemId }` (weapon/hand slots incl. secondary set; empty slots absent) |
| `EquipmentManager.GetItemInSlot(entityId, slotId)` | itemId in a clothing slot |
| `EquipmentManager.GetEquipWeights(entityId)` | `{ total, worn }` |
| `EquipmentManager.GetInventoryEx(entityId)` | array of `GetItemEx`-shaped tables — one call answers "what does this NPC carry and wear" |
| `EquipmentManager.GetInventoryId(entityId)` | the entity's inventory id (usable with `MoveItem`) |
| `EquipmentManager.SetItemEquipped(entityId, itemId, equip)` | equip/unequip through the soul (visuals + slot bookkeeping intact) |

## Examples

```lua
-- which of the bandit's two mailles is he wearing?
local bandit = System.GetEntityByName("bandit_01")
for _, it in ipairs(EquipmentManager.GetInventoryEx(bandit.id) or {}) do
    if it.className == "maille_long" then
        System.LogAlways(tostring(it.id) .. " equipped=" .. tostring(it.isEquipped))
    end
end

-- what helmet is the merchant actually wearing (vs just selling)?
local worn = EquipmentManager.GetEquippedClothing(merchant.id)

-- repair + clean the player's sword
local sword = player.human:GetItemInHand(0)
ItemManager.SetItemHealth(sword, 1.0)
ItemManager.WashItem(sword)
```

## Caveats

- Distant/unstreamed NPCs may not have their worn items materialized as real item
  instances yet (engine-side lazy materialization) — queries then return less than the
  NPC conceptually owns. Interact/get close first.
- Direct stat writes fire the engine's own notify paths, but some UI screens only
  refresh on reopen.
- `SetItemQuality` on non-equippable types is a no-op (`nil`).

## Build

Part of the KCD2 RE workspace: configure/build the `release` preset in `KCD2/RE/.buildenv`;
the DLL lands in `build-release/LuaUtils/LuaUtils.dll`. Deploy like any KCSE plugin.
