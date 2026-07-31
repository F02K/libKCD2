#pragma once

#include "rttr/string_view.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace rttr {
namespace detail {

class enumeration_wrapper_base
{
public:
    virtual ~enumeration_wrapper_base() = default;  // [0]
    virtual bool is_valid() const = 0;              // [1]
    virtual type get_underlying_type() const = 0;   // [2]
    virtual type get_type() const = 0;              // [3]
    virtual void unk_04() const {}                  // [4] names range
    virtual void unk_05() const {}                  // [5] values range
    virtual void unk_06() const {}                  // [6] value_to_name
    virtual variant name_to_value(string_view name) const = 0;  // [7]
    virtual void unk_08() const {}                  // [8] metadata, unverified

    type m_declaring_type;  // +0x08
};
static_assert(sizeof(enumeration_wrapper_base) == 0x10,
              "enumeration_wrapper_base is vptr plus declaring type");

}  // namespace detail
}  // namespace rttr
