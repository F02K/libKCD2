#include "rttr/argument.h"

#include "REL.h"
#include "rttr/variant.h"

namespace rttr {

argument::argument() noexcept
    : m_value(nullptr), m_variant(nullptr), m_type()
{}

argument::argument(const variant& value)
{
    using Fn = argument*(__fastcall*)(argument*, const variant*);
    static REL::Relocation<Fn> fn{ REL::ID(29710) };  // 0x1804F81F8
    fn(this, &value);
}

argument::argument(void* value, type valueType) noexcept
    : m_value(value), m_variant(nullptr), m_type(valueType)
{}

}  // namespace rttr
