#pragma once

#include "crysystem/SUserFunctionDesc.h"

namespace Offsets {
struct IFunctionHandler;
struct IScriptSystem;
struct IScriptTable;
}

namespace luautils {

class CScriptBind_AudioManager
{
public:
    void Init(Offsets::IScriptSystem* pSS);
    bool IsInitialized() const { return m_pMethodsTable != nullptr; }

    int IsReady(Offsets::IFunctionHandler* pH);
    int GetStatus(Offsets::IFunctionHandler* pH);
    int GetLoadedBanks(Offsets::IFunctionHandler* pH);
    int GetLoadedSounds(Offsets::IFunctionHandler* pH);

    int LoadBank(Offsets::IFunctionHandler* pH);
    int UnloadBank(Offsets::IFunctionHandler* pH);
    int LoadBankSampleData(Offsets::IFunctionHandler* pH);
    int UnloadBankSampleData(Offsets::IFunctionHandler* pH);
    int GetBankInfo(Offsets::IFunctionHandler* pH);
    int GetBankEvents(Offsets::IFunctionHandler* pH);

    int GetEventInfo(Offsets::IFunctionHandler* pH);
    int PlayEvent(Offsets::IFunctionHandler* pH);
    int StopEvent(Offsets::IFunctionHandler* pH);
    int SetEventPaused(Offsets::IFunctionHandler* pH);
    int SetEventParameter(Offsets::IFunctionHandler* pH);
    int SetEventVolume(Offsets::IFunctionHandler* pH);
    int SetEventPitch(Offsets::IFunctionHandler* pH);
    int SetEventPosition(Offsets::IFunctionHandler* pH);
    int AttachEventToEntity(Offsets::IFunctionHandler* pH);
    int DetachEvent(Offsets::IFunctionHandler* pH);
    int GetEventState(Offsets::IFunctionHandler* pH);

    int LoadSound(Offsets::IFunctionHandler* pH);
    int UnloadSound(Offsets::IFunctionHandler* pH);
    int GetSoundInfo(Offsets::IFunctionHandler* pH);
    int PlaySound(Offsets::IFunctionHandler* pH);
    int StopSound(Offsets::IFunctionHandler* pH);
    int SetSoundPaused(Offsets::IFunctionHandler* pH);
    int SetSoundVolume(Offsets::IFunctionHandler* pH);
    int SetSoundPitch(Offsets::IFunctionHandler* pH);
    int SetSoundLooping(Offsets::IFunctionHandler* pH);
    int SetSoundPosition(Offsets::IFunctionHandler* pH);
    int AttachSoundToEntity(Offsets::IFunctionHandler* pH);
    int DetachSound(Offsets::IFunctionHandler* pH);
    int GetSoundState(Offsets::IFunctionHandler* pH);

private:
    void RegisterFunction(const char* name, const char* params, const FunctionFunctor& function);

    Offsets::IScriptSystem* m_pSS = nullptr;
    Offsets::IScriptTable* m_pMethodsTable = nullptr;
};

inline CScriptBind_AudioManager g_audioManagerBind;

}  // namespace luautils
