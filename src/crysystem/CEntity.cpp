#include "crysystem/CEntity.h"

#include <REL/Relocation.h>

namespace
{
    struct proxy_ref
    {
        void* object{};
        void* control{};
    };
}

bool CEntity::EnablePhysics(bool enabled)
{
    using ResolveProxy = proxy_ref* (__fastcall*)(CEntity*, proxy_ref*, std::int32_t);
    using ReleaseControl = void (__fastcall*)(void*);
    using SetEnabled = void (__fastcall*)(void*, bool);

    static REL::Relocation<ResolveProxy> resolve{ REL::ID(23925) };       // Steam RVA 0x3CE9A8
    static REL::Relocation<ReleaseControl> release{ REL::ID(29667) };    // Steam RVA 0x4F6588

    proxy_ref physical;
    resolve(this, &physical, 1);  // ENTITY_PROXY_PHYSICS
    if (physical.object) {
        auto** vtable = *reinterpret_cast<void***>(physical.object);
        reinterpret_cast<SetEnabled>(vtable[24])(physical.object, enabled);
    }
    if (physical.control) {
        release(physical.control);
    }
    return physical.object != nullptr || !enabled;
}
