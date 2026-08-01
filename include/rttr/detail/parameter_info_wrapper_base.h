#pragma once

#include "rttr/string_view.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace rttr {
namespace detail {

class parameter_info_wrapper_base
{
public:
    virtual ~parameter_info_wrapper_base() = default;  // [0]
    virtual string_view get_name() const = 0;          // [1]
    virtual type get_type() const = 0;                 // [2]
    virtual bool has_default_value() const = 0;        // [3]
    virtual variant get_default_value() const = 0;     // [4]
};
static_assert(sizeof(parameter_info_wrapper_base) == 0x8,
              "parameter_info_wrapper_base is one vptr");

}  // namespace detail
}  // namespace rttr
