# LuaUtils

KCSE plugin for KCD2 that extends the vanilla `ItemManager` Lua table and publishes new `EquipmentManager` and `AudioManager` globals.

All functions use **dot-call syntax**:

```lua
ItemManager.GetItemEx(itemId)
EquipmentManager.GetEquippedClothing(entityId)
```

Do not call these functions with `:`. Item, inventory, and owner IDs are passed as Lua `ScriptHandle` values so the full 64-bit Warhorse WUID is preserved.

## ItemManager additions

### Queries

| Function | Returns |
| --- | --- |
| `ItemManager.GetItemEx(itemId)` | `{ id, className, type, amount, health, condition, quality, maxQuality, isEquipped, owner, entity }` |
| `ItemManager.IsItemEquipped(itemId)` | `bool` — per-instance equipped flag (`C_Item` flags bit 0) |
| `ItemManager.GetItemCondition(itemId)` | `float 0..1` — effective durability; runtime-data aware for equippables |
| `ItemManager.GetItemQuality(itemId)` | Current integer quality |
| `ItemManager.GetItemMaxQuality(itemId)` | Maximum integer quality |
| `ItemManager.GetItemPrices(itemId)` | `{ unit, stack, newUnit, newStack }`; current and new-condition unit/stack prices |

### Mutators

Mutators return `true` on success and `nil` if resolution or the native operation fails, except `MoveItem`, which returns the surviving item WUID.

| Function | Notes |
| --- | --- |
| `ItemManager.SetItemHealth(itemId, health)` | Native `SetItemHealth`, clamped to `0..1`, with the engine notify path |
| `ItemManager.SetItemCondition(itemId, condition)` | For equippables, maps normalized condition into the current quality tier's raw-health range; for other item types, writes health directly |
| `ItemManager.SetItemQuality(itemId, quality)` | Equippable-type items only |
| `ItemManager.SetItemAmount(itemId, amount)` | Listener-correct amount change; `amount <= 0` deletes a parented stack |
| `ItemManager.SetItemOwner(itemId, ownerId[, stolenFromOwnerId])` | Native owner write and owner-index update; omitted theft-source ID defaults to `ownerId` |
| `ItemManager.WashItem(itemId[, maxEffect])` | Per-item dirt operation; omitted effect performs a full wash |
| `ItemManager.SetItemPhaseId(itemId, phaseId)` | Set the discrete food/torch phase |
| `ItemManager.SetItemPhase(itemId, phase)` | Set normalized phase progress |
| `ItemManager.AdvanceItemPhase(itemId, amount)` | Advance normalized phase progress |
| `ItemManager.MoveItem(itemId, destinationInventoryId[, count])` | Split/merge-aware native move; `count` absent or `0` means the whole stack |

`ownerId` and `stolenFromOwnerId` are WUIDs, not CryEngine entity IDs. Theft state is not reducible to a permanent `ownerId != stolenFromOwnerId` test: the native system also records when the owner mark was set and can age or fade that mark.

## EquipmentManager

This is a new global table keyed by CryEngine entity ID.

| Function | Returns / behavior |
| --- | --- |
| `EquipmentManager.GetEquippedItems(entityId)` | Dense array of materialized equipped item WUIDs |
| `EquipmentManager.GetEquippedClothing(entityId)` | `{ [equipmentSlotId] = itemId }` for data-driven clothing slots |
| `EquipmentManager.GetHandSlots(entityId)` | Sparse `{ [1..8] = itemId }` using the fixed native weapon-equip mapping below; empty entries are absent |
| `EquipmentManager.GetItemInSlot(entityId, slotId)` | Item WUID in a data-driven clothing slot, or `nil`; does not query weapon or QAM slots |
| `EquipmentManager.GetEquipWeights(entityId)` | `{ total, worn }` |
| `EquipmentManager.GetInventoryEx(entityId)` | Dense array of `GetItemEx`-shaped item tables |
| `EquipmentManager.GetInventoryId(entityId)` | Inventory WUID usable with `MoveItem` |
| `EquipmentManager.SetItemEquipped(entityId, itemId, equip)` | Equip/unequip through the entity's inventory soul, preserving native visuals and bookkeeping |
| `EquipmentManager.GetWeaponQuickSlots(entityId[, outfitId])` | Four weapon-pair records: `{ outfitId, [1..4] = { usable, main?, off? } }` |
| `EquipmentManager.SetWeaponQuickSlot(entityId, quickSlot, itemId[, outfitId])` | Assign to pair `1..4`; returns the actual flattened main/off slot `1..8` selected by the native item classification |
| `EquipmentManager.ClearWeaponQuickSlot(entityId, quickSlot, offHand[, outfitId])` | Clear the main (`false`) or off (`true`) side of weapon pair `1..4` |
| `EquipmentManager.GetConsumableQuickSlots(entityId[, outfitId])` | Four records: `{ outfitId, [1..4] = { usable, item? } }` |
| `EquipmentManager.SetConsumableQuickSlot(entityId, slotId, itemId[, outfitId])` | Assign a consumable to slot `1..4`; returns `nil` when that pouch-controlled slot is not usable |
| `EquipmentManager.ClearConsumableQuickSlot(entityId, slotId[, outfitId])` | Clear consumable slot `1..4` |

