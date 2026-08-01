#include "AudioManager.h"

#include "Cry_Math.h"
#include "Offsets/vtables/IEntity.h"
#include "Offsets/vtables/IEntitySystem.h"
#include "Offsets/vtables/ITimer.h"
#include "crysystem/SSystemGlobalEnvironment.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace luautils::audio {

namespace {

constexpr std::size_t kMaxSoundBytes = 256u * 1024u * 1024u;

std::string LowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool IsFinite(float value)
{
    return std::isfinite(value);
}

bool IsFinite(const AudioVec3& value)
{
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

float LengthSquared(const AudioVec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

float Dot(const AudioVec3& left, const AudioVec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

bool Normalize(AudioVec3& value)
{
    const float lengthSquared = LengthSquared(value);
    if (!IsFinite(lengthSquared) || lengthSquared <= 1.0e-8f)
        return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

fmod::Vector ToFmod(const AudioVec3& value)
{
    return { value.x, value.z, value.y };
}

AudioVec3 ToAudioVec(const Vec3& value)
{
    return { value.x, value.y, value.z };
}

bool NormalizeNamedPath(const char* input, const char* prefix,
                        std::string& path, std::string& key, std::string& error)
{
    path.clear();
    key.clear();
    if (!input || !*input) {
        error = "FMOD path is empty";
        return false;
    }
    path = input;
    if (path.size() >= 0x800) {
        error = "FMOD path exceeds the supported limit";
        return false;
    }
    for (unsigned char c : path) {
        if (c < 0x20) {
            error = "FMOD path contains a control character";
            return false;
        }
    }
    key = LowerAscii(path);
    if (key.rfind(prefix, 0) != 0) {
        error = std::string("FMOD path must begin with ") + prefix;
        path.clear();
        key.clear();
        return false;
    }
    error.clear();
    return true;
}

bool ValidateSpatialOrientation(AudioVec3& forward, AudioVec3& up, std::string& error)
{
    if (!IsFinite(forward) || !IsFinite(up) || !Normalize(forward) || !Normalize(up)) {
        error = "orientation vectors must be finite and nonzero";
        return false;
    }
    if (std::fabs(Dot(forward, up)) >= 0.999f) {
        error = "orientation vectors must not be collinear";
        return false;
    }
    return true;
}

}  // namespace

bool AudioManager::IsReady()
{
    RefreshBackend();
    return m_runtimeStatus.ready;
}

AudioStatusInfo AudioManager::GetStatus()
{
    RefreshBackend();
    AudioStatusInfo info;
    info.apiResolved = m_runtimeStatus.apiResolved;
    info.ready = m_runtimeStatus.ready;
    info.initState = m_runtimeStatus.initState;
    info.initResultCode = m_runtimeStatus.initResultCode;
    info.runtimeVersion = m_runtimeStatus.runtimeVersion;
    info.backendEpoch = m_backendEpoch;
    info.bankCount = m_banks.Size();
    info.soundCount = m_sounds.Size();
    info.eventCount = m_events.Size();
    info.channelCount = m_channels.Size();
    info.error = m_runtimeStatus.error;
    info.lastAsyncError = m_lastAsyncError;
    return info;
}

void AudioManager::RefreshBackend()
{
    RuntimeStatus next = m_runtime.Refresh(m_api);
    if (!next.ready) {
        if (m_hasActiveBackend) {
            ResetForBackendChange();
            m_hasActiveBackend = false;
            m_activeIdentity = {};
            ++m_backendEpoch;
        }
        m_runtimeStatus = std::move(next);
        return;
    }

    if (!m_hasActiveBackend) {
        m_activeIdentity = next.identity;
        m_hasActiveBackend = true;
        ++m_backendEpoch;
    } else if (next.identity != m_activeIdentity) {
        ResetForBackendChange();
        m_activeIdentity = next.identity;
        ++m_backendEpoch;
    }
    m_runtimeStatus = std::move(next);
}

void AudioManager::ResetForBackendChange()
{
    m_banks.Clear();
    m_sounds.Clear();
    m_events.Clear();
    m_channels.Clear();
    m_bankPaths.clear();
    m_soundPaths.clear();
    m_eventOwners.clear();
    m_routes.clear();
    m_lastAsyncError = "FMOD backend changed; all AudioManager handles were invalidated";
}

bool AudioManager::EnsureReady(std::string& error)
{
    RefreshBackend();
    if (m_runtimeStatus.ready)
        return true;
    error = m_runtimeStatus.error.empty() ? "AudioManager is not ready" : m_runtimeStatus.error;
    return false;
}

std::vector<BankInfo> AudioManager::GetLoadedBanks()
{
    RefreshBackend();
    std::vector<BankInfo> result;
    for (std::uint64_t handle : m_banks.Handles()) {
        BankResource* resource = m_banks.Get(handle);
        if (!resource)
            continue;
        BankInfo info;
        info.handle = handle;
        info.path = resource->path.display;
        info.references = resource->references;
        info.activeInstances = resource->activeInstances;
        info.eventCount = static_cast<int>(resource->events.size());
        if (m_runtimeStatus.ready && resource->bank && m_api.StudioBankIsValid(resource->bank))
            m_api.StudioBankGetSampleLoadingState(resource->bank, &info.sampleLoadingState);
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<SoundInfo> AudioManager::GetLoadedSounds()
{
    RefreshBackend();
    std::vector<SoundInfo> result;
    for (std::uint64_t handle : m_sounds.Handles()) {
        SoundResource* resource = m_sounds.Get(handle);
        if (!resource)
            continue;
        SoundInfo info;
        info.handle = handle;
        info.path = resource->path.display;
        info.references = resource->references;
        info.activeInstances = resource->activeInstances;
        info.lengthMs = resource->lengthMs;
        info.type = resource->type;
        info.format = resource->format;
        info.channels = resource->channels;
        info.bits = resource->bits;
        result.push_back(std::move(info));
    }
    return result;
}

bool AudioManager::ReadEventPath(fmod::StudioEventDescription* description,
                                 std::string& path, std::string& error) const
{
    std::vector<char> buffer(256, '\0');
    for (;;) {
        int retrieved = 0;
        fmod::Result result = m_api.StudioEventDescriptionGetPath(
            description, buffer.data(), static_cast<int>(buffer.size()), &retrieved);
        if (result == fmod::kOk) {
            path.assign(buffer.data());
            if (path.empty()) {
                error = "FMOD event has an empty path";
                return false;
            }
            return true;
        }
        if (result != fmod::kErrTruncated || retrieved <= static_cast<int>(buffer.size())) {
            error = FmodApi::FormatError("FMOD_Studio_EventDescription_GetPath", result);
            return false;
        }
        buffer.assign(static_cast<std::size_t>(retrieved), '\0');
    }
}

bool AudioManager::ReadEventInfo(fmod::StudioEventDescription* description,
                                 const std::string& path, EventInfo& info,
                                 std::string& error) const
{
    info = {};
    info.path = path;
    fmod::Bool value = 0;
    fmod::Result result = m_api.StudioEventDescriptionIs3D(description, &value);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_Is3D", result);
        return false;
    }
    info.is3D = value != 0;

    result = m_api.StudioEventDescriptionIsOneshot(description, &value);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_IsOneshot", result);
        return false;
    }
    info.isOneshot = value != 0;

    result = m_api.StudioEventDescriptionIsSnapshot(description, &value);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_IsSnapshot", result);
        return false;
    }
    info.isSnapshot = value != 0;

    result = m_api.StudioEventDescriptionIsStream(description, &value);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_IsStream", result);
        return false;
    }
    info.isStream = value != 0;

    result = m_api.StudioEventDescriptionGetLength(description, &info.lengthMs);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_GetLength", result);
        return false;
    }
    if (info.is3D) {
        result = m_api.StudioEventDescriptionGetMinMaxDistance(
            description, &info.minDistance, &info.maxDistance);
        if (result != fmod::kOk) {
            error = FmodApi::FormatError("FMOD_Studio_EventDescription_GetMinMaxDistance", result);
            return false;
        }
    }
    return true;
}

bool AudioManager::ResolveEvent(const char* path, fmod::StudioEventDescription*& description,
                                EventInfo& info, std::string& error)
{
    std::string display;
    std::string key;
    if (!NormalizeNamedPath(path, "event:/", display, key, error))
        return false;

    description = nullptr;
    fmod::Result result = m_api.StudioSystemGetEvent(
        m_runtimeStatus.identity.studio, display.c_str(), &description);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_System_GetEvent", result);
        return false;
    }
    if (!description || !m_api.StudioEventDescriptionIsValid(description)) {
        error = "FMOD event description is invalid";
        return false;
    }
    return ReadEventInfo(description, display, info, error);
}

