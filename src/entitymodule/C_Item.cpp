#include "entitymodule/C_Item.h"
#include "entitymodule/C_EquippableItemRuntimeData.h"
#include "entitymodule/S_ItemClass.h"
#include "Offsets/Offsets.h"
#include "rpgmodule/C_RPGItemHealth.h"

#include <algorithm>

// C_Item engine-function forwarders (KCD2 WHGame.dll 1.5.6 RVAs; ids = kcd2 address library).
// Discovery context: RTTR type "wh::entitymodule::Item" registrar sub_180CE0CD8, the BT
// C_SetItemProperty worker 0x1830C36F4, and the inventory move/delete cores.

namespace wh::entitymodule {

bool C_Item::IsOfType(E_ItemType::Type type) const
{
    using Fn = bool (__fastcall*)(const C_Item*, int32_t);
    static REL::Relocation<Fn> fn{ REL::ID(26787) };  // 0x180469590
    return fn(this, type);
}

I_ItemRuntimeData* C_Item::GetOrCreateRuntimeData()
{
    using Fn = I_ItemRuntimeData* (__fastcall*)(C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(26781) };  // 0x180469454
    return fn(this);
}

S_ItemClass* C_Item::GetClassData() const
{
    using Fn = S_ItemClass* (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(26788) };  // 0x1804695B4
    return fn(this);
}

void C_Item::SetHealth(float health, void* outNotifyCtx)
{
    using Fn = void (__fastcall*)(C_Item*, float, void*);
    static REL::Relocation<Fn> fn{ REL::ID(27020) };  // 0x180470078
    fn(this, health, outNotifyCtx);
}

void C_Item::SetItemHealth(float health)
{
    using Fn = void (__fastcall*)(C_Item*, float);
    static REL::Relocation<Fn> fn{ REL::ID(48260) };  // 0x1808D61B8
    fn(this, health);
}

bool C_Item::SetCondition(float condition)
{
    condition = std::clamp(condition, 0.0F, 1.0F);
    if (!IsOfType(E_ItemType::Equippable)) {
        SetHealth(condition);
        return true;
    }

    auto* classData = GetClassData();
    auto* equippableClass = classData ? classData->GetAsEquippableItemClass() : nullptr;
    auto* itemHealth = wh::rpgmodule::C_RPGItemHealth::GetInstance();
    if (!equippableClass || !itemHealth) {
        return false;
    }
    const auto health = itemHealth->ConditionToHealth(
        equippableClass, condition, static_cast<std::uint32_t>(GetQuality()));
    SetHealth(health);
    return true;
}

bool C_Item::SetQuality(int32_t quality)
{
    if (!IsOfType(E_ItemType::Equippable)) {
        return false;
    }
    auto* runtime = static_cast<C_EquippableItemRuntimeData*>(GetOrCreateRuntimeData());
    if (!runtime) {
        return false;
    }
    const auto condition = GetCondition();
    const auto previousQuality = runtime->m_quality;
    runtime->m_quality = std::clamp(quality, 0, GetMaxQuality());
    if (!SetCondition(condition)) {
        runtime->m_quality = previousQuality;
        return false;
    }
    NotifyChanged(0x4000U);
    return true;
}

float C_Item::GetCondition() const
{
    using Fn = float (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(51872) };  // 0x18096F7D4
    return fn(this);
}

int32_t C_Item::GetQuality() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(26812) };  // 0x180469A70
    return fn(this);
}

int32_t C_Item::GetMaxQuality() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(49191) };  // 0x1808F6F34
    return fn(this);
}

int32_t C_Item::GetCurrentUnitPrice() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(28773) };  // 0x1804CCD18
    return fn(this);
}

int32_t C_Item::GetCurrentStackPrice() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(28763) };  // 0x1804CC1D8
    return fn(this);
}

int32_t C_Item::GetNewUnitPrice() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(51867) };  // 0x18096F68C
    return fn(this);
}

int32_t C_Item::GetNewStackPrice() const
{
    using Fn = int32_t (__fastcall*)(const C_Item*);
    static REL::Relocation<Fn> fn{ REL::ID(146500) };  // 0x18191DDF8
    return fn(this);
}

void C_Item::WashItem(float maxEffect)
{
    using Fn = void (__fastcall*)(C_Item*, float);
    static REL::Relocation<Fn> fn{ REL::ID(350090) };  // 0x182A6B758
    fn(this, maxEffect);
}

void C_Item::SetItemPhaseId(uint32_t phaseId)
{
    using Fn = void (__fastcall*)(C_Item*, uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(350081) };  // 0x182A6B09C
    fn(this, phaseId);
}

void C_Item::SetItemPhase(float phase)
{
    using Fn = void (__fastcall*)(C_Item*, float);
    static REL::Relocation<Fn> fn{ REL::ID(119762) };  // 0x181505914
    fn(this, phase);
}

void C_Item::AdvanceItemPhase(float amount)
{
    using Fn = void (__fastcall*)(C_Item*, float);
    static REL::Relocation<Fn> fn{ REL::ID(350000) };  // 0x182A689E4
    fn(this, amount);
}

void C_Item::SetAmount(int32_t amount)
{
    using Fn = void (__fastcall*)(C_Item*, int32_t);
    static REL::Relocation<Fn> fn{ REL::ID(26770) };  // 0x180468B34
    fn(this, amount);
}

void C_Item::SetInstanceGuid(const CryGUID& instanceGuid)
{
    using Fn = void (__fastcall*)(C_Item*, const CryGUID*);
    static REL::Relocation<Fn> fn{ REL::ID(26742) };  // Steam RVA 0x467A6C
    fn(this, &instanceGuid);
}

void C_Item::NotifyChanged(uint32_t flagMask)
{
    using Fn = void (__fastcall*)(C_Item*, uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(26695) };  // 0x18046643C
    fn(this, flagMask);
}

void C_Item::GetOwnerHandle(wh::framework::WUID& out) const
{
    using Fn = wh::framework::WUID* (__fastcall*)(const C_Item*, wh::framework::WUID*);
    static REL::Relocation<Fn> fn{ REL::ID(150937) };  // 0x1819BD1C4
    fn(this, &out);
}

bool C_Item::SetOwner(const wh::framework::WUID& owner, const wh::framework::WUID& stolenFromOwner, bool indexFlag)
{
    using Fn = bool (__fastcall*)(C_Item*, const wh::framework::WUID*, const wh::framework::WUID*, uint8_t);
    static REL::Relocation<Fn> fn{ REL::ID(195376) };  // 0x181F0F730
    return fn(this, &owner, &stolenFromOwner, indexFlag);
}

}  // namespace wh::entitymodule
