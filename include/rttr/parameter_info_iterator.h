#pragma once

namespace rttr {

class parameter_info;
class parameter_info_range;

class parameter_info_iterator
{
public:
    parameter_info_iterator() noexcept = default;
    parameter_info_iterator(const parameter_info* current,
                            const parameter_info_range* range) noexcept;

    const parameter_info& operator*() const noexcept;
    const parameter_info* operator->() const noexcept;
    parameter_info_iterator& operator++();

    bool operator==(const parameter_info_iterator& other) const noexcept;
    bool operator!=(const parameter_info_iterator& other) const noexcept;

    const parameter_info* m_current = nullptr;       // +0x00
    const parameter_info_range* m_range = nullptr;   // +0x08
};
static_assert(sizeof(parameter_info_iterator) == 0x10,
              "rttr::parameter_info_iterator must be 0x10");

}  // namespace rttr