bool AudioManager::EnumerateBankEvents(fmod::StudioBank* bank,
                                       std::vector<std::string>& events,
                                       std::string& error) const
{
    events.clear();
    int count = 0;
    fmod::Result result = m_api.StudioBankGetEventCount(bank, &count);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_GetEventCount", result);
        return false;
    }
    if (count <= 0)
        return true;

    std::vector<fmod::StudioEventDescription*> descriptions(static_cast<std::size_t>(count));
    int actual = 0;
    result = m_api.StudioBankGetEventList(bank, descriptions.data(), count, &actual);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_GetEventList", result);
        return false;
    }
    events.reserve(static_cast<std::size_t>(actual));
    for (int index = 0; index < actual; ++index) {
        fmod::StudioEventDescription* description = descriptions[static_cast<std::size_t>(index)];
        if (!description || !m_api.StudioEventDescriptionIsValid(description)) {
            error = "bank returned an invalid event description";
            return false;
        }
        std::string eventPath;
        if (!ReadEventPath(description, eventPath, error))
            return false;
        events.push_back(std::move(eventPath));
    }
    return true;
}

bool AudioManager::LoadBank(const char* path, bool loadSampleData,
                            std::uint64_t& handle, std::string& error)
{
    handle = 0;
    if (!EnsureReady(error))
        return false;

    CryPakPath normalized;
    if (!CryPakReader::Normalize(path, CryPakAssetKind::Bank, normalized, error))
        return false;

    auto existing = m_bankPaths.find(normalized.key);
    if (existing != m_bankPaths.end()) {
        if (BankResource* resource = m_banks.Get(existing->second)) {
            ++resource->references;
            handle = existing->second;
            if (loadSampleData) {
                fmod::Result result = m_api.StudioBankLoadSampleData(resource->bank);
                if (result != fmod::kOk) {
                    --resource->references;
                    handle = 0;
                    error = FmodApi::FormatError("FMOD_Studio_Bank_LoadSampleData", result);
                    return false;
                }
            }
            return true;
        }
        m_bankPaths.erase(existing);
    }

    std::vector<std::uint8_t> data;
    if (!CryPakReader::ReadAll(normalized,
            static_cast<std::size_t>(std::numeric_limits<int>::max()), data, error))
        return false;

    fmod::StudioBank* bank = nullptr;
    fmod::Result result = m_api.StudioSystemLoadBankMemory(
        m_runtimeStatus.identity.studio, reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()), fmod::kStudioLoadMemory,
        fmod::kStudioLoadBankNormal, &bank);
    data.clear();
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_System_LoadBankMemory", result);
        return false;
    }
    if (!bank || !m_api.StudioBankIsValid(bank)) {
        error = "FMOD returned an invalid bank";
        return false;
    }

    std::vector<std::string> events;
    if (!EnumerateBankEvents(bank, events, error)) {
        m_api.StudioBankUnload(bank);
        return false;
    }
    for (const std::string& eventPath : events) {
        if (m_eventOwners.find(LowerAscii(eventPath)) != m_eventOwners.end()) {
            m_api.StudioBankUnload(bank);
            error = "injected event path is already owned by another loaded bank: " + eventPath;
            return false;
        }
    }
    if (loadSampleData) {
        result = m_api.StudioBankLoadSampleData(bank);
        if (result != fmod::kOk) {
            m_api.StudioBankUnload(bank);
            error = FmodApi::FormatError("FMOD_Studio_Bank_LoadSampleData", result);
            return false;
        }
    }

    BankResource resource;
    resource.path = normalized;
    resource.bank = bank;
    resource.events = std::move(events);
    handle = m_banks.Emplace(std::move(resource));
    m_bankPaths[normalized.key] = handle;
    BankResource* stored = m_banks.Get(handle);
    for (const std::string& eventPath : stored->events)
        m_eventOwners[LowerAscii(eventPath)] = handle;
    return true;
}

bool AudioManager::ForceRetireBankEvents(std::uint64_t bankHandle, std::string& error)
{
    bool success = true;
    for (std::uint64_t eventHandle : m_events.Handles()) {
        EventInstance* event = m_events.Get(eventHandle);
        if (!event || event->ownerBank != bankHandle)
            continue;
        if (!RetireEvent(eventHandle, true, true))
            success = false;
    }
    if (!success)
        error = m_lastAsyncError.empty()
            ? "could not retire every bank event instance"
            : m_lastAsyncError;
    return success;
}

