#pragma once

#include <cstddef>

#include "rttr/instance.h"
#include "rttr/string_view.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace rttr::detail {

class property_wrapper_base
{
public:
    virtual ~property_wrapper_base() = default;  // [0]
    virtual void unk_01() const {}               // [1]
    virtual bool is_valid() const = 0;           // [2]
    virtual void unk_03() const {}               // [3]
    virtual void unk_04() const {}               // [4]
    virtual void unk_05() const {}               // [5]
    virtual type get_type() const = 0;            // [6]
    virtual variant get_metadata(const variant& key) const = 0;  // [7]
    virtual void unk_08() const {}                // [8] set_value, deferred
    virtual variant get_value(instance object) const = 0;        // [9]

    string_view m_name;       // +0x08
    type m_declaringType;     // +0x18
};
static_assert(offsetof(property_wrapper_base, m_name) == 0x08,
              "property_wrapper_base name at 0x08");
static_assert(offsetof(property_wrapper_base, m_declaringType) == 0x18,
              "property_wrapper_base declaring type at 0x18");
static_assert(sizeof(property_wrapper_base) == 0x20,
              "property_wrapper_base must be 0x20");

}  // namespace rttr::detail
