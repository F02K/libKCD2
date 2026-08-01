#pragma once

#include "AudioHandles.h"
#include "CryPakReader.h"
#include "FmodRuntime.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace luautils::audio {

struct AudioVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class AudioSpatialMode {
    None,
    Position,
    Entity,
};

struct AudioSpatialOptions {
    AudioSpatialMode mode = AudioSpatialMode::None;
    AudioVec3 position;
    AudioVec3 velocity;
    AudioVec3 offset;
    AudioVec3 forward{ 0.0f, 1.0f, 0.0f };
    AudioVec3 up{ 0.0f, 0.0f, 1.0f };
    std::uint32_t entityId = 0;
};

struct EventParameterValue {
    std::string name;
    float value = 0.0f;
};

struct EventPlayOptions {
    AudioSpatialOptions spatial;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool paused = false;
    bool ignoreSeekSpeed = false;
    std::vector<EventParameterValue> parameters;
};

struct SoundPlayOptions {
    AudioSpatialOptions spatial;
    std::string bus = "bus:/dieg/w_obj";
    float volume = 1.0f;
    float pitch = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    bool paused = false;
    bool loop = false;
};

struct AudioStatusInfo {
    bool apiResolved = false;
    bool ready = false;
    std::uint32_t initState = 0;
    std::uint32_t initResultCode = 0;
    std::uint32_t runtimeVersion = 0;
    std::uint64_t backendEpoch = 0;
    std::size_t bankCount = 0;
    std::size_t soundCount = 0;
    std::size_t eventCount = 0;
    std::size_t channelCount = 0;
    std::string error;
    std::string lastAsyncError;
};

struct BankInfo {
    std::uint64_t handle = 0;
    std::string path;
    std::uint32_t references = 0;
    std::uint32_t activeInstances = 0;
    int eventCount = 0;
    fmod::StudioLoadingState sampleLoadingState = fmod::StudioLoadingState::Unloaded;
};

struct EventInfo {
    std::string path;
    bool is3D = false;
    bool isOneshot = false;
    bool isSnapshot = false;
    bool isStream = false;
    int lengthMs = 0;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
};

struct EventStateInfo {
    std::string path;
    fmod::StudioPlaybackState playbackState = fmod::StudioPlaybackState::Stopped;
    bool paused = false;
    bool attached = false;
    float volume = 1.0f;
    float finalVolume = 1.0f;
    float pitch = 1.0f;
    float finalPitch = 1.0f;
};

struct SoundInfo {
    std::uint64_t handle = 0;
    std::string path;
    std::uint32_t references = 0;
    std::uint32_t activeInstances = 0;
    std::uint32_t lengthMs = 0;
    fmod::SoundType type = fmod::SoundType::Unknown;
    fmod::SoundFormat format = fmod::SoundFormat::None;
    int channels = 0;
    int bits = 0;
};

struct SoundStateInfo {
    std::string path;
    std::string bus;
    bool playing = false;
    bool paused = false;
    bool attached = false;
    bool is3D = false;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
};

class AudioManager
{
public:
    bool IsReady();
    AudioStatusInfo GetStatus();
    void Tick();

    std::vector<BankInfo> GetLoadedBanks();
    std::vector<SoundInfo> GetLoadedSounds();

    bool LoadBank(const char* path, bool loadSampleData, std::uint64_t& handle, std::string& error);
    bool UnloadBank(std::uint64_t handle, bool force, std::string& error);
    bool LoadBankSampleData(std::uint64_t handle, std::string& error);
    bool UnloadBankSampleData(std::uint64_t handle, std::string& error);
    bool GetBankInfo(std::uint64_t handle, BankInfo& info, std::string& error);
    bool GetBankEvents(std::uint64_t handle, std::vector<std::string>& events, std::string& error);

    bool GetEventInfo(const char* path, EventInfo& info, std::string& error);
    bool PlayEvent(const char* path, const EventPlayOptions& options,
                   std::uint64_t& handle, std::string& error);
    bool StopEvent(std::uint64_t handle, bool immediate, std::string& error);
    bool SetEventPaused(std::uint64_t handle, bool paused, std::string& error);
    bool SetEventParameter(std::uint64_t handle, const char* name, float value,
                           bool ignoreSeekSpeed, std::string& error);
    bool SetEventVolume(std::uint64_t handle, float volume, std::string& error);
    bool SetEventPitch(std::uint64_t handle, float pitch, std::string& error);
    bool SetEventPosition(std::uint64_t handle, const AudioVec3& position,
                          const AudioVec3& velocity, std::string& error);
    bool AttachEventToEntity(std::uint64_t handle, std::uint32_t entityId,
                             const AudioVec3& offset, std::string& error);
    bool DetachEvent(std::uint64_t handle, std::string& error);
    bool GetEventState(std::uint64_t handle, EventStateInfo& state, std::string& error);

