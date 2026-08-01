#pragma once

#include <cstddef>
#include <cstdint>

namespace rttr {

class parameter_info;
class parameter_info_iterator;

class parameter_info_range
{
public:
    parameter_info_range() noexcept
        : m_begin(nullptr), m_end(nullptr), m_filterStorage{}, m_filter(nullptr) {}
    parameter_info_range(const parameter_info_range&) = delete;
    parameter_info_range& operator=(const parameter_info_range&) = delete;
    ~parameter_info_range();

    std::size_t size() const;
    bool empty() const { return size() == 0; }
    parameter_info_iterator begin() const;
    parameter_info_iterator end() const;

    const parameter_info* m_begin;                   // +0x00
    const parameter_info* m_end;                     // +0x08
    alignas(void*) std::uint8_t m_filterStorage[0x38]; // +0x10
    void* m_filter;                                  // +0x48
};
static_assert(offsetof(parameter_info_range, m_filter) == 0x48,
              "rttr::parameter_info_range filter at 0x48");
static_assert(sizeof(parameter_info_range) == 0x50,
              "rttr::parameter_info_range must be 0x50");

}  // namespace rttr
