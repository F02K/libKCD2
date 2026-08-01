#include "scriptbind/ScriptBind_AudioManager.h"

#include "LuaHelpers.h"
#include "audio/AudioManager.h"
#include "Offsets/vtables/IFunctionHandler.h"
#include "Offsets/vtables/IScriptSystem.h"
#include "Offsets/vtables/IScriptTable.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace luautils {

namespace {

using audio::AudioManager;
using audio::AudioSpatialMode;
using audio::AudioVec3;
using audio::BankInfo;
using audio::EventInfo;
using audio::EventPlayOptions;
using audio::EventStateInfo;
using audio::SoundInfo;
using audio::SoundPlayOptions;
using audio::SoundStateInfo;

ScriptAnyValue NilValue()
{
    ScriptAnyValue value;
    value.type = ANY_TNIL;
    value.nHandle = 0;
    return value;
}

int ReturnError(Offsets::IFunctionHandler* pH, const std::string& error)
{
    const char* message = error.empty() ? "AudioManager operation failed" : error.c_str();
    return pH->EndFunctionAny2(NilValue(), ScriptAnyValue(message));
}

int ReturnTrue(Offsets::IFunctionHandler* pH)
{
    return pH->EndFunctionAny(ScriptAnyValue(true));
}

int ReturnTable(Offsets::IFunctionHandler* pH, Offsets::IScriptTable* table)
{
    if (!table)
        return ReturnError(pH, "could not create Lua result table");
    const int result = pH->EndFunctionAny(ScriptAnyValue(table));
    table->Release();
    return result;
}

bool GetAnyParam(Offsets::IFunctionHandler* pH, int index,
                 ScriptAnyValue& value, std::string& error)
{
    if (index > pH->GetParamCount()) {
        error = "missing required argument";
        return false;
    }
    value.type = ANY_ANY;
    value.table = nullptr;
    if (!pH->GetParamAny(index, value)) {
        error = "could not read argument";
        return false;
    }
    return true;
}

bool GetStringParam(Offsets::IFunctionHandler* pH, int index,
                    std::string& value, std::string& error)
{
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    if (any.type != ANY_TSTRING || !any.str) {
        error = "argument must be a string";
        return false;
    }
    value = any.str;
    return true;
}

bool GetNumberParam(Offsets::IFunctionHandler* pH, int index,
                    float& value, std::string& error)
{
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    if (any.type != ANY_TNUMBER || !std::isfinite(any.number)) {
        error = "argument must be a finite number";
        return false;
    }
    value = any.number;
    return true;
}

bool GetBoolParam(Offsets::IFunctionHandler* pH, int index,
                  bool& value, std::string& error)
{
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    if (any.type != ANY_TBOOLEAN) {
        error = "argument must be a Boolean";
        return false;
    }
    value = any.b;
    return true;
}

bool GetOptionalBoolParam(Offsets::IFunctionHandler* pH, int index,
                          bool& value, std::string& error)
{
    if (index > pH->GetParamCount())
        return true;
    return GetBoolParam(pH, index, value, error);
}

bool GetHandleParamValue(Offsets::IFunctionHandler* pH, int index,
                         std::uint64_t& value, std::string& error)
{
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    if (any.type != ANY_THANDLE || any.nHandle == 0) {
        error = "argument must be a nonzero ScriptHandle";
        return false;
    }
    value = static_cast<std::uint64_t>(any.nHandle);
    return true;
}

bool GetEntityParam(Offsets::IFunctionHandler* pH, int index,
                    std::uint32_t& value, std::string& error)
{
    std::uint64_t handle = 0;
    if (!GetHandleParamValue(pH, index, handle, error))
        return false;
    if (handle > std::numeric_limits<std::uint32_t>::max()) {
        error = "entity ID does not fit the CryEngine EntityId range";
        return false;
    }
    value = static_cast<std::uint32_t>(handle);
    return true;
}

bool ReadNumberValue(const ScriptAnyValue& any, float& value, std::string& error)
{
    if (any.type != ANY_TNUMBER || !std::isfinite(any.number)) {
        error = "option must be a finite number";
        return false;
    }
    value = any.number;
    return true;
}

bool ReadBoolValue(const ScriptAnyValue& any, bool& value, std::string& error)
{
    if (any.type != ANY_TBOOLEAN) {
        error = "option must be a Boolean";
        return false;
    }
    value = any.b;
    return true;
}

bool ReadStringValue(const ScriptAnyValue& any, std::string& value, std::string& error)
{
    if (any.type != ANY_TSTRING || !any.str) {
        error = "option must be a string";
        return false;
    }
    value = any.str;
    return true;
}

bool ReadHandleValue(const ScriptAnyValue& any, std::uint64_t& value, std::string& error)
{
    if (any.type != ANY_THANDLE || any.nHandle == 0) {
        error = "option must be a nonzero ScriptHandle";
        return false;
    }
    value = static_cast<std::uint64_t>(any.nHandle);
    return true;
}

bool ReadTableNumber(Offsets::IScriptTable* table, const char* key,
                     float& value, std::string& error)
{
    ScriptAnyValue any;
    any.type = ANY_ANY;
    any.table = nullptr;
    if (!table->GetValueAny(key, any) || any.type != ANY_TNUMBER ||
        !std::isfinite(any.number)) {
        error = std::string("vector field ") + key + " must be a finite number";
        return false;
    }
    value = any.number;
    return true;
}

bool ReadVectorValue(const ScriptAnyValue& any, AudioVec3& value, std::string& error)
{
    if (any.type == ANY_TVECTOR) {
        value = { any.vec3.x, any.vec3.y, any.vec3.z };
        if (std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z))
            return true;
        error = "vector components must be finite";
        return false;
    }
    if (any.type != ANY_TTABLE || !any.table) {
        error = "option must be a Vec3 or { x, y, z } table";
        return false;
    }
    return ReadTableNumber(any.table, "x", value.x, error) &&
           ReadTableNumber(any.table, "y", value.y, error) &&
           ReadTableNumber(any.table, "z", value.z, error);
}

