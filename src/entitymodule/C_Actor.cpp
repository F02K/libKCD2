#include "entitymodule/C_Actor.h"
#include "Offsets/Offsets.h"
#include "rpgmodule/C_Soul.h"

#include <array>

// C_Actor engine-function forwarders (KCD2 WHGame.dll 1.5.6 RVAs, verified in kd7u).

namespace wh::entitymodule {

wh::combatmodule::C_CombatActor* C_Actor::GetOrCreateCombatActor()
{
    // sub_18072DC90: returns m_pCombatActor (+0x278), allocating the 0x448 C_CombatActor
    // on first use.
    using Fn = wh::combatmodule::C_CombatActor* (__fastcall*)(C_Actor*);
    static REL::Relocation<Fn> fn{ REL::ID(21) };
    return fn(this);
}

C_Inventory* C_Actor::GetInventory()
{
    // Typed reimplementation of sub_1808D285C (every hop is an RE'd member/virtual):
    // m_pSoul (+0x668) -> C_Soul::m_inventorySoul (+0x198) -> I_InventorySoul::GetInventory [0].
    return m_pSoul ? m_pSoul->m_inventorySoul.GetInventory() : nullptr;
}

bool C_Actor::RequestLocomotion(const Vec3* moveTarget, float desiredSpeed)
{
    if (!m_pMovementController)
        return false;

    // KCD2's CMovementRequest retains the CryEngine binary layout:
    // flags@0, desired/target speed@56/60, move target@88. The controller
    // reads only fields selected by these flags.
    struct alignas(8) movement_request
    {
        std::array<std::byte, 512> storage{};
    } request;
    auto* flags = reinterpret_cast<std::uint64_t*>(request.storage.data());
    *flags |= 1ULL << 2;  // eMRF_DesiredSpeed
    *reinterpret_cast<float*>(request.storage.data() + 56) = desiredSpeed;
    *reinterpret_cast<float*>(request.storage.data() + 60) = desiredSpeed;
    if (moveTarget)
    {
        *flags |= 1ULL << 4;  // eMRF_MoveTarget
        *reinterpret_cast<Vec3*>(request.storage.data() + 88) = *moveTarget;
    }

    auto** vtable = *reinterpret_cast<void***>(m_pMovementController);
    using Fn = bool (__fastcall*)(IMovementController*, movement_request*);
    return reinterpret_cast<Fn>(vtable[1])(m_pMovementController, &request);
}

}  // namespace wh::entitymodule