bool AudioManager::UnloadBank(std::uint64_t handle, bool force, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    BankResource* resource = m_banks.Get(handle);
    if (!resource) {
        error = "invalid or stale bank handle";
        return false;
    }
    if (!force && resource->references > 1) {
        --resource->references;
        return true;
    }
    if (!force && resource->activeInstances != 0) {
        error = "bank still has active event instances";
        return false;
    }
    if (force) {
        if (!ForceRetireBankEvents(handle, error))
            return false;
        fmod::Result flushResult = m_api.StudioSystemFlushCommands(m_runtimeStatus.identity.studio);
        if (flushResult != fmod::kOk) {
            error = FmodApi::FormatError("FMOD_Studio_System_FlushCommands", flushResult);
            return false;
        }
    }

    fmod::Result result = m_api.StudioBankUnload(resource->bank);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_Unload", result);
        return false;
    }

    for (const std::string& eventPath : resource->events) {
        auto owner = m_eventOwners.find(LowerAscii(eventPath));
        if (owner != m_eventOwners.end() && owner->second == handle)
            m_eventOwners.erase(owner);
    }
    m_bankPaths.erase(resource->path.key);
    m_banks.Erase(handle);
    return true;
}

bool AudioManager::LoadBankSampleData(std::uint64_t handle, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    BankResource* resource = m_banks.Get(handle);
    if (!resource) {
        error = "invalid or stale bank handle";
        return false;
    }
    fmod::Result result = m_api.StudioBankLoadSampleData(resource->bank);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_LoadSampleData", result);
        return false;
    }
    return true;
}

bool AudioManager::UnloadBankSampleData(std::uint64_t handle, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    BankResource* resource = m_banks.Get(handle);
    if (!resource) {
        error = "invalid or stale bank handle";
        return false;
    }
    fmod::Result result = m_api.StudioBankUnloadSampleData(resource->bank);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_UnloadSampleData", result);
        return false;
    }
    return true;
}

bool AudioManager::GetBankInfo(std::uint64_t handle, BankInfo& info, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    BankResource* resource = m_banks.Get(handle);
    if (!resource) {
        error = "invalid or stale bank handle";
        return false;
    }
    info = {};
    info.handle = handle;
    info.path = resource->path.display;
    info.references = resource->references;
    info.activeInstances = resource->activeInstances;
    info.eventCount = static_cast<int>(resource->events.size());
    fmod::Result result = m_api.StudioBankGetSampleLoadingState(
        resource->bank, &info.sampleLoadingState);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bank_GetSampleLoadingState", result);
        return false;
    }
    return true;
}

bool AudioManager::GetBankEvents(std::uint64_t handle, std::vector<std::string>& events,
                                 std::string& error)
{
    if (!EnsureReady(error))
        return false;
    BankResource* resource = m_banks.Get(handle);
    if (!resource) {
        error = "invalid or stale bank handle";
        return false;
    }
    events = resource->events;
    return true;
}

bool AudioManager::GetEventInfo(const char* path, EventInfo& info, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    fmod::StudioEventDescription* description = nullptr;
    return ResolveEvent(path, description, info, error);
}

bool AudioManager::PlayEvent(const char* path, const EventPlayOptions& options,
                             std::uint64_t& handle, std::string& error)
{
    handle = 0;
    if (!EnsureReady(error))
        return false;
    if (!IsFinite(options.volume) || options.volume < 0.0f ||
        !IsFinite(options.pitch) || options.pitch <= 0.0f) {
        error = "event volume and pitch are invalid";
        return false;
    }
    for (const EventParameterValue& parameter : options.parameters) {
        if (parameter.name.empty() || !IsFinite(parameter.value)) {
            error = "event parameters require a name and finite value";
            return false;
        }
    }

    fmod::StudioEventDescription* description = nullptr;
    EventInfo info;
    if (!ResolveEvent(path, description, info, error))
        return false;
    if (info.is3D && options.spatial.mode == AudioSpatialMode::None) {
        error = "3D event requires a position or entity";
        return false;
    }
    if (!info.is3D && options.spatial.mode != AudioSpatialMode::None) {
        error = "2D event does not accept spatial options";
        return false;
    }

    EventInstance record;
    record.path = info.path;
    record.is3D = info.is3D;
    if (info.is3D) {
        if (options.spatial.mode == AudioSpatialMode::Position) {
            if (!IsFinite(options.spatial.position) || !IsFinite(options.spatial.velocity)) {
                error = "event position and velocity must be finite";
                return false;
            }
            AudioVec3 forward = options.spatial.forward;
            AudioVec3 up = options.spatial.up;
            if (!ValidateSpatialOrientation(forward, up, error))
                return false;
            record.attributes.position = ToFmod(options.spatial.position);
            record.attributes.velocity = ToFmod(options.spatial.velocity);
            record.attributes.forward = ToFmod(forward);
            record.attributes.up = ToFmod(up);
        } else {
            if (options.spatial.entityId == 0 || !IsFinite(options.spatial.offset)) {
                error = "event attachment requires a valid entity and finite offset";
                return false;
            }
            record.attachment.attached = true;
            record.attachment.entityId = options.spatial.entityId;
            record.attachment.offset = options.spatial.offset;
            if (!BuildEntityAttributes(record.attachment, 0.0f, record.attributes, error))
                return false;
        }
    }

    fmod::StudioEventInstance* instance = nullptr;
    fmod::Result result = m_api.StudioEventDescriptionCreateInstance(description, &instance);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventDescription_CreateInstance", result);
        return false;
    }
    record.instance = instance;

    const auto fail = [&](const char* operation, fmod::Result failure) {
        if (instance && m_api.StudioEventInstanceIsValid(instance))
            m_api.StudioEventInstanceRelease(instance);
        error = FmodApi::FormatError(operation, failure);
        return false;
    };

    if (info.is3D) {
        result = m_api.StudioEventInstanceSet3DAttributes(instance, &record.attributes);
        if (result != fmod::kOk)
            return fail("FMOD_Studio_EventInstance_Set3DAttributes", result);
    }
    result = m_api.StudioEventInstanceSetVolume(instance, options.volume);
    if (result != fmod::kOk)
        return fail("FMOD_Studio_EventInstance_SetVolume", result);
    result = m_api.StudioEventInstanceSetPitch(instance, options.pitch);
    if (result != fmod::kOk)
        return fail("FMOD_Studio_EventInstance_SetPitch", result);
    for (const EventParameterValue& parameter : options.parameters) {
        result = m_api.StudioEventInstanceSetParameterByName(
            instance, parameter.name.c_str(), parameter.value,
            options.ignoreSeekSpeed ? 1 : 0);
        if (result != fmod::kOk)
            return fail("FMOD_Studio_EventInstance_SetParameterByName", result);
    }
    result = m_api.StudioEventInstanceSetPaused(instance, options.paused ? 1 : 0);
    if (result != fmod::kOk)
        return fail("FMOD_Studio_EventInstance_SetPaused", result);
    result = m_api.StudioEventInstanceStart(instance);
    if (result != fmod::kOk)
        return fail("FMOD_Studio_EventInstance_Start", result);

    auto owner = m_eventOwners.find(LowerAscii(info.path));
    if (owner != m_eventOwners.end() && m_banks.Get(owner->second))
        record.ownerBank = owner->second;
    handle = m_events.Emplace(std::move(record));
    if (EventInstance* stored = m_events.Get(handle); stored && stored->ownerBank) {
        if (BankResource* bank = m_banks.Get(stored->ownerBank))
            ++bank->activeInstances;
    }
    return true;
}

