#pragma once

#include "FmodTypes.h"

#include <string>

namespace luautils::audio {

class FmodApi
{
public:
    bool Resolve(std::string& error);
    bool IsResolved() const { return m_resolved; }
    void Reset();

    static const char* ResultName(fmod::Result result);
    static std::string FormatError(const char* operation, fmod::Result result);

    fmod::Bool (*StudioSystemIsValid)(fmod::StudioSystem*) = nullptr;
    fmod::Result (*StudioSystemGetEvent)(fmod::StudioSystem*, const char*, fmod::StudioEventDescription**) = nullptr;
    fmod::Result (*StudioSystemGetBus)(fmod::StudioSystem*, const char*, fmod::StudioBus**) = nullptr;
    fmod::Result (*StudioSystemLoadBankMemory)(fmod::StudioSystem*, const char*, int, int,
                                               fmod::StudioLoadBankFlags, fmod::StudioBank**) = nullptr;
    fmod::Result (*StudioSystemFlushCommands)(fmod::StudioSystem*) = nullptr;

    fmod::Bool (*StudioBankIsValid)(fmod::StudioBank*) = nullptr;
    fmod::Result (*StudioBankUnload)(fmod::StudioBank*) = nullptr;
    fmod::Result (*StudioBankLoadSampleData)(fmod::StudioBank*) = nullptr;
    fmod::Result (*StudioBankUnloadSampleData)(fmod::StudioBank*) = nullptr;
    fmod::Result (*StudioBankGetSampleLoadingState)(fmod::StudioBank*, fmod::StudioLoadingState*) = nullptr;
    fmod::Result (*StudioBankGetEventCount)(fmod::StudioBank*, int*) = nullptr;
    fmod::Result (*StudioBankGetEventList)(fmod::StudioBank*, fmod::StudioEventDescription**, int, int*) = nullptr;

    fmod::Bool (*StudioEventDescriptionIsValid)(fmod::StudioEventDescription*) = nullptr;
    fmod::Result (*StudioEventDescriptionGetPath)(fmod::StudioEventDescription*, char*, int, int*) = nullptr;
    fmod::Result (*StudioEventDescriptionIs3D)(fmod::StudioEventDescription*, fmod::Bool*) = nullptr;
    fmod::Result (*StudioEventDescriptionIsOneshot)(fmod::StudioEventDescription*, fmod::Bool*) = nullptr;
    fmod::Result (*StudioEventDescriptionIsSnapshot)(fmod::StudioEventDescription*, fmod::Bool*) = nullptr;
    fmod::Result (*StudioEventDescriptionIsStream)(fmod::StudioEventDescription*, fmod::Bool*) = nullptr;
    fmod::Result (*StudioEventDescriptionGetLength)(fmod::StudioEventDescription*, int*) = nullptr;
    fmod::Result (*StudioEventDescriptionGetMinMaxDistance)(fmod::StudioEventDescription*, float*, float*) = nullptr;
    fmod::Result (*StudioEventDescriptionCreateInstance)(fmod::StudioEventDescription*, fmod::StudioEventInstance**) = nullptr;

