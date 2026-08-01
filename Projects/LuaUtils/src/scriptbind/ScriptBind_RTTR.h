#pragma once

#include "crysystem/SUserFunctionDesc.h"
#include "rttr/RttrHandleRegistry.h"
#include "rttr/RttrLuaConverter.h"

namespace Offsets {
struct IFunctionHandler;
struct IScriptSystem;
struct IScriptTable;
}

namespace luautils {

class CScriptBind_RTTR
{
public:
    CScriptBind_RTTR() noexcept;

    void Init(Offsets::IScriptSystem* pSS);
    bool IsInitialized() const noexcept { return m_pMethodsTable != nullptr; }
    void ClearHandles() noexcept { m_registry.Clear(); }

    int CallGlobal(Offsets::IFunctionHandler* pH);
    int CallMethod(Offsets::IFunctionHandler* pH);
    int GetProperty(Offsets::IFunctionHandler* pH);
    int GetEnum(Offsets::IFunctionHandler* pH);
    int GetTypeName(Offsets::IFunctionHandler* pH);
    int Release(Offsets::IFunctionHandler* pH);
    int Clear(Offsets::IFunctionHandler* pH);

private:
    void RegisterFunction(const char* name, const char* params,
                          const FunctionFunctor& function);

    RttrHandleRegistry m_registry;
    RttrLuaConverter m_converter;
    Offsets::IScriptSystem* m_pSS = nullptr;
    Offsets::IScriptTable* m_pMethodsTable = nullptr;
};

inline CScriptBind_RTTR g_rttrBind;

}  // namespace luautils