bool GetVectorParam(Offsets::IFunctionHandler* pH, int index,
                    AudioVec3& value, std::string& error)
{
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    return ReadVectorValue(any, value, error);
}

bool GetOptionalVectorParam(Offsets::IFunctionHandler* pH, int index,
                            AudioVec3& value, std::string& error)
{
    if (index > pH->GetParamCount())
        return true;
    return GetVectorParam(pH, index, value, error);
}

bool GetOptionalTableParam(Offsets::IFunctionHandler* pH, int index,
                           Offsets::IScriptTable*& table, std::string& error)
{
    table = nullptr;
    if (index > pH->GetParamCount())
        return true;
    ScriptAnyValue any;
    if (!GetAnyParam(pH, index, any, error))
        return false;
    if (any.type == ANY_TNIL)
        return true;
    if (any.type != ANY_TTABLE || !any.table) {
        error = "options argument must be a table";
        return false;
    }
    table = any.table;
    return true;
}

template <class Callback>
bool ForEachTableValue(Offsets::IScriptTable* table, Callback&& callback,
                       std::string& error)
{
    Offsets::IScriptTable::Iterator iterator = table->BeginIteration(false);
    bool success = true;
    while (table->MoveNext(iterator)) {
        if (!iterator.sKey) {
            error = "option tables require string keys";
            success = false;
            break;
        }
        if (!callback(iterator.sKey, iterator.value, error)) {
            success = false;
            break;
        }
    }
    table->EndIteration(iterator);
    return success;
}

bool ParseBankOptions(Offsets::IScriptTable* table, bool& sampleData,
                      std::string& error)
{
    if (!table)
        return true;
    return ForEachTableValue(table,
        [&](const char* key, const ScriptAnyValue& value, std::string& parseError) {
            if (std::string(key) == "sampleData")
                return ReadBoolValue(value, sampleData, parseError);
            parseError = std::string("unknown bank option: ") + key;
            return false;
        }, error);
}

bool ParseEventParameters(Offsets::IScriptTable* table,
                          std::vector<audio::EventParameterValue>& parameters,
                          std::string& error)
{
    return ForEachTableValue(table,
        [&](const char* key, const ScriptAnyValue& value, std::string& parseError) {
            float number = 0.0f;
            if (!ReadNumberValue(value, number, parseError))
                return false;
            if (!*key) {
                parseError = "event parameter name is empty";
                return false;
            }
            parameters.push_back({ key, number });
            return true;
        }, error);
}

