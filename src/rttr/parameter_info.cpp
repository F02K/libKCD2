#include "rttr/parameter_info.h"

namespace rttr {

parameter_info::parameter_info(const detail::parameter_info_wrapper_base* wrapper) noexcept
    : m_wrapper(wrapper)
{}

string_view parameter_info::get_name() const
{
    return m_wrapper ? m_wrapper->get_name() : string_view{};
}

type parameter_info::get_type() const
{
    return m_wrapper ? m_wrapper->get_type() : type{};
}

bool parameter_info::has_default_value() const
{
    return m_wrapper && m_wrapper->has_default_value();
}

variant parameter_info::get_default_value() const
{
    return m_wrapper ? m_wrapper->get_default_value() : variant{};
}

}  // namespace rttr
