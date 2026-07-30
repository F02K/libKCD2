#include "rpgmodule/C_RPGItemHealth.h"
#include "Offsets/Offsets.h"

namespace wh::rpgmodule {

C_RPGItemHealth* C_RPGItemHealth::GetInstance()
{
    using Fn = C_RPGItemHealth* (__fastcall*)();
    static REL::Relocation<Fn> fn{ REL::ID(48258) };  // 0x1808D6070
    return fn();
}

float C_RPGItemHealth::ConditionToHealth(const entitymodule::S_EquippableItemClass* itemClass,
                                         float condition, std::uint32_t quality) const
{
    using Fn = float (__fastcall*)(const C_RPGItemHealth*,
                                   const entitymodule::S_EquippableItemClass*, float, std::uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(51854) };  // 0x18096EE58
    return fn(this, itemClass, condition, quality);
}

}  // namespace wh::rpgmodule
