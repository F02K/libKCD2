#pragma once

#include "rttr/detail/parameter_info_wrapper_base.h"

namespace rttr {

class parameter_info
{
public:
    parameter_info() noexcept = default;
    explicit parameter_info(const detail::parameter_info_wrapper_base* wrapper) noexcept;

    bool is_valid() const noexcept { return m_wrapper != nullptr; }
    string_view get_name() const;
    type get_type() const;
    bool has_default_value() const;
    variant get_default_value() const;

    const detail::parameter_info_wrapper_base* m_wrapper = nullptr;  // +0x00
};
static_assert(sizeof(parameter_info) == 0x8,
              "rttr::parameter_info is one wrapper pointer");

}  // namespace rttr