    fmod::Bool (*StudioEventInstanceIsValid)(fmod::StudioEventInstance*) = nullptr;
    fmod::Result (*StudioEventInstanceStart)(fmod::StudioEventInstance*) = nullptr;
    fmod::Result (*StudioEventInstanceStop)(fmod::StudioEventInstance*, fmod::StudioStopMode) = nullptr;
    fmod::Result (*StudioEventInstanceRelease)(fmod::StudioEventInstance*) = nullptr;
    fmod::Result (*StudioEventInstanceSetPaused)(fmod::StudioEventInstance*, fmod::Bool) = nullptr;
    fmod::Result (*StudioEventInstanceGetPaused)(fmod::StudioEventInstance*, fmod::Bool*) = nullptr;
    fmod::Result (*StudioEventInstanceSetVolume)(fmod::StudioEventInstance*, float) = nullptr;
    fmod::Result (*StudioEventInstanceGetVolume)(fmod::StudioEventInstance*, float*, float*) = nullptr;
    fmod::Result (*StudioEventInstanceSetPitch)(fmod::StudioEventInstance*, float) = nullptr;
    fmod::Result (*StudioEventInstanceGetPitch)(fmod::StudioEventInstance*, float*, float*) = nullptr;
    fmod::Result (*StudioEventInstanceSetParameterByName)(fmod::StudioEventInstance*, const char*, float,
                                                          fmod::Bool) = nullptr;
    fmod::Result (*StudioEventInstanceSet3DAttributes)(fmod::StudioEventInstance*, const fmod::Attributes3D*) = nullptr;
    fmod::Result (*StudioEventInstanceGet3DAttributes)(fmod::StudioEventInstance*, fmod::Attributes3D*) = nullptr;
    fmod::Result (*StudioEventInstanceGetPlaybackState)(fmod::StudioEventInstance*, fmod::StudioPlaybackState*) = nullptr;

    fmod::Bool (*StudioBusIsValid)(fmod::StudioBus*) = nullptr;
    fmod::Result (*StudioBusLockChannelGroup)(fmod::StudioBus*) = nullptr;
    fmod::Result (*StudioBusUnlockChannelGroup)(fmod::StudioBus*) = nullptr;
    fmod::Result (*StudioBusGetChannelGroup)(fmod::StudioBus*, fmod::ChannelGroup**) = nullptr;

    fmod::Result (*SystemGetVersion)(fmod::System*, std::uint32_t*) = nullptr;
    fmod::Result (*SystemCreateSound)(fmod::System*, const char*, fmod::Mode,
                                      fmod::CreateSoundExInfo*, fmod::Sound**) = nullptr;
    fmod::Result (*SystemPlaySound)(fmod::System*, fmod::Sound*, fmod::ChannelGroup*,
                                    fmod::Bool, fmod::Channel**) = nullptr;

    fmod::Result (*SoundRelease)(fmod::Sound*) = nullptr;
    fmod::Result (*SoundGetLength)(fmod::Sound*, std::uint32_t*, fmod::TimeUnit) = nullptr;
    fmod::Result (*SoundGetFormat)(fmod::Sound*, fmod::SoundType*, fmod::SoundFormat*, int*, int*) = nullptr;

    fmod::Result (*ChannelStop)(fmod::Channel*) = nullptr;
    fmod::Result (*ChannelIsPlaying)(fmod::Channel*, fmod::Bool*) = nullptr;
    fmod::Result (*ChannelSetPaused)(fmod::Channel*, fmod::Bool) = nullptr;
    fmod::Result (*ChannelGetPaused)(fmod::Channel*, fmod::Bool*) = nullptr;
    fmod::Result (*ChannelSetVolume)(fmod::Channel*, float) = nullptr;
    fmod::Result (*ChannelGetVolume)(fmod::Channel*, float*) = nullptr;
    fmod::Result (*ChannelSetPitch)(fmod::Channel*, float) = nullptr;
    fmod::Result (*ChannelGetPitch)(fmod::Channel*, float*) = nullptr;
    fmod::Result (*ChannelSetMode)(fmod::Channel*, fmod::Mode) = nullptr;
    fmod::Result (*ChannelGetMode)(fmod::Channel*, fmod::Mode*) = nullptr;
    fmod::Result (*ChannelSet3DAttributes)(fmod::Channel*, const fmod::Vector*, const fmod::Vector*) = nullptr;
    fmod::Result (*ChannelGet3DAttributes)(fmod::Channel*, fmod::Vector*, fmod::Vector*) = nullptr;
    fmod::Result (*ChannelSet3DMinMaxDistance)(fmod::Channel*, float, float) = nullptr;
    fmod::Result (*ChannelGet3DMinMaxDistance)(fmod::Channel*, float*, float*) = nullptr;

private:
    bool m_resolved = false;
};

}  // namespace luautils::audio
