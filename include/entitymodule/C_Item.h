#pragma once
#include <cstdint>
#include "E_ItemType.h"
#include "../framework/I_BindableObject.h"
#include "../framework/WUID.h"
#include "../crysystem/EntityEventListenerWithCleanup.h"
#include "../Offsets/vtables/IEntity.h"   // Offsets::IEntity
#include "../CryEngine/CryCommon/CryExtension/CryGUID.h"

// -----------------------------------------------
// wh::entitymodule::C_Item -- KCD2 WHGame.dll 1.5.6 (kd7u).  sizeof 0xA8.
// -----------------------------------------------
// RTTI .?AVC_Item@entitymodule@wh@@  primary vtable 0x183A2DA88  factory sub_181EEDF50  dtor sub_180477554
// Bases (CHD, MI): I_BindableObject @+0x00 (primary), EntityEventListenerWithCleanup @+0x08.
// Own members from +0x28. This is an event-listener + bindable object, NOT a C_Actor-style entity
// extension (only 2 vtable pointers).
//
// [FUNDAMENTAL vs KCD1] sizeof 0xE0 -> 0xA8: KCD1's embedded S_ItemRuntimeDataTable (0x70) is gone --
//   per-item runtime data is now a single lazily-created pointer m_pRuntimeData @+0x58. The KCD1 u32
//   m_entityId is superseded by the IEntity* at +0x88 (id read from entity+0x30).

