#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rttr/argument.h"
#include "rttr/instance.h"
#include "rttr/parameter_info_range.h"
#include "rttr/string_view.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace rttr::detail {

class method_wrapper_base
{
public:
    virtual ~method_wrapper_base() = default;  // [0]
    virtual bool is_valid() const = 0;         // [1]
    virtual void unk_02() const {}             // [2]
    virtual void unk_03() const {}             // [3]
    virtual type get_return_type() const = 0;  // [4]
    virtual bool is_static() const = 0;        // [5]
    virtual void unk_06() const {}             // [6]
    virtual void unk_07() const {}             // [7]
    virtual parameter_info_range get_parameter_infos() const = 0;  // [8]
    virtual variant get_metadata(const variant& key) const = 0;    // [9]
    virtual void unk_0A() const {}             // [10]
    virtual void unk_0B() const {}             // [11]
    virtual void unk_0C() const {}             // [12]
    virtual void unk_0D() const {}             // [13]
    virtual void unk_0E() const {}             // [14]
    virtual void unk_0F() const {}             // [15]
    virtual void unk_10() const {}             // [16]
    virtual variant invoke_variadic(instance object,
                                    const std::vector<argument>& arguments) const = 0;  // [17]

    string_view m_name;           // +0x08
    string_view m_unk18;          // +0x18, zero in sampled registrations
    type m_declaringType;         // +0x28
    std::string m_signature;      // +0x30
};
static_assert(offsetof(method_wrapper_base, m_name) == 0x08,
              "method_wrapper_base name at 0x08");
static_assert(offsetof(method_wrapper_base, m_declaringType) == 0x28,
              "method_wrapper_base declaring type at 0x28");
static_assert(offsetof(method_wrapper_base, m_signature) == 0x30,
              "method_wrapper_base signature at 0x30");
static_assert(sizeof(method_wrapper_base) == 0x50,
              "method_wrapper_base must be 0x50");

}  // namespace rttr::detail