For every QAM function, omitted `outfitId` selects the actor's current outfit. Explicit native outfit IDs are `0 = A`, `1 = B`, and `2 = C`.

### `GetHandSlots` mapping

Despite its compatibility name, `GetHandSlots` exposes the complete fixed `E_WeaponEquipSlot` array, not only literal hands:

| Lua key | Native value | `E_WeaponEquipSlot` |
| ---: | ---: | --- |
| `1` | `0` | `PrimaryMainHand` |
| `2` | `1` | `PrimaryOffHand` |
| `3` | `2` | `SecondaryMainHand` |
| `4` | `3` | `SecondaryOffHand` |
| `5` | `4` | `Oversized` |
| `6` | `5` | `OversizedOff` |
| `7` | `6` | `Torch` |
| `8` | `7` | `Dagger` |

## Clothing slots, weapon slots, and QAM are separate

KCD2 has three independent slot systems:

1. **Clothing `EquipmentSlotId`** — a data-driven 32-bit database ID. `GetEquippedClothing` and `GetItemInSlot` use this system.
2. **`E_WeaponEquipSlot`** — the fixed eight-entry native array exposed by `GetHandSlots`.
3. **Player QAM assignments** — outfit-specific quick-access assignments held by the player inventory soul. They are not entries in either table above.

The fixed four-byte `S_EquipmentSlotIdWrapper` does not make clothing slots a compiled enum. Only the wrapper ABI is fixed; valid IDs and names come from the `equipment_slot` database.

### Belt and pouch

| Identifier domain | Belt | Pouch |
| --- | ---: | ---: |
| Clothing `EquipmentSlotId` | `44` | `45` |
| `ArmorType` | `76` | `77` |
| `ArmorArchetypeId` used by QAM capacity logic | `103` | `104` |

The equipped container items can therefore be queried as clothing:

```lua
local beltItem = EquipmentManager.GetItemInSlot(entityId, 44)
local pouchItem = EquipmentManager.GetItemInSlot(entityId, 45)
```

Those calls return the **belt or pouch item**, not the weapon, potion, food, or consumable assigned to quick access.

- An equipped belt controls the usable weapon-QAM pair count. The native no-belt baseline is one weapon pair.
- An equipped pouch controls the usable food/consumable-QAM slot count. The native no-pouch baseline is zero food slots.
- QAM assignments are stored separately for outfits A, B, and C in player-specific outfit managers.

Weapon QAM consists of four pairs. Each pair stores a main and off item, but assignment targets the pair rather than forcing a side: the native manager classifies the item and chooses main or off. `SetWeaponQuickSlot` therefore returns the actual flattened side (`1=Main_1`, `2=Off_1`, ..., `8=Off_4`).

Consumable QAM has four independent slots and uses each pair record's main item. Belt and pouch capacity is enforced before mutation, so records can exist while reporting `usable = false`. `GetHandSlots` remains a separate physical weapon-equip query and is not a QAM alias.

## Examples

```lua
-- Which of the bandit's duplicate mailles is actually equipped?
local bandit = System.GetEntityByName("bandit_01")
for _, item in ipairs(EquipmentManager.GetInventoryEx(bandit.id) or {}) do
    if item.className == "maille_long" then
        System.LogAlways(tostring(item.id) .. " equipped=" .. tostring(item.isEquipped))
    end
end

-- Which clothing item occupies each merchant equipment slot?
local merchant = System.GetEntityByName("merchant_01")
local worn = EquipmentManager.GetEquippedClothing(merchant.id)

-- Query the player's equipped belt and pouch container items.
local belt = EquipmentManager.GetItemInSlot(player.id, 44)
local pouch = EquipmentManager.GetItemInSlot(player.id, 45)

-- Read current-outfit QAM, then explicitly read outfit B.
local currentWeapons = EquipmentManager.GetWeaponQuickSlots(player.id)
local outfitBConsumables = EquipmentManager.GetConsumableQuickSlots(player.id, 1)

-- Repair and clean the player's current main-hand item.
local itemId = player.human:GetItemInHand(0)
ItemManager.SetItemHealth(itemId, 1.0)
ItemManager.WashItem(itemId)
```

## Resolution caveats