namespace wh::entitymodule {

class  S_ItemClass;
class  I_ItemRuntimeData;
class  C_InventoryBase;

// The 16-byte owner handle stored at C_Item+0x68/+0x70 (reader sub_1819BD1C4 fills both halves).
struct S_ItemOwnerHandle {
    wh::framework::WUID owner;    // +0x00  the owning soul/entity WUID
    wh::framework::WUID context;  // +0x08  possessor/context WUID (empty-sentinel when == owner)
};

class C_Item
    : public wh::framework::I_BindableObject               // +0x00  (primary vtable)
    , public ::EntityEventListenerWithCleanup              // +0x08  (vtable + event vector, 0x20)
{
public:
    inline static constexpr auto RTTI = Offsets::RTTI_C_Item;

    // ---- wired engine functions (forwarders in src/entitymodule/C_Item.cpp; ids = kcd2 addresslib) ----
    // IsKindOf against the S_ItemClass CRTP type tree (self + ancestors); e.g. Equippable=26 gates
    // the runtime-data condition path.  0x180469590
    bool IsOfType(E_ItemType::Type type) const;
    // Returns m_pRuntimeData, lazily constructing the class-appropriate subclass.  0x180469454
    I_ItemRuntimeData* GetOrCreateRuntimeData();
    // The health-write core: change-detect vs +0x54, pre-hooks 0x180469988/0x180470138, clamped
    // write, classData vf[15] gate or |h-1|<1e-5, post-update 0x1804694C0 + runtime-data refresh,
    // then broadcast 0x1804663DC (or *(outCtx+8)=1 when outCtx given).  0x180470078
    void SetHealth(float health01, void* outNotifyCtx = nullptr);
    // RTTR "SetItemHealth" ("Sets item health... clamped 0.0-1.0"; BT SetItemProperty prop 3):
    // for Equippable-type items additionally resets the runtime-data condition (+0xA4) when
    // raising above the base health, then runs SetHealth.  0x1808D61B8
    void SetItemHealth(float health01);
    // RTTR "Condition": runtime-data condition (+0xA4) for Equippable-type items, else raw
    // m_health (+0x54).  0x18096F7D4
    float GetCondition() const;
    int32_t GetQuality() const;         // RTTR "Quality" (runtime data +0xA0)  0x180469A70
    int32_t GetMaxQuality() const;      // RTTR "MaxQuality" (class-level cap)  0x1808F6F34
    // RTTR price getters (return width proven int for CurrentStackPrice via the RPG-log
    // accumulator; the other three assumed same family).
    int32_t GetCurrentUnitPrice() const;    // 0x1804CCD18
    int32_t GetCurrentStackPrice() const;   // 0x1804CC1D8
    int32_t GetNewUnitPrice() const;        // 0x18096F68C
    int32_t GetNewStackPrice() const;       // 0x18191DDF8
    // RTTR "WashItem" (param MaxEffect, default 1.0): Equippable-type only -- routes to the
    // runtime-data wash virtual (derived vtable +0xC8).  0x182A6B758
    void WashItem(float maxEffect = 1.0f);
    void SetItemPhaseId(uint32_t phaseId);  // RTTR "SetItemPhaseId" / BT prop 1  0x182A6B09C
    void SetItemPhase(float phase01);       // BT prop 0 (set variant)            0x181505914
    void AdvanceItemPhase(float amount01);  // RTTR "AdvanceItemPhase" / BT prop 0 (advance variant, clamp 0-1)  0x182A689E4
    // RAW amount write (m_amount only -- fires NO inventory listeners; the sanctioned in-inventory
    // change is C_InventoryBase::ChangeItemAmount).  0x180468B34
    void SetAmount(int32_t amount);
    // Gated item-changed broadcast (BT SetItemProperty prop 2 pokes mask 0x4000): if the global
    // gate 0x18046646C passes, runs 0x1804663DC -> listener notify + classData vf[15]
    // (classData, item, mask, (m_flags & mask)!=0).  NOT a flag mutator.  0x18046643C
    void NotifyChanged(uint32_t flagMask);
    // Reads the 16-byte owner handle (both halves, with fallbacks).  0x1819BD1C4
    void GetOwnerHandle(S_ItemOwnerHandle& out) const;
    // The sanctioned owner write: guard 0x180469828 (early-out, disqualifier [U]); writes +0x68 =
    // owner, +0x70 = context (empty-sentinel qword_1853257E8 when context == owner), +0x78 =
    // ownership ctx from *(*(S_GameContext+0x130)+0x88)+0x48; then upserts the C_ItemManager
    // owner index (+0x18) via 0x1804CA7EC(mgr+0x18, this, indexFlag).  0x181F0F730
    bool SetOwner(const wh::framework::WUID& owner, const wh::framework::WUID& context, bool indexFlag);

    uint8_t  m_state;                                  // +0x28  init 5 (state/enum; no reader isolated)
    uint8_t  _pad29[7];                                // +0x29
    wh::framework::WUID m_wuid;                        // +0x30  registry key (tag 2 = Item)  VERIFIED
    CryGUID  m_instanceGuid;                           // +0x38
    S_ItemClass* m_pClassData;                         // +0x48  class definition (GetClassData
                                                       //        0x1804695B4; null -> static default row)  VERIFIED
    int32_t  m_amount;                                 // +0x50  stack count (init 1)  VERIFIED
    float    m_health;                                 // +0x54  init 1.0 (GetHealth); for Equippable-type items the
                                                       //        EFFECTIVE condition lives in runtime data +0xA4  VERIFIED
    I_ItemRuntimeData* m_pRuntimeData;                 // +0x58  lazily created per-item runtime data  VERIFIED
    uint32_t m_flags;                                  // +0x60  bit flags  VERIFIED. bit 0 = EQUIPPED (RPG-log
                                                       //        isEquipped reads &1); bit 1 = excluded-from-inventory-ops
                                                       //        (delete core + move filter skip when set); 0x4000 = BT
                                                       //        SetItemProperty prop-2 notify mask (meaning unresolved)
    float    m_condition;                              // +0x64  wear/condition; initial-health path 0x180469348
                                                       //        writes it alongside runtime-data condition (tentative)
    wh::framework::WUID m_owner;                       // +0x68  owner WUID (SetOwner 0x181F0F730 writes)  VERIFIED
    uint64_t m_ownerExtra;                             // +0x70  owner-handle 2nd half: possessor/context WUID,
                                                       //        empty-sentinel qword_1853257E8 when == owner  VERIFIED
    void*    m_pOwnerContext;                          // +0x78  ownership context: SetOwner stores
                                                       //        *(*(S_GameContext+0x130)+0x88)+0x48 (pointee type UNVERIFIED)
    int64_t  m_expiry;                                 // +0x80  TTL sentinel init INT64_MAX  VERIFIED
    Offsets::IEntity* m_pLinkedEntity;                 // +0x88  spawned entity (OnEntityEvent)  VERIFIED
    C_InventoryBase* m_pInventory;                     // +0x90  CURRENT container: MoveItem core 0x1808D534C sets
                                                       //        = dst on placement; DeleteItem validates == the entity
                                                       //        inventory; notified via the listener trio (vf+0x40/
                                                       //        +0x50/+0x60)  [CORRECTED from "m_pItemManager (tentative)"]
    C_InventoryBase* m_pHolder98;                      // +0x98  secondary/source holder: non-null = alternate-held
                                                       //        state (stack-merge compare vs dst; own notify reason
                                                       //        codes; ZEROED on completed move). Pointee identity
                                                       //        beyond the shared holder vtable shape UNVERIFIED
                                                       //        [CORRECTED from "m_pInventory (tentative)"]
    uint32_t m_param0xA0;                              // +0xA0  init 42 (no reader isolated; UNVERIFIED)
    uint32_t _padA4;                                   // +0xA4
};
static_assert(sizeof(C_Item) == 0xA8, "C_Item must be 0xA8");

}  // namespace wh::entitymodule
