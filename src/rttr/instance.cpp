#include "rttr/instance.h"

#include "REL.h"
#include "rttr/variant.h"

namespace rttr {

instance::instance()
{
    using Fn = instance*(__fastcall*)(instance*);
    static REL::Relocation<Fn> fn{ REL::ID(36422) };  // 0x1806913CC
    fn(this);
}

instance::instance(const variant& value)
{
    // Internal helper is (variant, out), unlike a normal constructor.
    using Fn = instance*(__fastcall*)(const variant*, instance*);
    static REL::Relocation<Fn> fn{ REL::ID(36784) };  // 0x18069D270
    fn(&value, this);
}

}  // namespace rttr
