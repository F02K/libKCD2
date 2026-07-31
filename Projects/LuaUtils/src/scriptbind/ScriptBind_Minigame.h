// LuaUtils extension of the vanilla global `Minigame` Lua table.
//
// Not a CScriptableBase: the vanilla wh::playermodule::C_ScriptBindMinigame
// owns the table; we AddFunction our descriptor into it after its registrar
// runs (hook in plugin.cpp). Dot-call only.
//
//   Minigame.RequestExit(userId) -> bool     -- finds the user's live minigame
//     session (any type) and fires I_Minigame::RequestExit [46] on it -- the
//     same native path the "minigame_exit" key (ESC/back) drives. false/nil
//     if the user has no live session.

#pragma once

#include "crysystem/SUserFunctionDesc.h"

namespace Offsets {
struct IFunctionHandler;
struct IScriptSystem;
struct IScriptTable;
}

namespace luautils {

class CScriptBind_MinigameExt
{
public:
    // Attach to the live Minigame methods table and register our functions.
    // Re-attaching to the same table re-registers (AddFunction replaces - idempotent).
    void Attach(Offsets::IScriptSystem* pSS, Offsets::IScriptTable* pTable);
    bool IsAttached() const { return m_pTable != nullptr; }

    int RequestExit(Offsets::IFunctionHandler* pH);

private:
    void RegisterFunction(const char* sName, const char* sParams, const FunctionFunctor& f);

    Offsets::IScriptSystem* m_pSS = nullptr;
    Offsets::IScriptTable*  m_pTable = nullptr;
};

inline CScriptBind_MinigameExt g_minigameExt;

}  // namespace luautils
