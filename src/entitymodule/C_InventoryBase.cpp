#include "entitymodule/C_InventoryBase.h"
#include "Offsets/Offsets.h"

// C_InventoryBase engine-function forwarders (KCD2 WHGame.dll 1.5.6 RVAs, utem;
// ids = kcd2 address library).

namespace wh::entitymodule {

C_Item* C_InventoryBase::FindItemByClass(const CryGUID& classId)
{
    // 0x1808D315C: linear walk of m_items comparing each item's class guid
    // (*(item+0x48) = S_ItemClass, guid at +0x08).
    using Fn = C_Item* (__fastcall*)(C_InventoryBase*, const CryGUID*);
    static REL::Relocation<Fn> fn{ REL::ID(48187) };  // 0x1808D315C
    return fn(this, &classId);
}

bool C_InventoryBase::ChangeItemAmount(C_Item* item, int32_t delta)
{
    using Fn = bool (__fastcall*)(C_InventoryBase*, C_Item*, int32_t);
    static REL::Relocation<Fn> fn{ REL::ID(136741) };  // 0x18179CF50
    return fn(this, item, delta);
}

void C_InventoryBase::RemoveItem(C_Item* item, uint32_t reason, uint32_t count)
{
    using Fn = void (__fastcall*)(C_InventoryBase*, C_Item*, uint32_t, uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(27292) };  // 0x180479758
    fn(this, item, reason, count);
}

C_Item* C_InventoryBase::MoveItemIn(C_Item* item, uint32_t count, bool unkOwnerFlag)
{
    using Fn = C_Item* (__fastcall*)(C_InventoryBase*, C_Item*, uint32_t, uint8_t);
    static REL::Relocation<Fn> fn{ REL::ID(48244) };  // 0x1808D534C
    return fn(this, item, count, unkOwnerFlag);
}

C_Item* C_InventoryBase::CreateItem(
    const CryGUID& classId,
    float health,
    uint32_t amount)
{
    // S_ItemInitParams is 0xF8 bytes (all observed inventory paths step the
    // records by 0xF8). Keep it opaque here because its internal fields are
    // owned by the engine builder/destructor pair.
    struct S_ItemInitParamsOpaque {
        std::byte data[0xF8];
    };
    static_assert(sizeof(S_ItemInitParamsOpaque) == 0xF8);
    alignas(16) S_ItemInitParamsOpaque params{};
    using Build = void* (__fastcall*)(
        S_ItemInitParamsOpaque*,
        const CryGUID*,
        uint32_t,
        float);
    using Insert = C_Item* (__fastcall*)(
        C_InventoryBase*,
        S_ItemInitParamsOpaque*,
        uint32_t,
        uint8_t);
    using Destroy = void (__fastcall*)(S_ItemInitParamsOpaque*);
    static REL::Relocation<Build> build{ REL::ID(26335) };    // Steam RVA 0x4533E4
    static REL::Relocation<Insert> insert{ REL::ID(26689) };  // Steam RVA 0x465FC0
    static REL::Relocation<Destroy> destroy{ REL::ID(26348) };// Steam RVA 0x453BEC

    // BuildItemInitParams copies the GUID with `movaps xmm0, [rdx]`. CryGUID
    // itself only has 8-byte alignment, so callers are not required to provide
    // the 16-byte alignment expected by this native implementation. Mirror the
    // script binder's aligned stack copy before crossing the engine ABI.
    static_assert(sizeof(CryGUID) == 0x10);
    alignas(16) const CryGUID alignedClassId = classId;
    build(&params, &alignedClassId, amount, health);
    auto* result = insert(this, &params, 4, 0);
    destroy(&params);
    return result;
}

}  // namespace wh::entitymodule