    bool LoadSound(const char* path, std::uint64_t& handle, std::string& error);
    bool UnloadSound(std::uint64_t handle, bool force, std::string& error);
    bool GetSoundInfo(std::uint64_t handle, SoundInfo& info, std::string& error);
    bool PlaySound(std::uint64_t soundHandle, const SoundPlayOptions& options,
                   std::uint64_t& instanceHandle, std::string& error);
    bool StopSound(std::uint64_t handle, std::string& error);
    bool SetSoundPaused(std::uint64_t handle, bool paused, std::string& error);
    bool SetSoundVolume(std::uint64_t handle, float volume, std::string& error);
    bool SetSoundPitch(std::uint64_t handle, float pitch, std::string& error);
    bool SetSoundLooping(std::uint64_t handle, bool loop, std::string& error);
    bool SetSoundPosition(std::uint64_t handle, const AudioVec3& position,
                          const AudioVec3& velocity, std::string& error);
    bool AttachSoundToEntity(std::uint64_t handle, std::uint32_t entityId,
                             const AudioVec3& offset, std::string& error);
    bool DetachSound(std::uint64_t handle, std::string& error);
    bool GetSoundState(std::uint64_t handle, SoundStateInfo& state, std::string& error);

private:
    struct Attachment {
        bool attached = false;
        std::uint32_t entityId = 0;
        AudioVec3 offset;
        AudioVec3 previousPosition;
        bool hasPreviousPosition = false;
    };

    struct BankResource {
        CryPakPath path;
        fmod::StudioBank* bank = nullptr;
        std::uint32_t references = 1;
        std::uint32_t activeInstances = 0;
        std::vector<std::string> events;
    };

    struct SoundResource {
        CryPakPath path;
        fmod::Sound* sound = nullptr;
        std::uint32_t references = 1;
        std::uint32_t activeInstances = 0;
        std::uint32_t lengthMs = 0;
        fmod::SoundType type = fmod::SoundType::Unknown;
        fmod::SoundFormat format = fmod::SoundFormat::None;
        int channels = 0;
        int bits = 0;
    };

    struct EventInstance {
        fmod::StudioEventInstance* instance = nullptr;
        std::string path;
        std::uint64_t ownerBank = 0;
        bool is3D = false;
        Attachment attachment;
        fmod::Attributes3D attributes{};
    };

    struct ChannelInstance {
        fmod::Channel* channel = nullptr;
        std::uint64_t ownerSound = 0;
        std::string path;
        std::string routeKey;
        bool is3D = false;
        bool cleanupRequested = false;
        Attachment attachment;
        fmod::Vector position{};
        fmod::Vector velocity{};
    };

    struct BusRoute {
        std::string path;
        fmod::StudioBus* bus = nullptr;
        fmod::ChannelGroup* group = nullptr;
        std::uint32_t activeChannels = 0;
        bool locked = false;
    };

    bool EnsureReady(std::string& error);
    void RefreshBackend();
    void ResetForBackendChange();

    bool ResolveEvent(const char* path, fmod::StudioEventDescription*& description,
                      EventInfo& info, std::string& error);
    bool ReadEventPath(fmod::StudioEventDescription* description,
                       std::string& path, std::string& error) const;
    bool ReadEventInfo(fmod::StudioEventDescription* description,
                       const std::string& path, EventInfo& info, std::string& error) const;
    bool EnumerateBankEvents(fmod::StudioBank* bank, std::vector<std::string>& events,
                             std::string& error) const;

    bool AcquireRoute(const std::string& path, const std::string& key,
                      BusRoute*& route, std::string& error);
    void ReleaseRoute(const std::string& key);

    bool BuildEntityAttributes(Attachment& attachment, float deltaTime,
                               fmod::Attributes3D& attributes, std::string& error) const;
    bool BuildEntityChannelPosition(Attachment& attachment, float deltaTime,
                                    fmod::Vector& position, fmod::Vector& velocity,
                                    std::string& error) const;

    bool RetireEvent(std::uint64_t handle, bool stopInstance, bool releaseInstance);
    bool RetireChannel(std::uint64_t handle, bool stopChannel);
    bool ForceRetireBankEvents(std::uint64_t bankHandle, std::string& error);
    bool ForceRetireSoundChannels(std::uint64_t soundHandle, std::string& error);

    FmodApi m_api;
    FmodRuntime m_runtime;
    RuntimeStatus m_runtimeStatus;
    BackendIdentity m_activeIdentity;
    bool m_hasActiveBackend = false;
    std::uint64_t m_backendEpoch = 0;
    std::string m_lastAsyncError;

    AudioHandlePool<AudioHandleKind::Bank, BankResource> m_banks;
    AudioHandlePool<AudioHandleKind::Sound, SoundResource> m_sounds;
    AudioHandlePool<AudioHandleKind::Event, EventInstance> m_events;
    AudioHandlePool<AudioHandleKind::Channel, ChannelInstance> m_channels;

    std::unordered_map<std::string, std::uint64_t> m_bankPaths;
    std::unordered_map<std::string, std::uint64_t> m_soundPaths;
    std::unordered_map<std::string, std::uint64_t> m_eventOwners;
    std::unordered_map<std::string, BusRoute> m_routes;
};

inline AudioManager g_audioManager;

}  // namespace luautils::audio
