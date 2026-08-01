#pragma once

#include "FmodApi.h"

#include <cstdint>
#include <string>

namespace luautils::audio {

struct BackendIdentity {
    void* wrapper = nullptr;
    fmod::StudioSystem* studio = nullptr;
    fmod::System* core = nullptr;

    bool operator==(const BackendIdentity& other) const
    {
        return wrapper == other.wrapper && studio == other.studio && core == other.core;
    }
    bool operator!=(const BackendIdentity& other) const { return !(*this == other); }
};

struct RuntimeStatus {
    bool apiResolved = false;
    bool ready = false;
    std::uint32_t initState = 0;
    std::uint32_t initResultCode = 0;
    std::uint32_t runtimeVersion = 0;
    BackendIdentity identity;
    std::string error;
};

class FmodRuntime
{
public:
    RuntimeStatus Refresh(FmodApi& api) const;
};

}  // namespace luautils::audio
