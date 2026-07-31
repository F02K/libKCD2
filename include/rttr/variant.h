#pragma once

#include <cstdint>

namespace rttr {

class type;

namespace detail {

struct variant_data
{
    void* m_storage[2];  // +0x00 inline value storage or owned heap pointer
};

enum class variant_policy_operation : std::uint8_t
{
    destroy = 0,
    clone = 1,
    swap = 2,
    get_value = 5,
    get_type = 6,
    get_ptr = 7,
    get_raw_type = 8,
    get_raw_ptr = 9,
    get_address_container = 0xA,
    is_valid = 0xF,
    is_nullptr = 0x10
};

}  // namespace detail

class variant
{
public:
    using policy_func = bool (*)(std::uint8_t operation,
                                 detail::variant_data* data,
                                 void* argument);

    variant() noexcept;
    variant(const variant& other);
    variant(variant&& other) noexcept;
    variant& operator=(const variant& other);
    variant& operator=(variant&& other) noexcept;
    ~variant();

    bool is_valid() const;
    bool is_nullptr() const;
    type get_type() const;
    void* get_value() const;
    void* get_ptr() const;
    void* get_raw_ptr() const;

    static variant from_policy(detail::variant_data data, policy_func policy) noexcept;

    detail::variant_data m_data;  // +0x00
    policy_func m_policy;         // +0x10

private:
    variant(detail::variant_data data, policy_func policy) noexcept;
    void* get_policy_pointer(detail::variant_policy_operation operation) const;
};
static_assert(sizeof(variant) == 0x18,
              "rttr::variant is 16-byte storage plus one policy pointer");

}  // namespace rttr