bool AudioManager::StopEvent(std::uint64_t handle, bool immediate, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    fmod::Result result = m_api.StudioEventInstanceStop(
        event->instance, immediate ? fmod::StudioStopMode::Immediate
                                   : fmod::StudioStopMode::AllowFadeout);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_Stop", result);
        return false;
    }
    return true;
}

bool AudioManager::SetEventPaused(std::uint64_t handle, bool paused, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    fmod::Result result = m_api.StudioEventInstanceSetPaused(event->instance, paused ? 1 : 0);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_SetPaused", result);
        return false;
    }
    return true;
}

bool AudioManager::SetEventParameter(std::uint64_t handle, const char* name, float value,
                                     bool ignoreSeekSpeed, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!name || !*name || !IsFinite(value)) {
        error = "parameter name and value are invalid";
        return false;
    }
    fmod::Result result = m_api.StudioEventInstanceSetParameterByName(
        event->instance, name, value, ignoreSeekSpeed ? 1 : 0);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_SetParameterByName", result);
        return false;
    }
    return true;
}

bool AudioManager::SetEventVolume(std::uint64_t handle, float volume, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!IsFinite(volume) || volume < 0.0f) {
        error = "volume must be finite and nonnegative";
        return false;
    }
    fmod::Result result = m_api.StudioEventInstanceSetVolume(event->instance, volume);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_SetVolume", result);
        return false;
    }
    return true;
}

bool AudioManager::SetEventPitch(std::uint64_t handle, float pitch, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!IsFinite(pitch) || pitch <= 0.0f) {
        error = "pitch must be finite and positive";
        return false;
    }
    fmod::Result result = m_api.StudioEventInstanceSetPitch(event->instance, pitch);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_SetPitch", result);
        return false;
    }
    return true;
}

bool AudioManager::SetEventPosition(std::uint64_t handle, const AudioVec3& position,
                                    const AudioVec3& velocity, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!event->is3D) {
        error = "event is not 3D";
        return false;
    }
    if (event->attachment.attached) {
        error = "detach the event before setting a static position";
        return false;
    }
    if (!IsFinite(position) || !IsFinite(velocity)) {
        error = "position and velocity must be finite";
        return false;
    }
    fmod::Attributes3D attributes = event->attributes;
    attributes.position = ToFmod(position);
    attributes.velocity = ToFmod(velocity);
    fmod::Result result = m_api.StudioEventInstanceSet3DAttributes(event->instance, &attributes);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_Set3DAttributes", result);
        return false;
    }
    event->attributes = attributes;
    return true;
}

bool AudioManager::AttachEventToEntity(std::uint64_t handle, std::uint32_t entityId,
                                       const AudioVec3& offset, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!event->is3D) {
        error = "event is not 3D";
        return false;
    }
    if (entityId == 0 || !IsFinite(offset)) {
        error = "attachment requires a valid entity and finite offset";
        return false;
    }

    Attachment attachment;
    attachment.attached = true;
    attachment.entityId = entityId;
    attachment.offset = offset;
    fmod::Attributes3D attributes{};
    if (!BuildEntityAttributes(attachment, 0.0f, attributes, error))
        return false;
    fmod::Result result = m_api.StudioEventInstanceSet3DAttributes(event->instance, &attributes);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_Set3DAttributes", result);
        return false;
    }
    event->attachment = attachment;
    event->attributes = attributes;
    return true;
}

bool AudioManager::DetachEvent(std::uint64_t handle, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    if (!event->is3D) {
        error = "event is not 3D";
        return false;
    }
    if (!event->attachment.attached)
        return true;

    fmod::Attributes3D attributes = event->attributes;
    attributes.velocity = {};
    fmod::Result result = m_api.StudioEventInstanceSet3DAttributes(event->instance, &attributes);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_Set3DAttributes", result);
        return false;
    }
    event->attachment = {};
    event->attributes = attributes;
    return true;
}

bool AudioManager::GetEventState(std::uint64_t handle, EventStateInfo& state,
                                 std::string& error)
{
    if (!EnsureReady(error))
        return false;
    EventInstance* event = m_events.Get(handle);
    if (!event || !event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
        error = "invalid or stale event handle";
        return false;
    }
    state = {};
    state.path = event->path;
    state.attached = event->attachment.attached;

    fmod::Result result = m_api.StudioEventInstanceGetPlaybackState(
        event->instance, &state.playbackState);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_GetPlaybackState", result);
        return false;
    }
    fmod::Bool paused = 0;
    result = m_api.StudioEventInstanceGetPaused(event->instance, &paused);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_GetPaused", result);
        return false;
    }
    state.paused = paused != 0;
    result = m_api.StudioEventInstanceGetVolume(
        event->instance, &state.volume, &state.finalVolume);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_GetVolume", result);
        return false;
    }
    result = m_api.StudioEventInstanceGetPitch(
        event->instance, &state.pitch, &state.finalPitch);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_EventInstance_GetPitch", result);
        return false;
    }
    return true;
}