- Distant or unstreamed NPCs may not yet have their worn items materialized as `C_Item` instances. Queries can therefore return less than the NPC conceptually owns; interact with or approach the NPC first.
- Some souls have no inventory or equipment manager. All query functions are nil-graceful instead of raising Lua errors.
- Direct stat writes use the engine's notify paths, but some UI screens refresh only after being reopened.
- `SetItemQuality` returns `nil` for non-equippable item types.

## AudioManager

`AudioManager` is a new direct-audio global. It complements the vanilla `Sound` table rather than replacing CryEngine ATL triggers or entity audio proxies.

All custom audio assets are loaded through **CryPak virtual paths**. The API does not accept absolute disk paths and does not use the Windows filesystem directly. Put assets in a mounted mod PAK under a unique path such as:

```text
Libs/Audio/MyMod/my_events.bank
Libs/Audio/MyMod/hit.ogg
```

Do not put direct-loaded banks under `Sounds/fmod/Build/PC/`; Warhorse's startup bank enumerator also scans that directory for native ATL/FSW registration.

### Return convention

Audio handles are opaque generation-checked `ScriptHandle` values. Never convert them to Lua numbers.

- Success returns one value. Mutators return `true`.
- Failure returns `nil, errorString`.
- `IsReady` always returns a Boolean.
- `GetStatus` always returns a table.
- Event handles automatically become stale after the event reaches `stopped` and the frame tick releases it.
- Core sound-instance handles become stale after explicit stop or natural completion.

```lua
local sound, err = AudioManager.LoadSound("Libs/Audio/MyMod/hit.ogg")
if not sound then
    System.LogAlways(err)
    return
end
```

### System and resources

| Function | Returns / behavior |
| --- | --- |
| `AudioManager.IsReady()` | Whether the game-owned FMOD Studio/Core systems are available and version-compatible |
| `AudioManager.GetStatus()` | Readiness, wrapper state/result, runtime version, backend epoch, resource counts, and latest asynchronous error |
| `AudioManager.GetLoadedBanks()` | Dense array of loaded-bank info tables |
| `AudioManager.GetLoadedSounds()` | Dense array of loaded loose-sound info tables |
| `AudioManager.LoadBank(path[, options])` | Load an FMOD Studio bank from CryPak memory; `{ sampleData = true }` optionally requests sample data |
| `AudioManager.UnloadBank(bankHandle[, force])` | Drop one logical reference; final normal unload refuses active events, while force stops owned events first |
| `AudioManager.LoadBankSampleData(bankHandle)` | Request bank sample data |
| `AudioManager.UnloadBankSampleData(bankHandle)` | Release bank sample data |
| `AudioManager.GetBankInfo(bankHandle)` | Path, references, active instances, event count, and sample loading state |
| `AudioManager.GetBankEvents(bankHandle)` | Dense array of raw `event:/...` paths contained in the bank |
| `AudioManager.LoadSound(path)` | Load and synchronously decode a CryPak-visible `.wav` or `.ogg`; compressed input is capped at 256 MiB |
| `AudioManager.UnloadSound(soundHandle[, force])` | Drop one logical reference; final normal unload refuses active channels |
| `AudioManager.GetSoundInfo(soundHandle)` | Path, references, active channels, duration, codec/container, channels, and bit depth |

Loading the same normalized CryPak path repeatedly returns the same handle and increments its logical reference count. Match those loads with normal unload calls, or use `force=true` to discard all references and owned instances.

### Studio events

| Function | Returns / behavior |
| --- | --- |
| `AudioManager.GetEventInfo(eventPath)` | Authored 2D/3D, oneshot, snapshot, stream, length, and distance metadata |
| `AudioManager.PlayEvent(eventPath[, options])` | Start a raw Studio event and return an event-instance handle |
| `AudioManager.StopEvent(eventHandle[, immediate])` | Request fadeout or immediate stop; handle remains valid while FMOD reports `stopping` |
| `AudioManager.SetEventPaused(eventHandle, paused)` | Pause/unpause |
| `AudioManager.SetEventParameter(eventHandle, name, value[, ignoreSeekSpeed])` | Set a named authored parameter |
| `AudioManager.SetEventVolume(eventHandle, volume)` | Set nonnegative instance volume |
| `AudioManager.SetEventPitch(eventHandle, pitch)` | Set positive instance pitch |
| `AudioManager.SetEventPosition(eventHandle, position[, velocity])` | Move a static 3D event; attached events must be detached first |
| `AudioManager.AttachEventToEntity(eventHandle, entityId[, offset])` | Follow an entity using its world transform |
| `AudioManager.DetachEvent(eventHandle)` | Freeze the last transform and zero velocity |
| `AudioManager.GetEventState(eventHandle)` | Playback state, pause, attachment, volume, and pitch |

