#pragma once

#include "rttr/string_view.h"
#include "rttr/variant.h"

namespace rttr {

namespace detail {
class enumeration_wrapper_base;
}

class enumeration
{
public:
    enumeration() noexcept = default;
    explicit enumeration(const detail::enumeration_wrapper_base* wrapper) noexcept;

    bool is_valid() const;
    variant name_to_value(string_view name) const;

    const detail::enumeration_wrapper_base* m_wrapper = nullptr;  // +0x00
};
static_assert(sizeof(enumeration) == 0x8,
              "rttr::enumeration is one wrapper pointer");

}  // namespace rttr