bool AudioManager::LoadSound(const char* path, std::uint64_t& handle, std::string& error)
{
    handle = 0;
    if (!EnsureReady(error))
        return false;

    CryPakPath normalized;
    if (!CryPakReader::Normalize(path, CryPakAssetKind::Sound, normalized, error))
        return false;
    auto existing = m_soundPaths.find(normalized.key);
    if (existing != m_soundPaths.end()) {
        if (SoundResource* resource = m_sounds.Get(existing->second)) {
            ++resource->references;
            handle = existing->second;
            return true;
        }
        m_soundPaths.erase(existing);
    }

    std::vector<std::uint8_t> data;
    if (!CryPakReader::ReadAll(normalized, kMaxSoundBytes, data, error))
        return false;

    fmod::CreateSoundExInfo exInfo{};
    exInfo.cbsize = sizeof(exInfo);
    exInfo.length = static_cast<std::uint32_t>(data.size());
    fmod::Sound* sound = nullptr;
    fmod::Result result = m_api.SystemCreateSound(
        m_runtimeStatus.identity.core, reinterpret_cast<const char*>(data.data()),
        fmod::kOpenMemory | fmod::kCreateSample, &exInfo, &sound);
    data.clear();
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_System_CreateSound", result);
        return false;
    }
    if (!sound) {
        error = "FMOD returned an invalid sound";
        return false;
    }

    SoundResource resource;
    resource.path = normalized;
    resource.sound = sound;
    result = m_api.SoundGetLength(sound, &resource.lengthMs, fmod::kTimeUnitMs);
    if (result != fmod::kOk) {
        m_api.SoundRelease(sound);
        error = FmodApi::FormatError("FMOD_Sound_GetLength", result);
        return false;
    }
    result = m_api.SoundGetFormat(sound, &resource.type, &resource.format,
                                  &resource.channels, &resource.bits);
    if (result != fmod::kOk) {
        m_api.SoundRelease(sound);
        error = FmodApi::FormatError("FMOD_Sound_GetFormat", result);
        return false;
    }

    handle = m_sounds.Emplace(std::move(resource));
    m_soundPaths[normalized.key] = handle;
    return true;
}

bool AudioManager::ForceRetireSoundChannels(std::uint64_t soundHandle, std::string& error)
{
    bool success = true;
    for (std::uint64_t channelHandle : m_channels.Handles()) {
        ChannelInstance* channel = m_channels.Get(channelHandle);
        if (channel && channel->ownerSound == soundHandle &&
            !RetireChannel(channelHandle, true))
            success = false;
    }
    if (!success)
        error = m_lastAsyncError.empty()
            ? "could not retire every sound channel instance"
            : m_lastAsyncError;
    return success;
}

bool AudioManager::UnloadSound(std::uint64_t handle, bool force, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    SoundResource* resource = m_sounds.Get(handle);
    if (!resource) {
        error = "invalid or stale sound handle";
        return false;
    }
    if (!force && resource->references > 1) {
        --resource->references;
        return true;
    }
    if (!force && resource->activeInstances != 0) {
        error = "sound still has active channel instances";
        return false;
    }
    if (force && !ForceRetireSoundChannels(handle, error))
        return false;

    fmod::Result result = m_api.SoundRelease(resource->sound);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Sound_Release", result);
        return false;
    }
    m_soundPaths.erase(resource->path.key);
    m_sounds.Erase(handle);
    return true;
}

bool AudioManager::GetSoundInfo(std::uint64_t handle, SoundInfo& info, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    SoundResource* resource = m_sounds.Get(handle);
    if (!resource) {
        error = "invalid or stale sound handle";
        return false;
    }
    info = {};
    info.handle = handle;
    info.path = resource->path.display;
    info.references = resource->references;
    info.activeInstances = resource->activeInstances;
    info.lengthMs = resource->lengthMs;
    info.type = resource->type;
    info.format = resource->format;
    info.channels = resource->channels;
    info.bits = resource->bits;
    return true;
}

bool AudioManager::AcquireRoute(const std::string& path, const std::string& key,
                                BusRoute*& route, std::string& error)
{
    BusRoute& stored = m_routes[key];
    if (stored.path.empty())
        stored.path = path;
    if (stored.locked && stored.bus && stored.group && m_api.StudioBusIsValid(stored.bus)) {
        route = &stored;
        return true;
    }
    if (stored.activeChannels != 0) {
        error = "audio bus route lost its channel group while active";
        return false;
    }
    if (stored.locked) {
        if (!stored.bus || !m_api.StudioBusIsValid(stored.bus)) {
            error = "locked audio bus route became invalid";
            return false;
        }
        fmod::Result unlockResult = m_api.StudioBusUnlockChannelGroup(stored.bus);
        if (unlockResult != fmod::kOk) {
            error = FmodApi::FormatError(
                "FMOD_Studio_Bus_UnlockChannelGroup", unlockResult);
            return false;
        }
    }

    stored.bus = nullptr;
    stored.group = nullptr;
    stored.locked = false;
    fmod::Result result = m_api.StudioSystemGetBus(
        m_runtimeStatus.identity.studio, path.c_str(), &stored.bus);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_System_GetBus", result);
        return false;
    }
    if (!stored.bus || !m_api.StudioBusIsValid(stored.bus)) {
        error = "FMOD bus is invalid: " + path;
        return false;
    }
    result = m_api.StudioBusLockChannelGroup(stored.bus);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_Bus_LockChannelGroup", result);
        return false;
    }
    stored.locked = true;
    result = m_api.StudioSystemFlushCommands(m_runtimeStatus.identity.studio);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Studio_System_FlushCommands", result);
        fmod::Result unlockResult = m_api.StudioBusUnlockChannelGroup(stored.bus);
        if (unlockResult == fmod::kOk) {
            stored.locked = false;
            stored.bus = nullptr;
        } else {
            error += "; cleanup: " + FmodApi::FormatError(
                "FMOD_Studio_Bus_UnlockChannelGroup", unlockResult);
        }
        return false;
    }
    result = m_api.StudioBusGetChannelGroup(stored.bus, &stored.group);
    if (result != fmod::kOk || !stored.group) {
        error = result == fmod::kOk
            ? "FMOD bus returned a null channel group"
            : FmodApi::FormatError("FMOD_Studio_Bus_GetChannelGroup", result);
        fmod::Result unlockResult = m_api.StudioBusUnlockChannelGroup(stored.bus);
        if (unlockResult == fmod::kOk) {
            stored.locked = false;
            stored.bus = nullptr;
        } else {
            error += "; cleanup: " + FmodApi::FormatError(
                "FMOD_Studio_Bus_UnlockChannelGroup", unlockResult);
        }
        stored.group = nullptr;
        return false;
    }
    route = &stored;
    return true;
}