bool ParseEventOptions(Offsets::IScriptTable* table, EventPlayOptions& options,
                       std::string& error)
{
    if (!table)
        return true;

    bool hasPosition = false;
    bool hasVelocity = false;
    bool hasEntity = false;
    bool hasOffset = false;
    bool hasForward = false;
    bool hasUp = false;

    const bool parsed = ForEachTableValue(table,
        [&](const char* rawKey, const ScriptAnyValue& value, std::string& parseError) {
            const std::string key(rawKey);
            if (key == "position") {
                hasPosition = true;
                return ReadVectorValue(value, options.spatial.position, parseError);
            }
            if (key == "velocity") {
                hasVelocity = true;
                return ReadVectorValue(value, options.spatial.velocity, parseError);
            }
            if (key == "entityId") {
                std::uint64_t entity = 0;
                if (!ReadHandleValue(value, entity, parseError))
                    return false;
                if (entity > std::numeric_limits<std::uint32_t>::max()) {
                    parseError = "entityId exceeds the CryEngine EntityId range";
                    return false;
                }
                hasEntity = true;
                options.spatial.entityId = static_cast<std::uint32_t>(entity);
                return true;
            }
            if (key == "offset") {
                hasOffset = true;
                return ReadVectorValue(value, options.spatial.offset, parseError);
            }
            if (key == "forward") {
                hasForward = true;
                return ReadVectorValue(value, options.spatial.forward, parseError);
            }
            if (key == "up") {
                hasUp = true;
                return ReadVectorValue(value, options.spatial.up, parseError);
            }
            if (key == "volume")
                return ReadNumberValue(value, options.volume, parseError);
            if (key == "pitch")
                return ReadNumberValue(value, options.pitch, parseError);
            if (key == "paused")
                return ReadBoolValue(value, options.paused, parseError);
            if (key == "ignoreSeekSpeed")
                return ReadBoolValue(value, options.ignoreSeekSpeed, parseError);
            if (key == "parameters") {
                if (value.type != ANY_TTABLE || !value.table) {
                    parseError = "parameters must be a string-keyed table";
                    return false;
                }
                return ParseEventParameters(value.table, options.parameters, parseError);
            }
            parseError = "unknown event option: " + key;
            return false;
        }, error);
    if (!parsed)
        return false;

    if (hasPosition && hasEntity) {
        error = "position and entityId are mutually exclusive";
        return false;
    }
    if (hasVelocity && !hasPosition) {
        error = "velocity requires position";
        return false;
    }
    if (hasOffset && !hasEntity) {
        error = "offset requires entityId";
        return false;
    }
    if (hasForward != hasUp) {
        error = "forward and up must be supplied together";
        return false;
    }
    if ((hasForward || hasUp) && !hasPosition) {
        error = "forward and up require a static position";
        return false;
    }
    if (hasPosition)
        options.spatial.mode = AudioSpatialMode::Position;
    else if (hasEntity)
        options.spatial.mode = AudioSpatialMode::Entity;
    return true;
}

bool ParseSoundOptions(Offsets::IScriptTable* table, SoundPlayOptions& options,
                       std::string& error)
{
    if (!table)
        return true;

    bool hasPosition = false;
    bool hasVelocity = false;
    bool hasEntity = false;
    bool hasOffset = false;
    bool hasMinDistance = false;
    bool hasMaxDistance = false;

    const bool parsed = ForEachTableValue(table,
        [&](const char* rawKey, const ScriptAnyValue& value, std::string& parseError) {
            const std::string key(rawKey);
            if (key == "bus")
                return ReadStringValue(value, options.bus, parseError);
            if (key == "loop")
                return ReadBoolValue(value, options.loop, parseError);
            if (key == "paused")
                return ReadBoolValue(value, options.paused, parseError);
            if (key == "volume")
                return ReadNumberValue(value, options.volume, parseError);
            if (key == "pitch")
                return ReadNumberValue(value, options.pitch, parseError);
            if (key == "position") {
                hasPosition = true;
                return ReadVectorValue(value, options.spatial.position, parseError);
            }
            if (key == "velocity") {
                hasVelocity = true;
                return ReadVectorValue(value, options.spatial.velocity, parseError);
            }
            if (key == "entityId") {
                std::uint64_t entity = 0;
                if (!ReadHandleValue(value, entity, parseError))
                    return false;
                if (entity > std::numeric_limits<std::uint32_t>::max()) {
                    parseError = "entityId exceeds the CryEngine EntityId range";
                    return false;
                }
                hasEntity = true;
                options.spatial.entityId = static_cast<std::uint32_t>(entity);
                return true;
            }
            if (key == "offset") {
                hasOffset = true;
                return ReadVectorValue(value, options.spatial.offset, parseError);
            }
            if (key == "minDistance") {
                hasMinDistance = true;
                return ReadNumberValue(value, options.minDistance, parseError);
            }
            if (key == "maxDistance") {
                hasMaxDistance = true;
                return ReadNumberValue(value, options.maxDistance, parseError);
            }
            parseError = "unknown sound option: " + key;
            return false;
        }, error);
    if (!parsed)
        return false;

    if (hasPosition && hasEntity) {
        error = "position and entityId are mutually exclusive";
        return false;
    }
    if (hasVelocity && !hasPosition) {
        error = "velocity requires position";
        return false;
    }
    if (hasOffset && !hasEntity) {
        error = "offset requires entityId";
        return false;
    }
    if ((hasMinDistance || hasMaxDistance) && !hasPosition && !hasEntity) {
        error = "3D distance options require position or entityId";
        return false;
    }
    if (hasPosition)
        options.spatial.mode = AudioSpatialMode::Position;
    else if (hasEntity)
        options.spatial.mode = AudioSpatialMode::Entity;
    return true;
}

