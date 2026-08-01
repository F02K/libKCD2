#include "rttr/RttrHandleRegistry.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace luautils {

RttrHandleRegistry::Handle RttrHandleRegistry::Store(rttr::variant&& value)
{
    if (!value.is_valid())
        throw std::invalid_argument("cannot store an invalid RTTR variant");
    if (m_nextHandle == 0)
        throw std::overflow_error("RTTR handle space exhausted");

    const Handle handle = m_nextHandle;
    auto [entry, inserted] = m_entries.emplace(handle, std::move(value));
    if (!inserted)
        throw std::logic_error("RTTR handle collision");

    m_nextHandle = handle == std::numeric_limits<Handle>::max()
        ? 0
        : handle + 1;
    return handle;
}

const rttr::variant* RttrHandleRegistry::Lookup(Handle handle) const noexcept
{
    if (handle == 0)
        return nullptr;
    const auto entry = m_entries.find(handle);
    return entry != m_entries.end() ? &entry->second : nullptr;
}

bool RttrHandleRegistry::Release(Handle handle) noexcept
{
    return handle != 0 && m_entries.erase(handle) != 0;
}

void RttrHandleRegistry::Clear() noexcept
{
    m_entries.clear();
}

}  // namespace luautils
