#include "scriptbind/ScriptBind_Minigame.h"

#include "Offsets/vtables/IFunctionHandler.h"
#include "Offsets/vtables/IScriptSystem.h"
#include "Offsets/vtables/IScriptTable.h"
#include "game/S_GameContext.h"
#include "playermodule/C_MinigameManager.h"
#include "playermodule/C_PlayerModule.h"
#include "playermodule/I_Minigame.h"

using namespace wh::playermodule;

namespace luautils {

void CScriptBind_MinigameExt::Attach(Offsets::IScriptSystem* pSS, Offsets::IScriptTable* pTable)
{
    if (!pSS || !pTable)
        return;
    m_pSS = pSS;
    m_pTable = pTable;

    RegisterFunction("RequestExit", "userId", functor(*this, &CScriptBind_MinigameExt::RequestExit));
}

void CScriptBind_MinigameExt::RegisterFunction(const char* sName, const char* sParams, const FunctionFunctor& f)
{
    SUserFunctionDesc fd;
    fd.sGlobalName     = "Minigame";
    fd.sFunctionName   = sName;
    fd.sFunctionParams = sParams;
    fd.pFunctor        = f;
    m_pTable->AddFunction(fd);
}

int CScriptBind_MinigameExt::RequestExit(Offsets::IFunctionHandler* pH)
{
    int userId = 0;
    if (!pH->GetParam(1, userId))
        return pH->EndFunction();

    auto* ctx = wh::game::S_GameContext::GetInstance();
    auto* pm = ctx ? ctx->m_pPlayerModule : nullptr;
    auto* mgr = pm ? pm->m_pMinigameManager : nullptr;
    if (!mgr)
        return pH->EndFunction();

    auto range = mgr->m_sessions.equal_range(static_cast<uint32_t>(userId));
    for (auto it = range.first; it != range.second; ++it) {
        if (!it->second->IsFinished()) {   // the live (not-yet-finished) session
            it->second->RequestExit();
            return pH->EndFunctionAny(ScriptAnyValue(true));
        }
    }
    return pH->EndFunction();
}

}  // namespace luautils