const char* LoadingStateName(audio::fmod::StudioLoadingState state)
{
    switch (state) {
    case audio::fmod::StudioLoadingState::Unloading: return "unloading";
    case audio::fmod::StudioLoadingState::Unloaded: return "unloaded";
    case audio::fmod::StudioLoadingState::Loading: return "loading";
    case audio::fmod::StudioLoadingState::Loaded: return "loaded";
    case audio::fmod::StudioLoadingState::Error: return "error";
    }
    return "unknown";
}

const char* PlaybackStateName(audio::fmod::StudioPlaybackState state)
{
    switch (state) {
    case audio::fmod::StudioPlaybackState::Playing: return "playing";
    case audio::fmod::StudioPlaybackState::Sustaining: return "sustaining";
    case audio::fmod::StudioPlaybackState::Stopped: return "stopped";
    case audio::fmod::StudioPlaybackState::Starting: return "starting";
    case audio::fmod::StudioPlaybackState::Stopping: return "stopping";
    }
    return "unknown";
}

const char* SoundTypeName(audio::fmod::SoundType type)
{
    switch (type) {
    case audio::fmod::SoundType::Aiff: return "aiff";
    case audio::fmod::SoundType::Flac: return "flac";
    case audio::fmod::SoundType::Fsb: return "fsb";
    case audio::fmod::SoundType::Mpeg: return "mpeg";
    case audio::fmod::SoundType::OggVorbis: return "oggvorbis";
    case audio::fmod::SoundType::Wav: return "wav";
    case audio::fmod::SoundType::Opus: return "opus";
    default: return "unknown";
    }
}

const char* SoundFormatName(audio::fmod::SoundFormat format)
{
    switch (format) {
    case audio::fmod::SoundFormat::Pcm8: return "pcm8";
    case audio::fmod::SoundFormat::Pcm16: return "pcm16";
    case audio::fmod::SoundFormat::Pcm24: return "pcm24";
    case audio::fmod::SoundFormat::Pcm32: return "pcm32";
    case audio::fmod::SoundFormat::PcmFloat: return "pcmfloat";
    case audio::fmod::SoundFormat::Bitstream: return "bitstream";
    default: return "none";
    }
}

Offsets::IScriptTable* BuildBankInfoTable(Offsets::IScriptSystem* pSS, const BankInfo& info)
{
    Offsets::IScriptTable* table = pSS->CreateTable(0, 0);
    if (!table)
        return nullptr;
    table->SetValueAny("handle", HandleValue(info.handle));
    table->SetValueAny("path", ScriptAnyValue(info.path.c_str()));
    table->SetValueAny("references", ScriptAnyValue(static_cast<int>(info.references)));
    table->SetValueAny("activeInstances", ScriptAnyValue(static_cast<int>(info.activeInstances)));
    table->SetValueAny("eventCount", ScriptAnyValue(info.eventCount));
    table->SetValueAny("sampleLoadingState", ScriptAnyValue(LoadingStateName(info.sampleLoadingState)));
    return table;
}

Offsets::IScriptTable* BuildEventInfoTable(Offsets::IScriptSystem* pSS, const EventInfo& info)
{
    Offsets::IScriptTable* table = pSS->CreateTable(0, 0);
    if (!table)
        return nullptr;
    table->SetValueAny("path", ScriptAnyValue(info.path.c_str()));
    table->SetValueAny("is3D", ScriptAnyValue(info.is3D));
    table->SetValueAny("isOneshot", ScriptAnyValue(info.isOneshot));
    table->SetValueAny("isSnapshot", ScriptAnyValue(info.isSnapshot));
    table->SetValueAny("isStream", ScriptAnyValue(info.isStream));
    table->SetValueAny("lengthMs", ScriptAnyValue(info.lengthMs));
    table->SetValueAny("minDistance", ScriptAnyValue(info.minDistance));
    table->SetValueAny("maxDistance", ScriptAnyValue(info.maxDistance));
    return table;
}

