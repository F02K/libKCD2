#include "rttr/parameter_info_iterator.h"

#include "REL.h"
#include "rttr/parameter_info.h"
#include "rttr/parameter_info_range.h"

namespace rttr {

parameter_info_iterator::parameter_info_iterator(
    const parameter_info* current, const parameter_info_range* range) noexcept
    : m_current(current), m_range(range)
{}

const parameter_info& parameter_info_iterator::operator*() const noexcept
{
    return *m_current;
}

const parameter_info* parameter_info_iterator::operator->() const noexcept
{
    return m_current;
}

parameter_info_iterator& parameter_info_iterator::operator++()
{
    using Fn = void(__fastcall*)(const parameter_info_range*, parameter_info_iterator*);
    static REL::Relocation<Fn> fn{ REL::ID(37056) };  // 0x1806A8E80
    fn(m_range, this);
    return *this;
}

bool parameter_info_iterator::operator==(const parameter_info_iterator& other) const noexcept
{
    return m_current == other.m_current;
}

bool parameter_info_iterator::operator!=(const parameter_info_iterator& other) const noexcept
{
    return !(*this == other);
}

}  // namespace rttr
