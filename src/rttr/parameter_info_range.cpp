#include "rttr/parameter_info_range.h"

#include "REL.h"
#include "rttr/parameter_info_iterator.h"

namespace rttr {

parameter_info_range::~parameter_info_range()
{
    using Fn = void(__fastcall*)(parameter_info_range*);
    static REL::Relocation<Fn> fn{ REL::ID(37063) };  // 0x1806A91D8
    fn(this);
}

std::size_t parameter_info_range::size() const
{
    using Fn = std::size_t(__fastcall*)(const parameter_info_range*);
    static REL::Relocation<Fn> fn{ REL::ID(36765) };  // 0x18069C998
    return fn(this);
}

parameter_info_iterator parameter_info_range::begin() const
{
    // 0x10-byte member return is this-first then hidden result.
    using Fn = parameter_info_iterator*(__fastcall*)(const parameter_info_range*,
                                                     parameter_info_iterator*);
    static REL::Relocation<Fn> fn{ REL::ID(36767) };  // 0x18069C9EC
    parameter_info_iterator result;
    fn(this, &result);
    return result;
}

parameter_info_iterator parameter_info_range::end() const
{
    return parameter_info_iterator(m_end, this);
}

}  // namespace rttr
