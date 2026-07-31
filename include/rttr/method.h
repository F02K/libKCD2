#pragma once

#include <vector>

#include "rttr/argument.h"
#include "rttr/instance.h"
#include "rttr/parameter_info_range.h"
#include "rttr/string_view.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace rttr {

namespace detail {
class method_wrapper_base;
}

class method
{
public:
    method() noexcept : m_wrapper(nullptr) {}
    explicit method(const detail::method_wrapper_base* wrapper) noexcept : m_wrapper(wrapper) {}
    method(const method& other) noexcept : m_wrapper(other.m_wrapper) {}
    method& operator=(const method& other) noexcept
    {
        m_wrapper = other.m_wrapper;
        return *this;
    }

    bool is_valid() const;
    string_view get_name() const;
    type get_return_type() const;
    type get_declaring_type() const;
    bool is_static() const;
    parameter_info_range get_parameter_infos() const;
    variant get_metadata(const variant& key) const;
    variant invoke_variadic(instance object,
                            const std::vector<argument>& arguments) const;

    const detail::method_wrapper_base* m_wrapper;  // +0x00
};
static_assert(sizeof(method) == 0x8, "rttr::method is one wrapper pointer");

}  // namespace rttr
