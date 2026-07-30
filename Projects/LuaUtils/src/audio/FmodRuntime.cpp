#include "FmodRuntime.h"

#include "REL.h"

#include <cstddef>
#include <cstdint>

namespace luautils::audio {

namespace {

constexpr std::ptrdiff_t kInitStateOffset = 0xD8;
constexpr std::ptrdiff_t kInitResultOffset = 0xDC;
constexpr std::ptrdiff_t kStudioSystemOffset = 0xE0;
constexpr std::ptrdiff_t kCoreSystemOffset = 0xE8;

template <class T>
T ReadMember(void* object, std::ptrdiff_t offset)
{
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

}  // namespace

RuntimeStatus FmodRuntime::Refresh(FmodApi& api) const
{
    RuntimeStatus status;
    std::string resolveError;
    status.apiResolved = api.Resolve(resolveError);

    static REL::Relocation<void**> wrapperGlobal{ REL::ID(1257963) };
    void* wrapper = *wrapperGlobal;
    status.identity.wrapper = wrapper;
    if (!wrapper) {
        status.error = "FMOD wrapper is not available";
        return status;
    }

    status.initState = ReadMember<std::uint32_t>(wrapper, kInitStateOffset);
    status.initResultCode = ReadMember<std::uint32_t>(wrapper, kInitResultOffset);
    status.identity.studio = ReadMember<fmod::StudioSystem*>(wrapper, kStudioSystemOffset);
    status.identity.core = ReadMember<fmod::System*>(wrapper, kCoreSystemOffset);

    if (!status.apiResolved) {
        status.error = resolveError;
        return status;
    }
    if (status.initState != 2) {
        status.error = "FMOD wrapper initialization is incomplete";
        return status;
    }
    if (!status.identity.studio || !status.identity.core) {
        status.error = "FMOD system pointers are not available";
        return status;
    }
    if (!api.StudioSystemIsValid(status.identity.studio)) {
        status.error = "FMOD Studio system is invalid";
        return status;
    }

    fmod::Result result = api.SystemGetVersion(status.identity.core, &status.runtimeVersion);
    if (result != fmod::kOk) {
        status.error = FmodApi::FormatError("FMOD_System_GetVersion", result);
        return status;
    }
    if (status.runtimeVersion != fmod::kVersion) {
        status.error = "unsupported FMOD runtime version";
        return status;
    }

    status.ready = true;
    return status;
}

}  // namespace luautils::audio