void AudioManager::ReleaseRoute(const std::string& key)
{
    auto found = m_routes.find(key);
    if (found == m_routes.end())
        return;
    BusRoute& route = found->second;
    if (route.activeChannels != 0)
        --route.activeChannels;
    if (route.activeChannels != 0)
        return;
    if (route.locked && route.bus && m_hasActiveBackend && m_runtimeStatus.ready) {
        fmod::Result result = m_api.StudioBusUnlockChannelGroup(route.bus);
        if (result != fmod::kOk) {
            m_lastAsyncError = FmodApi::FormatError(
                "FMOD_Studio_Bus_UnlockChannelGroup", result);
            return;
        }
    }
    route.locked = false;
    route.group = nullptr;
    route.bus = nullptr;
}

bool AudioManager::PlaySound(std::uint64_t soundHandle, const SoundPlayOptions& options,
                             std::uint64_t& instanceHandle, std::string& error)
{
    instanceHandle = 0;
    if (!EnsureReady(error))
        return false;
    SoundResource* sound = m_sounds.Get(soundHandle);
    if (!sound) {
        error = "invalid or stale sound handle";
        return false;
    }
    if (!IsFinite(options.volume) || options.volume < 0.0f ||
        !IsFinite(options.pitch) || options.pitch <= 0.0f ||
        !IsFinite(options.minDistance) || options.minDistance <= 0.0f ||
        !IsFinite(options.maxDistance) || options.maxDistance < options.minDistance) {
        error = "sound volume, pitch, or distance range is invalid";
        return false;
    }

    std::string busPath;
    std::string busKey;
    if (!NormalizeNamedPath(options.bus.c_str(), "bus:/", busPath, busKey, error))
        return false;

    ChannelInstance record;
    record.ownerSound = soundHandle;
    record.path = sound->path.display;
    record.routeKey = busKey;
    record.is3D = options.spatial.mode != AudioSpatialMode::None;
    if (options.spatial.mode == AudioSpatialMode::Position) {
        if (!IsFinite(options.spatial.position) || !IsFinite(options.spatial.velocity)) {
            error = "sound position and velocity must be finite";
            return false;
        }
        record.position = ToFmod(options.spatial.position);
        record.velocity = ToFmod(options.spatial.velocity);
    } else if (options.spatial.mode == AudioSpatialMode::Entity) {
        if (options.spatial.entityId == 0 || !IsFinite(options.spatial.offset)) {
            error = "sound attachment requires a valid entity and finite offset";
            return false;
        }
        record.attachment.attached = true;
        record.attachment.entityId = options.spatial.entityId;
        record.attachment.offset = options.spatial.offset;
        if (!BuildEntityChannelPosition(record.attachment, 0.0f,
                                        record.position, record.velocity, error))
            return false;
    }

    BusRoute* route = nullptr;
    if (!AcquireRoute(busPath, busKey, route, error))
        return false;

    fmod::Channel* channel = nullptr;
    fmod::Result result = m_api.SystemPlaySound(
        m_runtimeStatus.identity.core, sound->sound, route->group, 1, &channel);
    if (result != fmod::kOk) {
        if (route->activeChannels == 0) {
            route->activeChannels = 1;
            ReleaseRoute(busKey);
        }
        error = FmodApi::FormatError("FMOD_System_PlaySound", result);
        return false;
    }
    record.channel = channel;
    ++sound->activeInstances;
    ++route->activeChannels;

    const auto fail = [&](const char* operation, fmod::Result failure) {
        error = FmodApi::FormatError(operation, failure);
        fmod::Result stopResult = m_api.ChannelStop(channel);
        if (stopResult == fmod::kOk || stopResult == fmod::kErrInvalidHandle) {
            if (sound->activeInstances != 0)
                --sound->activeInstances;
            ReleaseRoute(busKey);
        } else {
            record.cleanupRequested = true;
            m_channels.Emplace(std::move(record));
            m_lastAsyncError = FmodApi::FormatError("FMOD_Channel_Stop", stopResult);
            error += "; cleanup queued: " + m_lastAsyncError;
        }
        return false;
    };

    fmod::Mode mode = record.is3D ? fmod::kMode3D : fmod::kMode2D;
    mode |= options.loop ? fmod::kLoopNormal : fmod::kLoopOff;
    result = m_api.ChannelSetMode(channel, mode);
    if (result != fmod::kOk)
        return fail("FMOD_Channel_SetMode", result);
    if (record.is3D) {
        result = m_api.ChannelSet3DMinMaxDistance(
            channel, options.minDistance, options.maxDistance);
        if (result != fmod::kOk)
            return fail("FMOD_Channel_Set3DMinMaxDistance", result);
        result = m_api.ChannelSet3DAttributes(channel, &record.position, &record.velocity);
        if (result != fmod::kOk)
            return fail("FMOD_Channel_Set3DAttributes", result);
    }
    result = m_api.ChannelSetVolume(channel, options.volume);
    if (result != fmod::kOk)
        return fail("FMOD_Channel_SetVolume", result);
    result = m_api.ChannelSetPitch(channel, options.pitch);
    if (result != fmod::kOk)
        return fail("FMOD_Channel_SetPitch", result);
    result = m_api.ChannelSetPaused(channel, options.paused ? 1 : 0);
    if (result != fmod::kOk)
        return fail("FMOD_Channel_SetPaused", result);

    instanceHandle = m_channels.Emplace(std::move(record));
    return true;
}

bool AudioManager::StopSound(std::uint64_t handle, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance || !instance->channel) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    fmod::Result result = m_api.ChannelStop(instance->channel);
    if (result != fmod::kOk && result != fmod::kErrInvalidHandle) {
        error = FmodApi::FormatError("FMOD_Channel_Stop", result);
        return false;
    }
    RetireChannel(handle, false);
    return true;
}

bool AudioManager::SetSoundPaused(std::uint64_t handle, bool paused, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    fmod::Result result = m_api.ChannelSetPaused(instance->channel, paused ? 1 : 0);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_SetPaused", result);
        return false;
    }
    return true;
}

bool AudioManager::SetSoundVolume(std::uint64_t handle, float volume, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    if (!IsFinite(volume) || volume < 0.0f) {
        error = "volume must be finite and nonnegative";
        return false;
    }
    fmod::Result result = m_api.ChannelSetVolume(instance->channel, volume);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_SetVolume", result);
        return false;
    }
    return true;
}

bool AudioManager::SetSoundPitch(std::uint64_t handle, float pitch, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    if (!IsFinite(pitch) || pitch <= 0.0f) {
        error = "pitch must be finite and positive";
        return false;
    }
    fmod::Result result = m_api.ChannelSetPitch(instance->channel, pitch);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_SetPitch", result);
        return false;
    }
    return true;
}

