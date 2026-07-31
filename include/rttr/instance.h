#pragma once

#include "rttr/type.h"

namespace rttr {

class variant;

class instance
{
public:
    instance();
    explicit instance(const variant& value);

    type m_type;      // +0x00 reflected value type
    type m_rawType;   // +0x08 reflected raw type
    void* m_ptr;      // +0x10 adjusted value pointer
    void* m_rawPtr;   // +0x18 raw value pointer
};
static_assert(sizeof(instance) == 0x20, "rttr::instance must be 0x20");

}  // namespace rttr
