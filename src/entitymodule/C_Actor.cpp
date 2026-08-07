#include "entitymodule/C_Actor.h"
#include "Offsets/Offsets.h"
#include "Offsets/RTTI.h"
#include "rpgmodule/C_Soul.h"

#include <array>

// C_Actor engine-function forwarders (KCD2 WHGame.dll 1.5.6 RVAs, verified in kd7u).

namespace wh::entitymodule {

namespace {

bool has_actor_runtime_type(const C_Actor* actor, const REL::ID& target)
{
    if (!actor)
        return false;
    // C_Actor / C_Animal / C_Human TypeDescriptors. Using explicit address
    // IDs here keeps every runtime dependency covered by KCD2MP's vendored
    // Address Library audit on Steam, GOG, and Epic.
    const auto source = REL::ID(1519);  // RTTI_C_Actor
    return __RTDynamicCast(
        const_cast<C_Actor*>(actor),
        0,
        reinterpret_cast<void*>(source.address()),
        reinterpret_cast<void*>(target.address()),
        0) != nullptr;
}

}  // namespace

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
    return RequestLocomotion({moveTarget, nullptr, desiredSpeed, false});
}

bool C_Actor::RequestLocomotion(
    const SMultiplayerLocomotionRequest& input)
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
    *reinterpret_cast<float*>(request.storage.data() + 56) = input.desiredSpeed;
    *reinterpret_cast<float*>(request.storage.data() + 60) = input.desiredSpeed;
    if (input.moveTarget)
    {
        *flags |= 1ULL << 4;  // eMRF_MoveTarget
        *reinterpret_cast<Vec3*>(request.storage.data() + 88) = *input.moveTarget;
    }
    if (input.facingDirection)
    {
        *flags |= 1ULL << 18;  // eMRF_DesiredBodyDirectionAtTarget
        *reinterpret_cast<Vec3*>(request.storage.data() + 112) =
            *input.facingDirection;
        *flags |= 1ULL << 23;  // eMRF_AllowLowerBodyToTurn
    }
    if (input.allowStrafing)
        *flags |= 1ULL << 21;  // eMRF_AllowStrafing

    auto** vtable = *reinterpret_cast<void***>(m_pMovementController);
    using Fn = bool (__fastcall*)(IMovementController*, movement_request*);
    return reinterpret_cast<Fn>(vtable[1])(m_pMovementController, &request);
}

bool C_Actor::IsHumanActor() const
{
    return has_actor_runtime_type(this, REL::ID(1533));  // RTTI_C_Human
}

bool C_Actor::IsAnimalActor() const
{
    return has_actor_runtime_type(this, REL::ID(1526));  // RTTI_C_Animal
}

}  // namespace wh::entitymodule