bool AudioManager::SetSoundLooping(std::uint64_t handle, bool loop, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    fmod::Mode mode = 0;
    fmod::Result result = m_api.ChannelGetMode(instance->channel, &mode);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_GetMode", result);
        return false;
    }
    mode &= ~(fmod::kLoopOff | fmod::kLoopNormal | fmod::kLoopBidirectional);
    mode |= loop ? fmod::kLoopNormal : fmod::kLoopOff;
    result = m_api.ChannelSetMode(instance->channel, mode);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_SetMode", result);
        return false;
    }
    return true;
}

bool AudioManager::SetSoundPosition(std::uint64_t handle, const AudioVec3& position,
                                    const AudioVec3& velocity, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    if (!instance->is3D) {
        error = "sound instance is not 3D";
        return false;
    }
    if (instance->attachment.attached) {
        error = "detach the sound before setting a static position";
        return false;
    }
    if (!IsFinite(position) || !IsFinite(velocity)) {
        error = "position and velocity must be finite";
        return false;
    }
    fmod::Vector fmodPosition = ToFmod(position);
    fmod::Vector fmodVelocity = ToFmod(velocity);
    fmod::Result result = m_api.ChannelSet3DAttributes(
        instance->channel, &fmodPosition, &fmodVelocity);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_Set3DAttributes", result);
        return false;
    }
    instance->position = fmodPosition;
    instance->velocity = fmodVelocity;
    return true;
}

bool AudioManager::AttachSoundToEntity(std::uint64_t handle, std::uint32_t entityId,
                                       const AudioVec3& offset, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    if (!instance->is3D) {
        error = "sound instance is not 3D";
        return false;
    }
    if (entityId == 0 || !IsFinite(offset)) {
        error = "attachment requires a valid entity and finite offset";
        return false;
    }

    Attachment attachment;
    attachment.attached = true;
    attachment.entityId = entityId;
    attachment.offset = offset;
    fmod::Vector position{};
    fmod::Vector velocity{};
    if (!BuildEntityChannelPosition(attachment, 0.0f, position, velocity, error))
        return false;
    fmod::Result result = m_api.ChannelSet3DAttributes(instance->channel, &position, &velocity);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_Set3DAttributes", result);
        return false;
    }
    instance->attachment = attachment;
    instance->position = position;
    instance->velocity = velocity;
    return true;
}

bool AudioManager::DetachSound(std::uint64_t handle, std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    if (!instance->is3D) {
        error = "sound instance is not 3D";
        return false;
    }
    if (!instance->attachment.attached)
        return true;

    fmod::Vector velocity{};
    fmod::Result result = m_api.ChannelSet3DAttributes(
        instance->channel, &instance->position, &velocity);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_Set3DAttributes", result);
        return false;
    }
    instance->attachment = {};
    instance->velocity = {};
    return true;
}

bool AudioManager::GetSoundState(std::uint64_t handle, SoundStateInfo& state,
                                 std::string& error)
{
    if (!EnsureReady(error))
        return false;
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance) {
        error = "invalid or stale sound instance handle";
        return false;
    }
    state = {};
    state.path = instance->path;
    state.attached = instance->attachment.attached;
    state.is3D = instance->is3D;
    auto route = m_routes.find(instance->routeKey);
    if (route != m_routes.end())
        state.bus = route->second.path;

    fmod::Bool flag = 0;
    fmod::Result result = m_api.ChannelIsPlaying(instance->channel, &flag);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_IsPlaying", result);
        return false;
    }
    state.playing = flag != 0;
    result = m_api.ChannelGetPaused(instance->channel, &flag);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_GetPaused", result);
        return false;
    }
    state.paused = flag != 0;
    result = m_api.ChannelGetVolume(instance->channel, &state.volume);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_GetVolume", result);
        return false;
    }
    result = m_api.ChannelGetPitch(instance->channel, &state.pitch);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_GetPitch", result);
        return false;
    }
    fmod::Mode mode = 0;
    result = m_api.ChannelGetMode(instance->channel, &mode);
    if (result != fmod::kOk) {
        error = FmodApi::FormatError("FMOD_Channel_GetMode", result);
        return false;
    }
    state.looping = (mode & (fmod::kLoopNormal | fmod::kLoopBidirectional)) != 0;
    if (instance->is3D) {
        result = m_api.ChannelGet3DMinMaxDistance(
            instance->channel, &state.minDistance, &state.maxDistance);
        if (result != fmod::kOk) {
            error = FmodApi::FormatError("FMOD_Channel_Get3DMinMaxDistance", result);
            return false;
        }
    }
    return true;
}

bool AudioManager::BuildEntityAttributes(Attachment& attachment, float deltaTime,
                                         fmod::Attributes3D& attributes,
                                         std::string& error) const
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    Offsets::IEntity* entity = env && env->pEntitySystem
        ? env->pEntitySystem->GetEntity(attachment.entityId)
        : nullptr;
    Matrix34* world = entity ? entity->GetWorldTMPtr() : nullptr;
    if (!world) {
        error = "attached entity is not available";
        return false;
    }

    const Vec3 worldPosition = world->TransformPoint(
        Vec3(attachment.offset.x, attachment.offset.y, attachment.offset.z));
    AudioVec3 position = ToAudioVec(worldPosition);
    AudioVec3 forward = ToAudioVec(world->GetColumn1());
    AudioVec3 up = ToAudioVec(world->GetColumn2());
    if (!ValidateSpatialOrientation(forward, up, error))
        return false;

    AudioVec3 velocity;
    if (attachment.hasPreviousPosition && deltaTime > 0.0f && deltaTime <= 0.25f) {
        const float inverseDelta = 1.0f / deltaTime;
        velocity.x = (position.x - attachment.previousPosition.x) * inverseDelta;
        velocity.y = (position.y - attachment.previousPosition.y) * inverseDelta;
        velocity.z = (position.z - attachment.previousPosition.z) * inverseDelta;
    }
    attachment.previousPosition = position;
    attachment.hasPreviousPosition = true;

    attributes.position = ToFmod(position);
    attributes.velocity = ToFmod(velocity);
    attributes.forward = ToFmod(forward);
    attributes.up = ToFmod(up);
    return true;
}

