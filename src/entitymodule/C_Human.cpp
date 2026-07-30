#include "entitymodule/C_Human.h"

#include "REL/Relocation.h"

namespace wh::entitymodule
{
namespace
{
void* weapon_controller(const C_Human* human)
{
    auto** vtable = *reinterpret_cast<void***>(
        const_cast<C_Human*>(human));
    using Fn = void* (__fastcall*)(C_Human*);
    return reinterpret_cast<Fn>(vtable[191])(
        const_cast<C_Human*>(human));
}
}

bool C_Human::SetWeaponDrawn(bool drawn)
{
    auto* controller = weapon_controller(this);
    if (!controller)
        return false;
    if (drawn)
    {
        using Draw = void (__fastcall*)(void*);
        static REL::Relocation<Draw> draw{ REL::ID(351569) };  // Steam RVA 0x2AA24A0
        draw(controller);
    }
    else
    {
        using Holster = void (__fastcall*)(void*);
        static REL::Relocation<Holster> holster{ REL::ID(48938) };  // Steam RVA 0x8EE994
        holster(controller);
    }
    return IsWeaponDrawn() == drawn;
}

bool C_Human::IsWeaponDrawn() const
{
    auto* controller = weapon_controller(this);
    if (!controller)
        return false;
    // Exact query made by C_ScriptBindHuman::IsWeaponDrawn.
    using Fn = bool (__fastcall*)(void*, bool, std::uint32_t, std::uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(47899) };  // Steam RVA 0x8C69B4
    return fn(controller, true, 3U, 27U);
}
}
