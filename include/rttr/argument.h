#pragma once

#include "rttr/type.h"

namespace rttr {

class variant;

class argument
{
public:
    argument() noexcept;
    explicit argument(const variant& value);
    argument(void* value, type valueType) noexcept;

    void* get_value() const noexcept { return m_value; }
    type get_type() const noexcept { return m_type; }

    void* m_value;             // +0x00 resolved value address
    const variant* m_variant;  // +0x08 source variant, null for a direct value view
    type m_type;               // +0x10 exact reflected type
};
static_assert(sizeof(argument) == 0x18, "rttr::argument must be 0x18");

}  // namespace rttr