`PlayEvent` options:

```lua
{
    position = { x = 1, y = 2, z = 3 },
    velocity = { x = 0, y = 0, z = 0 },

    -- Use entityId instead of position for attachment.
    entityId = entity.id,
    offset = { x = 0, y = 0, z = 1 },

    -- Optional static-position orientation; supply both together.
    forward = { x = 0, y = 1, z = 0 },
    up = { x = 0, y = 0, z = 1 },

    volume = 1.0,
    pitch = 1.0,
    paused = false,
    parameters = { ParameterName = 0.5 },
    ignoreSeekSpeed = false
}
```

Authored 3D events require `position` or `entityId`. Authored 2D events reject spatial fields. `position` and `entityId` are mutually exclusive.

### Loose WAV/OGG playback

| Function | Returns / behavior |
| --- | --- |
| `AudioManager.PlaySound(soundHandle[, options])` | Play a loaded sound through a KCD2 Studio bus and return a channel-instance handle |
| `AudioManager.StopSound(instanceHandle)` | Stop and invalidate the channel handle |
| `AudioManager.SetSoundPaused(instanceHandle, paused)` | Pause/unpause |
| `AudioManager.SetSoundVolume(instanceHandle, volume)` | Set nonnegative channel volume |
| `AudioManager.SetSoundPitch(instanceHandle, pitch)` | Set positive channel pitch |
| `AudioManager.SetSoundLooping(instanceHandle, loop)` | Switch loop-off/loop-normal for that channel only |
| `AudioManager.SetSoundPosition(instanceHandle, position[, velocity])` | Move a static 3D channel |
| `AudioManager.AttachSoundToEntity(instanceHandle, entityId[, offset])` | Follow an entity |
| `AudioManager.DetachSound(instanceHandle)` | Freeze the last position and zero velocity |
| `AudioManager.GetSoundState(instanceHandle)` | Bus, playback/pause/loop state, attachment, volume, pitch, and distance range |

`PlaySound` options:

```lua
{
    bus = "bus:/dieg/w_obj",
    loop = false,
    paused = false,
    volume = 1.0,
    pitch = 1.0,

    position = { x = 1, y = 2, z = 3 },
    velocity = { x = 0, y = 0, z = 0 },

    -- Use entityId instead of position for attachment.
    entityId = entity.id,
    offset = { x = 0, y = 0, z = 1 },

    minDistance = 1.0,
    maxDistance = 100.0
}
```

No spatial fields means 2D playback. `position` or `entityId` selects 3D playback. One loaded sound resource can back multiple simultaneous channels with independent loop, volume, pitch, bus, and position settings.

### Audio examples

```lua
-- Play a CryPak OGG as a UI/non-spatial effect.
local sound, err = AudioManager.LoadSound("Libs/Audio/MyMod/notify.ogg")
if sound then
    local channel
    channel, err = AudioManager.PlaySound(sound, {
        bus = "bus:/dieg/w_obj",
        volume = 0.7
    })
end

-- Attach the same resource to an entity as a looping 3D sound.
local attached, attachErr = AudioManager.PlaySound(sound, {
    entityId = player.id,
    offset = { x = 0, y = 0, z = 1.5 },
    loop = true,
    minDistance = 1,
    maxDistance = 30
})

-- Inspect and play a raw authored event.
local eventInfo, eventErr = AudioManager.GetEventInfo(
    "event:/animals/raven/raven_whistling")
local event, playErr = AudioManager.PlayEvent(
    "event:/animals/raven/raven_whistling",
    { entityId = player.id, volume = 0.25 })
```

### Audio limitations

- The plugin reuses KCD2's existing FMOD 2.2.21 systems; it does not create or ship another FMOD runtime.
- Direct-loaded banks bypass Warhorse's `FSW_BANK`/`FSW_EVENT_DESCRIPTION` registry. Their events are available through raw `AudioManager` paths, not automatically through vanilla ATL triggers, entity audio proxies, SKALD, or FlowGraph.
- Compatible event banks must target FMOD Studio 2.2.21 and normally require a KCD-derived authoring project. Banks from unrelated FMOD projects may reference incompatible mixer/bus GUIDs.
- Loose effects are fully buffered and decoded; use a Studio bank for large or streaming content.
- `AudioManager` performs no obstruction, occlusion, environment, or ATL-proxy processing for direct instances.
- The implementation is release-build verified. Live in-game CryPak bank/effect playback remains to be tested with a mounted test PAK and a compatible event-bearing bank.

## Build

LuaUtils is part of the KCD2 RE workspace. Build the `release` preset from `KCD2/RE/.buildenv`; the DLL is emitted under `build-release/LuaUtils/LuaUtils.dll` and deploys like other KCSE plugins.