bool AudioManager::BuildEntityChannelPosition(Attachment& attachment, float deltaTime,
                                              fmod::Vector& position,
                                              fmod::Vector& velocity,
                                              std::string& error) const
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    Offsets::IEntity* entity = env && env->pEntitySystem
        ? env->pEntitySystem->GetEntity(attachment.entityId)
        : nullptr;
    Matrix34* world = entity ? entity->GetWorldTMPtr() : nullptr;
    if (!world) {
        error = "attached entity is not available";
        return false;
    }

    const Vec3 worldPosition = world->TransformPoint(
        Vec3(attachment.offset.x, attachment.offset.y, attachment.offset.z));
    AudioVec3 current = ToAudioVec(worldPosition);
    AudioVec3 currentVelocity;
    if (attachment.hasPreviousPosition && deltaTime > 0.0f && deltaTime <= 0.25f) {
        const float inverseDelta = 1.0f / deltaTime;
        currentVelocity.x = (current.x - attachment.previousPosition.x) * inverseDelta;
        currentVelocity.y = (current.y - attachment.previousPosition.y) * inverseDelta;
        currentVelocity.z = (current.z - attachment.previousPosition.z) * inverseDelta;
    }
    attachment.previousPosition = current;
    attachment.hasPreviousPosition = true;
    position = ToFmod(current);
    velocity = ToFmod(currentVelocity);
    return true;
}

bool AudioManager::RetireEvent(std::uint64_t handle, bool stopInstance,
                               bool releaseInstance)
{
    EventInstance* event = m_events.Get(handle);
    if (!event)
        return true;
    const std::uint64_t ownerBank = event->ownerBank;
    const bool valid = event->instance && m_api.StudioEventInstanceIsValid(event->instance);
    if ((stopInstance || releaseInstance) && valid) {
        if (!m_hasActiveBackend || !m_runtimeStatus.ready)
            return false;
        if (stopInstance) {
            fmod::Result result = m_api.StudioEventInstanceStop(
                event->instance, fmod::StudioStopMode::Immediate);
            if (result != fmod::kOk && result != fmod::kErrInvalidHandle) {
                m_lastAsyncError = FmodApi::FormatError(
                    "FMOD_Studio_EventInstance_Stop", result);
                return false;
            }
        }
        if (releaseInstance) {
            fmod::Result result = m_api.StudioEventInstanceRelease(event->instance);
            if (result != fmod::kOk && result != fmod::kErrInvalidHandle) {
                m_lastAsyncError = FmodApi::FormatError(
                    "FMOD_Studio_EventInstance_Release", result);
                return false;
            }
        }
    }
    if (ownerBank) {
        if (BankResource* bank = m_banks.Get(ownerBank); bank && bank->activeInstances != 0)
            --bank->activeInstances;
    }
    m_events.Erase(handle);
    return true;
}

bool AudioManager::RetireChannel(std::uint64_t handle, bool stopChannel)
{
    ChannelInstance* instance = m_channels.Get(handle);
    if (!instance)
        return true;
    const std::uint64_t ownerSound = instance->ownerSound;
    const std::string routeKey = instance->routeKey;
    if (stopChannel && instance->channel) {
        if (!m_hasActiveBackend || !m_runtimeStatus.ready)
            return false;
        fmod::Result result = m_api.ChannelStop(instance->channel);
        if (result != fmod::kOk && result != fmod::kErrInvalidHandle) {
            m_lastAsyncError = FmodApi::FormatError("FMOD_Channel_Stop", result);
            return false;
        }
    }
    if (SoundResource* sound = m_sounds.Get(ownerSound); sound && sound->activeInstances != 0)
        --sound->activeInstances;
    ReleaseRoute(routeKey);
    m_channels.Erase(handle);
    return true;
}

void AudioManager::Tick()
{
    RefreshBackend();
    if (!m_runtimeStatus.ready)
        return;

    auto* env = SSystemGlobalEnvironment::GetInstance();
    const float deltaTime = env && env->pTimer
        ? env->pTimer->GetFrameTime(Offsets::ITimer::ETIMER_GAME)
        : 0.0f;

    for (std::uint64_t handle : m_events.Handles()) {
        EventInstance* event = m_events.Get(handle);
        if (!event)
            continue;
        if (!event->instance || !m_api.StudioEventInstanceIsValid(event->instance)) {
            RetireEvent(handle, false, false);
            continue;
        }
        if (event->attachment.attached) {
            std::string updateError;
            fmod::Attributes3D attributes = event->attributes;
            if (!BuildEntityAttributes(event->attachment, deltaTime, attributes, updateError)) {
                m_lastAsyncError = updateError;
                RetireEvent(handle, true, true);
                continue;
            }
            fmod::Result result = m_api.StudioEventInstanceSet3DAttributes(
                event->instance, &attributes);
            if (result != fmod::kOk) {
                m_lastAsyncError = FmodApi::FormatError(
                    "FMOD_Studio_EventInstance_Set3DAttributes", result);
                RetireEvent(handle, true, true);
                continue;
            }
            event->attributes = attributes;
        }

        fmod::StudioPlaybackState state = fmod::StudioPlaybackState::Stopped;
        fmod::Result result = m_api.StudioEventInstanceGetPlaybackState(event->instance, &state);
        if (result != fmod::kOk) {
            m_lastAsyncError = FmodApi::FormatError(
                "FMOD_Studio_EventInstance_GetPlaybackState", result);
            RetireEvent(handle, true, true);
            continue;
        }
        if (state == fmod::StudioPlaybackState::Stopped)
            RetireEvent(handle, false, true);
    }

    for (std::uint64_t handle : m_channels.Handles()) {
        ChannelInstance* instance = m_channels.Get(handle);
        if (!instance)
            continue;
        if (instance->cleanupRequested) {
            RetireChannel(handle, true);
            continue;
        }
        fmod::Bool playing = 0;
        fmod::Result result = m_api.ChannelIsPlaying(instance->channel, &playing);
        if (result == fmod::kErrInvalidHandle || (result == fmod::kOk && !playing)) {
            RetireChannel(handle, false);
            continue;
        }
        if (result != fmod::kOk) {
            m_lastAsyncError = FmodApi::FormatError("FMOD_Channel_IsPlaying", result);
            RetireChannel(handle, true);
            continue;
        }
        if (instance->attachment.attached) {
            std::string updateError;
            fmod::Vector position{};
            fmod::Vector velocity{};
            if (!BuildEntityChannelPosition(instance->attachment, deltaTime,
                                            position, velocity, updateError)) {
                m_lastAsyncError = updateError;
                RetireChannel(handle, true);
                continue;
            }
            result = m_api.ChannelSet3DAttributes(instance->channel, &position, &velocity);
            if (result != fmod::kOk) {
                m_lastAsyncError = FmodApi::FormatError("FMOD_Channel_Set3DAttributes", result);
                RetireChannel(handle, true);
                continue;
            }
            instance->position = position;
            instance->velocity = velocity;
        }
    }
}

}  // namespace luautils::audio
