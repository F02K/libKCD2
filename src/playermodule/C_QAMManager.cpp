#include "playermodule/C_QAMManager.h"
#include "Offsets/Offsets.h"

namespace wh::playermodule {

void C_QAMManager::SetItem(entitymodule::C_Item* item, std::uint32_t slot)
{
    using Fn = void (__fastcall*)(C_QAMManager*, entitymodule::C_Item*, std::uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(204887) };  // 0x18203C030
    fn(this, item, slot);
}

bool C_QAMManager::ClearWeaponItem(entitymodule::C_Item* item, E_QAM_WeaponSlot::Type slot)
{
    using Fn = bool (__fastcall*)(C_QAMManager*, entitymodule::C_Item*, E_QAM_WeaponSlot::Type);
    static REL::Relocation<Fn> fn{ REL::ID(204903) };  // 0x18203CF70
    return fn(this, item, slot);
}

}  // namespace wh::playermodule