Offsets::IScriptTable* BuildSoundInfoTable(Offsets::IScriptSystem* pSS, const SoundInfo& info)
{
    Offsets::IScriptTable* table = pSS->CreateTable(0, 0);
    if (!table)
        return nullptr;
    table->SetValueAny("handle", HandleValue(info.handle));
    table->SetValueAny("path", ScriptAnyValue(info.path.c_str()));
    table->SetValueAny("references", ScriptAnyValue(static_cast<int>(info.references)));
    table->SetValueAny("activeInstances", ScriptAnyValue(static_cast<int>(info.activeInstances)));
    table->SetValueAny("lengthMs", ScriptAnyValue(static_cast<int>(info.lengthMs)));
    table->SetValueAny("type", ScriptAnyValue(SoundTypeName(info.type)));
    table->SetValueAny("format", ScriptAnyValue(SoundFormatName(info.format)));
    table->SetValueAny("channels", ScriptAnyValue(info.channels));
    table->SetValueAny("bits", ScriptAnyValue(info.bits));
    return table;
}

}  // namespace

void CScriptBind_AudioManager::Init(Offsets::IScriptSystem* pSS)
{
    m_pSS = pSS;
    m_pMethodsTable = pSS->CreateTable(0, 0);
    if (!m_pMethodsTable)
        return;
    m_pMethodsTable->AddRef();

    RegisterFunction("IsReady", "", functor(*this, &CScriptBind_AudioManager::IsReady));
    RegisterFunction("GetStatus", "", functor(*this, &CScriptBind_AudioManager::GetStatus));
    RegisterFunction("GetLoadedBanks", "", functor(*this, &CScriptBind_AudioManager::GetLoadedBanks));
    RegisterFunction("GetLoadedSounds", "", functor(*this, &CScriptBind_AudioManager::GetLoadedSounds));
    RegisterFunction("LoadBank", "path[, options]", functor(*this, &CScriptBind_AudioManager::LoadBank));
    RegisterFunction("UnloadBank", "bankHandle[, force]", functor(*this, &CScriptBind_AudioManager::UnloadBank));
    RegisterFunction("LoadBankSampleData", "bankHandle", functor(*this, &CScriptBind_AudioManager::LoadBankSampleData));
    RegisterFunction("UnloadBankSampleData", "bankHandle", functor(*this, &CScriptBind_AudioManager::UnloadBankSampleData));
    RegisterFunction("GetBankInfo", "bankHandle", functor(*this, &CScriptBind_AudioManager::GetBankInfo));
    RegisterFunction("GetBankEvents", "bankHandle", functor(*this, &CScriptBind_AudioManager::GetBankEvents));
    RegisterFunction("GetEventInfo", "eventPath", functor(*this, &CScriptBind_AudioManager::GetEventInfo));
    RegisterFunction("PlayEvent", "eventPath[, options]", functor(*this, &CScriptBind_AudioManager::PlayEvent));
    RegisterFunction("StopEvent", "eventHandle[, immediate]", functor(*this, &CScriptBind_AudioManager::StopEvent));
    RegisterFunction("SetEventPaused", "eventHandle, paused", functor(*this, &CScriptBind_AudioManager::SetEventPaused));
    RegisterFunction("SetEventParameter", "eventHandle, name, value[, ignoreSeekSpeed]", functor(*this, &CScriptBind_AudioManager::SetEventParameter));
    RegisterFunction("SetEventVolume", "eventHandle, volume", functor(*this, &CScriptBind_AudioManager::SetEventVolume));
    RegisterFunction("SetEventPitch", "eventHandle, pitch", functor(*this, &CScriptBind_AudioManager::SetEventPitch));
    RegisterFunction("SetEventPosition", "eventHandle, position[, velocity]", functor(*this, &CScriptBind_AudioManager::SetEventPosition));
    RegisterFunction("AttachEventToEntity", "eventHandle, entityId[, offset]", functor(*this, &CScriptBind_AudioManager::AttachEventToEntity));
    RegisterFunction("DetachEvent", "eventHandle", functor(*this, &CScriptBind_AudioManager::DetachEvent));
    RegisterFunction("GetEventState", "eventHandle", functor(*this, &CScriptBind_AudioManager::GetEventState));
    RegisterFunction("LoadSound", "path", functor(*this, &CScriptBind_AudioManager::LoadSound));
    RegisterFunction("UnloadSound", "soundHandle[, force]", functor(*this, &CScriptBind_AudioManager::UnloadSound));
    RegisterFunction("GetSoundInfo", "soundHandle", functor(*this, &CScriptBind_AudioManager::GetSoundInfo));
    RegisterFunction("PlaySound", "soundHandle[, options]", functor(*this, &CScriptBind_AudioManager::PlaySound));
    RegisterFunction("StopSound", "instanceHandle", functor(*this, &CScriptBind_AudioManager::StopSound));
    RegisterFunction("SetSoundPaused", "instanceHandle, paused", functor(*this, &CScriptBind_AudioManager::SetSoundPaused));
    RegisterFunction("SetSoundVolume", "instanceHandle, volume", functor(*this, &CScriptBind_AudioManager::SetSoundVolume));
    RegisterFunction("SetSoundPitch", "instanceHandle, pitch", functor(*this, &CScriptBind_AudioManager::SetSoundPitch));
    RegisterFunction("SetSoundLooping", "instanceHandle, loop", functor(*this, &CScriptBind_AudioManager::SetSoundLooping));
    RegisterFunction("SetSoundPosition", "instanceHandle, position[, velocity]", functor(*this, &CScriptBind_AudioManager::SetSoundPosition));
    RegisterFunction("AttachSoundToEntity", "instanceHandle, entityId[, offset]", functor(*this, &CScriptBind_AudioManager::AttachSoundToEntity));
    RegisterFunction("DetachSound", "instanceHandle", functor(*this, &CScriptBind_AudioManager::DetachSound));
    RegisterFunction("GetSoundState", "instanceHandle", functor(*this, &CScriptBind_AudioManager::GetSoundState));

    m_pSS->SetGlobalAny("AudioManager", ScriptAnyValue(m_pMethodsTable));
}

