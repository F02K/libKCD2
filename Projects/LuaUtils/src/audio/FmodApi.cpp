#include "FmodApi.h"

#include <Windows.h>

#include <iterator>
#include <string>

namespace luautils::audio {

namespace {

template <class T>
bool ResolveOne(HMODULE module, const char* name, T& out, std::string& error)
{
    out = reinterpret_cast<T>(GetProcAddress(module, name));
    if (out)
        return true;
    error = std::string("missing FMOD export: ") + name;
    return false;
}

}  // namespace

bool FmodApi::Resolve(std::string& error)
{
    if (m_resolved)
        return true;

    HMODULE studio = GetModuleHandleW(L"fmodstudio.dll");
    HMODULE core = GetModuleHandleW(L"fmod.dll");
    if (!studio || !core) {
        error = "FMOD runtime modules are not loaded";
        return false;
    }

    FmodApi loaded;
#define LOAD_STUDIO(member, exportName) \
    if (!ResolveOne(studio, exportName, loaded.member, error)) return false
#define LOAD_CORE(member, exportName) \
    if (!ResolveOne(core, exportName, loaded.member, error)) return false

    LOAD_STUDIO(StudioSystemIsValid, "FMOD_Studio_System_IsValid");
    LOAD_STUDIO(StudioSystemGetEvent, "FMOD_Studio_System_GetEvent");
    LOAD_STUDIO(StudioSystemGetBus, "FMOD_Studio_System_GetBus");
    LOAD_STUDIO(StudioSystemLoadBankMemory, "FMOD_Studio_System_LoadBankMemory");
    LOAD_STUDIO(StudioSystemFlushCommands, "FMOD_Studio_System_FlushCommands");
    LOAD_STUDIO(StudioBankIsValid, "FMOD_Studio_Bank_IsValid");
    LOAD_STUDIO(StudioBankUnload, "FMOD_Studio_Bank_Unload");
    LOAD_STUDIO(StudioBankLoadSampleData, "FMOD_Studio_Bank_LoadSampleData");
    LOAD_STUDIO(StudioBankUnloadSampleData, "FMOD_Studio_Bank_UnloadSampleData");
    LOAD_STUDIO(StudioBankGetSampleLoadingState, "FMOD_Studio_Bank_GetSampleLoadingState");
    LOAD_STUDIO(StudioBankGetEventCount, "FMOD_Studio_Bank_GetEventCount");
    LOAD_STUDIO(StudioBankGetEventList, "FMOD_Studio_Bank_GetEventList");
    LOAD_STUDIO(StudioEventDescriptionIsValid, "FMOD_Studio_EventDescription_IsValid");
    LOAD_STUDIO(StudioEventDescriptionGetPath, "FMOD_Studio_EventDescription_GetPath");
    LOAD_STUDIO(StudioEventDescriptionIs3D, "FMOD_Studio_EventDescription_Is3D");
    LOAD_STUDIO(StudioEventDescriptionIsOneshot, "FMOD_Studio_EventDescription_IsOneshot");
    LOAD_STUDIO(StudioEventDescriptionIsSnapshot, "FMOD_Studio_EventDescription_IsSnapshot");
    LOAD_STUDIO(StudioEventDescriptionIsStream, "FMOD_Studio_EventDescription_IsStream");
    LOAD_STUDIO(StudioEventDescriptionGetLength, "FMOD_Studio_EventDescription_GetLength");
    LOAD_STUDIO(StudioEventDescriptionGetMinMaxDistance, "FMOD_Studio_EventDescription_GetMinMaxDistance");
    LOAD_STUDIO(StudioEventDescriptionCreateInstance, "FMOD_Studio_EventDescription_CreateInstance");
    LOAD_STUDIO(StudioEventInstanceIsValid, "FMOD_Studio_EventInstance_IsValid");
    LOAD_STUDIO(StudioEventInstanceStart, "FMOD_Studio_EventInstance_Start");
    LOAD_STUDIO(StudioEventInstanceStop, "FMOD_Studio_EventInstance_Stop");
    LOAD_STUDIO(StudioEventInstanceRelease, "FMOD_Studio_EventInstance_Release");
    LOAD_STUDIO(StudioEventInstanceSetPaused, "FMOD_Studio_EventInstance_SetPaused");
    LOAD_STUDIO(StudioEventInstanceGetPaused, "FMOD_Studio_EventInstance_GetPaused");
    LOAD_STUDIO(StudioEventInstanceSetVolume, "FMOD_Studio_EventInstance_SetVolume");
    LOAD_STUDIO(StudioEventInstanceGetVolume, "FMOD_Studio_EventInstance_GetVolume");
    LOAD_STUDIO(StudioEventInstanceSetPitch, "FMOD_Studio_EventInstance_SetPitch");
    LOAD_STUDIO(StudioEventInstanceGetPitch, "FMOD_Studio_EventInstance_GetPitch");
    LOAD_STUDIO(StudioEventInstanceSetParameterByName, "FMOD_Studio_EventInstance_SetParameterByName");
    LOAD_STUDIO(StudioEventInstanceSet3DAttributes, "FMOD_Studio_EventInstance_Set3DAttributes");
    LOAD_STUDIO(StudioEventInstanceGet3DAttributes, "FMOD_Studio_EventInstance_Get3DAttributes");
    LOAD_STUDIO(StudioEventInstanceGetPlaybackState, "FMOD_Studio_EventInstance_GetPlaybackState");
    LOAD_STUDIO(StudioBusIsValid, "FMOD_Studio_Bus_IsValid");
    LOAD_STUDIO(StudioBusLockChannelGroup, "FMOD_Studio_Bus_LockChannelGroup");
    LOAD_STUDIO(StudioBusUnlockChannelGroup, "FMOD_Studio_Bus_UnlockChannelGroup");
    LOAD_STUDIO(StudioBusGetChannelGroup, "FMOD_Studio_Bus_GetChannelGroup");

    LOAD_CORE(SystemGetVersion, "FMOD_System_GetVersion");
    LOAD_CORE(SystemCreateSound, "FMOD_System_CreateSound");
    LOAD_CORE(SystemPlaySound, "FMOD_System_PlaySound");
    LOAD_CORE(SoundRelease, "FMOD_Sound_Release");
    LOAD_CORE(SoundGetLength, "FMOD_Sound_GetLength");
    LOAD_CORE(SoundGetFormat, "FMOD_Sound_GetFormat");
    LOAD_CORE(ChannelStop, "FMOD_Channel_Stop");
    LOAD_CORE(ChannelIsPlaying, "FMOD_Channel_IsPlaying");
    LOAD_CORE(ChannelSetPaused, "FMOD_Channel_SetPaused");
    LOAD_CORE(ChannelGetPaused, "FMOD_Channel_GetPaused");
    LOAD_CORE(ChannelSetVolume, "FMOD_Channel_SetVolume");
    LOAD_CORE(ChannelGetVolume, "FMOD_Channel_GetVolume");
    LOAD_CORE(ChannelSetPitch, "FMOD_Channel_SetPitch");
    LOAD_CORE(ChannelGetPitch, "FMOD_Channel_GetPitch");
    LOAD_CORE(ChannelSetMode, "FMOD_Channel_SetMode");
    LOAD_CORE(ChannelGetMode, "FMOD_Channel_GetMode");
    LOAD_CORE(ChannelSet3DAttributes, "FMOD_Channel_Set3DAttributes");
    LOAD_CORE(ChannelGet3DAttributes, "FMOD_Channel_Get3DAttributes");
    LOAD_CORE(ChannelSet3DMinMaxDistance, "FMOD_Channel_Set3DMinMaxDistance");
    LOAD_CORE(ChannelGet3DMinMaxDistance, "FMOD_Channel_Get3DMinMaxDistance");

#undef LOAD_CORE
#undef LOAD_STUDIO

    loaded.m_resolved = true;
    *this = loaded;
    error.clear();
    return true;
}

void FmodApi::Reset()
{
    *this = FmodApi{};
}

const char* FmodApi::ResultName(fmod::Result result)
{
    static constexpr const char* names[] = {
        "FMOD_OK", "FMOD_ERR_BADCOMMAND", "FMOD_ERR_CHANNEL_ALLOC",
        "FMOD_ERR_CHANNEL_STOLEN", "FMOD_ERR_DMA", "FMOD_ERR_DSP_CONNECTION",
        "FMOD_ERR_DSP_DONTPROCESS", "FMOD_ERR_DSP_FORMAT", "FMOD_ERR_DSP_INUSE",
        "FMOD_ERR_DSP_NOTFOUND", "FMOD_ERR_DSP_RESERVED", "FMOD_ERR_DSP_SILENCE",
        "FMOD_ERR_DSP_TYPE", "FMOD_ERR_FILE_BAD", "FMOD_ERR_FILE_COULDNOTSEEK",
        "FMOD_ERR_FILE_DISKEJECTED", "FMOD_ERR_FILE_EOF", "FMOD_ERR_FILE_ENDOFDATA",
        "FMOD_ERR_FILE_NOTFOUND", "FMOD_ERR_FORMAT", "FMOD_ERR_HEADER_MISMATCH",
        "FMOD_ERR_HTTP", "FMOD_ERR_HTTP_ACCESS", "FMOD_ERR_HTTP_PROXY_AUTH",
        "FMOD_ERR_HTTP_SERVER_ERROR", "FMOD_ERR_HTTP_TIMEOUT", "FMOD_ERR_INITIALIZATION",
        "FMOD_ERR_INITIALIZED", "FMOD_ERR_INTERNAL", "FMOD_ERR_INVALID_FLOAT",
        "FMOD_ERR_INVALID_HANDLE", "FMOD_ERR_INVALID_PARAM", "FMOD_ERR_INVALID_POSITION",
        "FMOD_ERR_INVALID_SPEAKER", "FMOD_ERR_INVALID_SYNCPOINT", "FMOD_ERR_INVALID_THREAD",
        "FMOD_ERR_INVALID_VECTOR", "FMOD_ERR_MAXAUDIBLE", "FMOD_ERR_MEMORY",
        "FMOD_ERR_MEMORY_CANTPOINT", "FMOD_ERR_NEEDS3D", "FMOD_ERR_NEEDSHARDWARE",
        "FMOD_ERR_NET_CONNECT", "FMOD_ERR_NET_SOCKET_ERROR", "FMOD_ERR_NET_URL",
        "FMOD_ERR_NET_WOULD_BLOCK", "FMOD_ERR_NOTREADY", "FMOD_ERR_OUTPUT_ALLOCATED",
        "FMOD_ERR_OUTPUT_CREATEBUFFER", "FMOD_ERR_OUTPUT_DRIVERCALL", "FMOD_ERR_OUTPUT_FORMAT",
        "FMOD_ERR_OUTPUT_INIT", "FMOD_ERR_OUTPUT_NODRIVERS", "FMOD_ERR_PLUGIN",
        "FMOD_ERR_PLUGIN_MISSING", "FMOD_ERR_PLUGIN_RESOURCE", "FMOD_ERR_PLUGIN_VERSION",
        "FMOD_ERR_RECORD", "FMOD_ERR_REVERB_CHANNELGROUP", "FMOD_ERR_REVERB_INSTANCE",
        "FMOD_ERR_SUBSOUNDS", "FMOD_ERR_SUBSOUND_ALLOCATED", "FMOD_ERR_SUBSOUND_CANTMOVE",
        "FMOD_ERR_TAGNOTFOUND", "FMOD_ERR_TOOMANYCHANNELS", "FMOD_ERR_TRUNCATED",
        "FMOD_ERR_UNIMPLEMENTED", "FMOD_ERR_UNINITIALIZED", "FMOD_ERR_UNSUPPORTED",
        "FMOD_ERR_VERSION", "FMOD_ERR_EVENT_ALREADY_LOADED", "FMOD_ERR_EVENT_LIVEUPDATE_BUSY",
        "FMOD_ERR_EVENT_LIVEUPDATE_MISMATCH", "FMOD_ERR_EVENT_LIVEUPDATE_TIMEOUT",
        "FMOD_ERR_EVENT_NOTFOUND", "FMOD_ERR_STUDIO_UNINITIALIZED", "FMOD_ERR_STUDIO_NOT_LOADED",
        "FMOD_ERR_INVALID_STRING", "FMOD_ERR_ALREADY_LOCKED", "FMOD_ERR_NOT_LOCKED",
        "FMOD_ERR_RECORD_DISCONNECTED", "FMOD_ERR_TOOMANYSAMPLES",
    };
    if (result >= 0 && static_cast<std::size_t>(result) < std::size(names))
        return names[result];
    return "FMOD_ERR_UNKNOWN";
}

std::string FmodApi::FormatError(const char* operation, fmod::Result result)
{
    return std::string(operation) + ": " + ResultName(result) + " (" +
           std::to_string(result) + ")";
}

}  // namespace luautils::audio
