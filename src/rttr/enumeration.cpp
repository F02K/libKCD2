#include "rttr/enumeration.h"

#include "rttr/detail/enumeration_wrapper_base.h"

namespace rttr {

enumeration::enumeration(const detail::enumeration_wrapper_base* wrapper) noexcept
    : m_wrapper(wrapper)
{}

bool enumeration::is_valid() const
{
    return m_wrapper && m_wrapper->is_valid();
}

variant enumeration::name_to_value(string_view name) const
{
    return m_wrapper ? m_wrapper->name_to_value(name) : variant{};
}

}  // namespace rttr
