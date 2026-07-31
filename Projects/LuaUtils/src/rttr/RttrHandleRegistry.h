#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "rttr/variant.h"

namespace luautils {

class RttrHandleRegistry
{
public:
    using Handle = std::uint64_t;

    RttrHandleRegistry() = default;
    RttrHandleRegistry(const RttrHandleRegistry&) = delete;
    RttrHandleRegistry& operator=(const RttrHandleRegistry&) = delete;

    Handle Store(rttr::variant&& value);
    const rttr::variant* Lookup(Handle handle) const noexcept;
    bool Release(Handle handle) noexcept;
    void Clear() noexcept;

    std::size_t Size() const noexcept { return m_entries.size(); }

private:
    std::unordered_map<Handle, rttr::variant> m_entries;
    Handle m_nextHandle = 1;
};

}  // namespace luautils
