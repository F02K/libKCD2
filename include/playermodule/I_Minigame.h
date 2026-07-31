#pragma once
#include <cstdint>
#include "E_MinigameType.h"

// -----------------------------------------------
// wh::playermodule::I_Minigame -- minigame-session interface (KCD2 WHGame.dll 1.5.6, kd7u).
// sizeof 0x08 (vptr only).
// -----------------------------------------------
// Primary interface of the per-player minigame sessions stored in the player action subsystem
// (owner+0x20 hash: userId -> list<I_Minigame*>).  C_Alchemy implements it as the head of its
// primary vtable chain (0x183F60B08).  Slot knowledge is PARTIAL -- only the slots exercised by
// the generic session plumbing and the alchemy session are named; everything else is _vfN filler.
// DO NOT call through unnamed slots.  Total slot count >= 47 ([46] verified; [36]-[45] still
// unverified filler).
//
// Evidence: FindOrCreateAction sub_182024240 matches sessions by vf[0]()==type and consults
// vf[7](); C_Alchemy::Teardown = primary slot [5] (sub_1809F1A8C, vtable+0x28); the start path
// invokes +0x108 self-destruct on failure, +0x110 push-action-map-context (CryString), +0x118 the
// minigame-exit hint toggler (sub_1809F1E1C).
//
// RequestExit [46] (vtable+0x170): fired by the "minigame_exit" input action (ESC/backspace on
// keyboard, B/Circle on pad -- fixed "back" superaction, defaultProfile.xml) via
// C_Minigame::SetActionMapContext (0x18085A3EC), which registers a std::function callback whose
// _Do_call (sub_18202B570) invokes exactly this slot on the captured session. Verified consistent
// across four session types: C_Alchemy (sub_1819C790C -- cancels the in-flight verb action, then
// mode=6/"Exiting" or defers via a pending-exit flag if the current action can't be interrupted
// yet), C_Dice (sub_182E91AD0 -- same shape, own mode field), C_LockPicking (sub_1827C0EBC) and
// C_Blacksmithing (sub_1806A0700) (thin thunks forwarding to their own class-specific slot). This
// is the universal "player pressed the exit key" entry point -- NOT the final teardown (see
// Teardown [5] above), which a later per-frame driver invokes once the exit resolves.

namespace wh::playermodule {

class I_Minigame {
public:
    virtual E_MinigameType::Type GetMinigameType() const = 0;   // [0]  matched by FindOrCreateAction (type 3 = Alchemy)
    virtual void _vf1() = 0;                                    // [1]
    virtual void _vf2() = 0;                                    // [2]
    virtual void _vf3() = 0;                                    // [3]
    virtual void _vf4() = 0;                                    // [4]
    virtual void Teardown() = 0;                                // [5]  session teardown (C_Alchemy: sub_1809F1A8C -- unregisters "alchemy" filters, audio/entity cleanup)
    virtual void _vf6() = 0;                                    // [6]
    virtual bool IsFinished() const = 0;                        // [7]  +0x38  liveness/completion query consulted by FindOrCreateAction
                                                                //      (matchExtra || !IsFinished()).  Verified across 4 session types:
                                                                //      C_Alchemy (sub_180737914: mode byte @+0x300 == 8), C_Dice
                                                                //      (sub_181A74AC0: raw flag byte @+0x118), C_LockPicking
                                                                //      (sub_181783FA8: mode byte @+0x88 == 0xB), C_Blacksmithing
                                                                //      (sub_180858F80: two nested sub-queue status checks) -- pure
                                                                //      query in all four, no side effects.
    virtual void _vf8() = 0;                                    // [8]
    virtual void _vf9() = 0;                                    // [9]
    virtual void _vf10() = 0;                                   // [10]
    virtual void _vf11() = 0;                                   // [11]
    virtual void _vf12() = 0;                                   // [12]
    virtual void _vf13() = 0;                                   // [13]
    virtual void _vf14() = 0;                                   // [14]
    virtual void _vf15() = 0;                                   // [15]
    virtual void _vf16() = 0;                                   // [16]
    virtual void _vf17() = 0;                                   // [17]
    virtual void _vf18() = 0;                                   // [18]
    virtual void _vf19() = 0;                                   // [19]
    virtual void _vf20() = 0;                                   // [20]
    virtual void Reset() = 0;                                   // [21] in-session reset.  C_Alchemy impl
                                                                //      sub_1806C4F1C: full table reset
                                                                //      (content visuals/buckets/props/state/
                                                                //      fire/verbs, kind-entity ids KEPT);
                                                                //      run by the finish cbs AFTER grading
    virtual void _vf22() = 0;                                   // [22]
    virtual void _vf23() = 0;                                   // [23]
    virtual void _vf24() = 0;                                   // [24]
    virtual void _vf25() = 0;                                   // [25]
    virtual void _vf26() = 0;                                   // [26]
    virtual void _vf27() = 0;                                   // [27]
    virtual void _vf28() = 0;                                   // [28]
    virtual void _vf29() = 0;                                   // [29]
    virtual void _vf30() = 0;                                   // [30]
    virtual void _vf31() = 0;                                   // [31]
    virtual void _vf32() = 0;                                   // [32]
    virtual void SelfDestruct() = 0;                            // [33] +0x108  invoked when Start fails (session discards itself)
    virtual void PushActionMapContext(void* cryStrContext) = 0; // [34] +0x110  Start pushes CryString("alchemy") through this
    virtual void ToggleExitHint(bool enable) = 0;               // [35] +0x118  sub_1809F1E1C -- "minigame_exit" action/hint
    virtual void _vf36() = 0;                                   // [36]
    virtual void _vf37() = 0;                                   // [37]
    virtual void _vf38() = 0;                                   // [38]
    virtual void _vf39() = 0;                                   // [39]
    virtual void _vf40() = 0;                                   // [40]
    virtual void _vf41() = 0;                                   // [41]
    virtual void _vf42() = 0;                                   // [42]
    virtual void _vf43() = 0;                                   // [43]
    virtual void _vf44() = 0;                                   // [44]
    virtual void _vf45() = 0;                                   // [45]
    virtual void RequestExit() = 0;                             // [46] +0x170  fired by the "minigame_exit" action (ESC/backspace via the
                                                                //      fixed "back" superaction); see banner evidence above. Cancels the
                                                                //      in-flight action and transitions toward session exit, deferring
                                                                //      if not currently interruptible. Universal across minigame types.
};
static_assert(sizeof(I_Minigame) == 8, "I_Minigame is a vptr-only interface");

}  // namespace wh::playermodule