void CScriptBind_AudioManager::RegisterFunction(const char* name, const char* params,
                                                const FunctionFunctor& function)
{
    SUserFunctionDesc descriptor;
    descriptor.sGlobalName = "AudioManager";
    descriptor.sFunctionName = name;
    descriptor.sFunctionParams = params;
    descriptor.pFunctor = function;
    m_pMethodsTable->AddFunction(descriptor);
}

int CScriptBind_AudioManager::IsReady(Offsets::IFunctionHandler* pH)
{
    return pH->EndFunctionAny(ScriptAnyValue(audio::g_audioManager.IsReady()));
}

int CScriptBind_AudioManager::GetStatus(Offsets::IFunctionHandler* pH)
{
    const audio::AudioStatusInfo status = audio::g_audioManager.GetStatus();
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create status table");
    table->SetValueAny("apiResolved", ScriptAnyValue(status.apiResolved));
    table->SetValueAny("ready", ScriptAnyValue(status.ready));
    table->SetValueAny("initState", ScriptAnyValue(static_cast<int>(status.initState)));
    table->SetValueAny("initResultCode", ScriptAnyValue(static_cast<int>(status.initResultCode)));
    table->SetValueAny("runtimeVersion", ScriptAnyValue(static_cast<int>(status.runtimeVersion)));
    table->SetValueAny("backendEpoch", HandleValue(status.backendEpoch));
    table->SetValueAny("bankCount", ScriptAnyValue(static_cast<int>(status.bankCount)));
    table->SetValueAny("soundCount", ScriptAnyValue(static_cast<int>(status.soundCount)));
    table->SetValueAny("eventCount", ScriptAnyValue(static_cast<int>(status.eventCount)));
    table->SetValueAny("channelCount", ScriptAnyValue(static_cast<int>(status.channelCount)));
    if (!status.error.empty())
        table->SetValueAny("error", ScriptAnyValue(status.error.c_str()));
    if (!status.lastAsyncError.empty())
        table->SetValueAny("lastAsyncError", ScriptAnyValue(status.lastAsyncError.c_str()));
    return ReturnTable(pH, table);
}

int CScriptBind_AudioManager::GetLoadedBanks(Offsets::IFunctionHandler* pH)
{
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create bank list");
    int index = 1;
    for (const BankInfo& info : audio::g_audioManager.GetLoadedBanks()) {
        Offsets::IScriptTable* item = BuildBankInfoTable(m_pSS, info);
        if (!item) {
            table->Release();
            return ReturnError(pH, "could not create bank info table");
        }
        table->SetAtAny(index++, ScriptAnyValue(item));
        item->Release();
    }
    return ReturnTable(pH, table);
}

int CScriptBind_AudioManager::GetLoadedSounds(Offsets::IFunctionHandler* pH)
{
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create sound list");
    int index = 1;
    for (const SoundInfo& info : audio::g_audioManager.GetLoadedSounds()) {
        Offsets::IScriptTable* item = BuildSoundInfoTable(m_pSS, info);
        if (!item) {
            table->Release();
            return ReturnError(pH, "could not create sound info table");
        }
        table->SetAtAny(index++, ScriptAnyValue(item));
        item->Release();
    }
    return ReturnTable(pH, table);
}

int CScriptBind_AudioManager::LoadBank(Offsets::IFunctionHandler* pH)
{
    std::string path;
    std::string error;
    Offsets::IScriptTable* table = nullptr;
    bool sampleData = false;
    if (!GetStringParam(pH, 1, path, error) ||
        !GetOptionalTableParam(pH, 2, table, error) ||
        !ParseBankOptions(table, sampleData, error))
        return ReturnError(pH, error);
    std::uint64_t handle = 0;
    if (!audio::g_audioManager.LoadBank(path.c_str(), sampleData, handle, error))
        return ReturnError(pH, error);
    return pH->EndFunctionAny(HandleValue(handle));
}

int CScriptBind_AudioManager::UnloadBank(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool force = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetOptionalBoolParam(pH, 2, force, error))
        return ReturnError(pH, error);
    if (!audio::g_audioManager.UnloadBank(handle, force, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::LoadBankSampleData(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.LoadBankSampleData(handle, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::UnloadBankSampleData(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.UnloadBankSampleData(handle, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::GetBankInfo(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    BankInfo info;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.GetBankInfo(handle, info, error))
        return ReturnError(pH, error);
    return ReturnTable(pH, BuildBankInfoTable(m_pSS, info));
}

int CScriptBind_AudioManager::GetBankEvents(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    std::vector<std::string> events;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.GetBankEvents(handle, events, error))
        return ReturnError(pH, error);
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create event list");
    int index = 1;
    for (const std::string& path : events)
        table->SetAtAny(index++, ScriptAnyValue(path.c_str()));
    return ReturnTable(pH, table);
}

int CScriptBind_AudioManager::GetEventInfo(Offsets::IFunctionHandler* pH)
{
    std::string path;
    std::string error;
    EventInfo info;
    if (!GetStringParam(pH, 1, path, error) ||
        !audio::g_audioManager.GetEventInfo(path.c_str(), info, error))
        return ReturnError(pH, error);
    return ReturnTable(pH, BuildEventInfoTable(m_pSS, info));
}

int CScriptBind_AudioManager::PlayEvent(Offsets::IFunctionHandler* pH)
{
    std::string path;
    std::string error;
    Offsets::IScriptTable* table = nullptr;
    EventPlayOptions options;
    if (!GetStringParam(pH, 1, path, error) ||
        !GetOptionalTableParam(pH, 2, table, error) ||
        !ParseEventOptions(table, options, error))
        return ReturnError(pH, error);
    std::uint64_t handle = 0;
    if (!audio::g_audioManager.PlayEvent(path.c_str(), options, handle, error))
        return ReturnError(pH, error);
    return pH->EndFunctionAny(HandleValue(handle));
}

int CScriptBind_AudioManager::StopEvent(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool immediate = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetOptionalBoolParam(pH, 2, immediate, error) ||
        !audio::g_audioManager.StopEvent(handle, immediate, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetEventPaused(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool value = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetBoolParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetEventPaused(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetEventParameter(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string name;
    float value = 0.0f;
    bool ignoreSeekSpeed = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetStringParam(pH, 2, name, error) ||
        !GetNumberParam(pH, 3, value, error) ||
        !GetOptionalBoolParam(pH, 4, ignoreSeekSpeed, error) ||
        !audio::g_audioManager.SetEventParameter(
            handle, name.c_str(), value, ignoreSeekSpeed, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetEventVolume(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    float value = 0.0f;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetNumberParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetEventVolume(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetEventPitch(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    float value = 0.0f;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetNumberParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetEventPitch(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetEventPosition(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    AudioVec3 position;
    AudioVec3 velocity;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetVectorParam(pH, 2, position, error) ||
        !GetOptionalVectorParam(pH, 3, velocity, error) ||
        !audio::g_audioManager.SetEventPosition(handle, position, velocity, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::AttachEventToEntity(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::uint32_t entityId = 0;
    AudioVec3 offset;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetEntityParam(pH, 2, entityId, error) ||
        !GetOptionalVectorParam(pH, 3, offset, error) ||
        !audio::g_audioManager.AttachEventToEntity(handle, entityId, offset, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::DetachEvent(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.DetachEvent(handle, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::GetEventState(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    EventStateInfo state;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.GetEventState(handle, state, error))
        return ReturnError(pH, error);
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create event state table");
    table->SetValueAny("path", ScriptAnyValue(state.path.c_str()));
    table->SetValueAny("state", ScriptAnyValue(PlaybackStateName(state.playbackState)));
    table->SetValueAny("paused", ScriptAnyValue(state.paused));
    table->SetValueAny("attached", ScriptAnyValue(state.attached));
    table->SetValueAny("volume", ScriptAnyValue(state.volume));
    table->SetValueAny("finalVolume", ScriptAnyValue(state.finalVolume));
    table->SetValueAny("pitch", ScriptAnyValue(state.pitch));
    table->SetValueAny("finalPitch", ScriptAnyValue(state.finalPitch));
    return ReturnTable(pH, table);
}

int CScriptBind_AudioManager::LoadSound(Offsets::IFunctionHandler* pH)
{
    std::string path;
    std::string error;
    if (!GetStringParam(pH, 1, path, error))
        return ReturnError(pH, error);
    std::uint64_t handle = 0;
    if (!audio::g_audioManager.LoadSound(path.c_str(), handle, error))
        return ReturnError(pH, error);
    return pH->EndFunctionAny(HandleValue(handle));
}

int CScriptBind_AudioManager::UnloadSound(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool force = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetOptionalBoolParam(pH, 2, force, error) ||
        !audio::g_audioManager.UnloadSound(handle, force, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::GetSoundInfo(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    SoundInfo info;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.GetSoundInfo(handle, info, error))
        return ReturnError(pH, error);
    return ReturnTable(pH, BuildSoundInfoTable(m_pSS, info));
}

int CScriptBind_AudioManager::PlaySound(Offsets::IFunctionHandler* pH)
{
    std::uint64_t soundHandle = 0;
    std::string error;
    Offsets::IScriptTable* table = nullptr;
    SoundPlayOptions options;
    if (!GetHandleParamValue(pH, 1, soundHandle, error) ||
        !GetOptionalTableParam(pH, 2, table, error) ||
        !ParseSoundOptions(table, options, error))
        return ReturnError(pH, error);
    std::uint64_t handle = 0;
    if (!audio::g_audioManager.PlaySound(soundHandle, options, handle, error))
        return ReturnError(pH, error);
    return pH->EndFunctionAny(HandleValue(handle));
}

int CScriptBind_AudioManager::StopSound(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.StopSound(handle, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetSoundPaused(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool value = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetBoolParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetSoundPaused(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetSoundVolume(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    float value = 0.0f;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetNumberParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetSoundVolume(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetSoundPitch(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    float value = 0.0f;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetNumberParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetSoundPitch(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetSoundLooping(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    bool value = false;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetBoolParam(pH, 2, value, error) ||
        !audio::g_audioManager.SetSoundLooping(handle, value, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::SetSoundPosition(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    AudioVec3 position;
    AudioVec3 velocity;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetVectorParam(pH, 2, position, error) ||
        !GetOptionalVectorParam(pH, 3, velocity, error) ||
        !audio::g_audioManager.SetSoundPosition(handle, position, velocity, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::AttachSoundToEntity(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::uint32_t entityId = 0;
    AudioVec3 offset;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !GetEntityParam(pH, 2, entityId, error) ||
        !GetOptionalVectorParam(pH, 3, offset, error) ||
        !audio::g_audioManager.AttachSoundToEntity(handle, entityId, offset, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::DetachSound(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.DetachSound(handle, error))
        return ReturnError(pH, error);
    return ReturnTrue(pH);
}

int CScriptBind_AudioManager::GetSoundState(Offsets::IFunctionHandler* pH)
{
    std::uint64_t handle = 0;
    SoundStateInfo state;
    std::string error;
    if (!GetHandleParamValue(pH, 1, handle, error) ||
        !audio::g_audioManager.GetSoundState(handle, state, error))
        return ReturnError(pH, error);
    Offsets::IScriptTable* table = m_pSS->CreateTable(0, 0);
    if (!table)
        return ReturnError(pH, "could not create sound state table");
    table->SetValueAny("path", ScriptAnyValue(state.path.c_str()));
    table->SetValueAny("bus", ScriptAnyValue(state.bus.c_str()));
    table->SetValueAny("playing", ScriptAnyValue(state.playing));
    table->SetValueAny("paused", ScriptAnyValue(state.paused));
    table->SetValueAny("attached", ScriptAnyValue(state.attached));
    table->SetValueAny("is3D", ScriptAnyValue(state.is3D));
    table->SetValueAny("looping", ScriptAnyValue(state.looping));
    table->SetValueAny("volume", ScriptAnyValue(state.volume));
    table->SetValueAny("pitch", ScriptAnyValue(state.pitch));
    table->SetValueAny("minDistance", ScriptAnyValue(state.minDistance));
    table->SetValueAny("maxDistance", ScriptAnyValue(state.maxDistance));
    return ReturnTable(pH, table);
}

}  // namespace luautils
