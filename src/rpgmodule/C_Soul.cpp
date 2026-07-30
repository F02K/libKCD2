#include "rpgmodule/C_Soul.h"
#include "rpgmodule/C_SoulList.h"
#include "rpgmodule/C_RPGModule.h"
#include "Offsets/Offsets.h"
#include "Offsets/vtables/IEntitySystem.h"
#include "Offsets/vtables/IEntity.h"
#include "crysystem/SSystemGlobalEnvironment.h"

#include <algorithm>
#include <cmath>

// C_Soul / C_SoulList engine-function forwarders. Thin wrappers around verified
// KCD2 1.5.6 RVAs (mirrors C_FactionManager.cpp).

namespace wh { namespace rpgmodule {

C_SoulList* C_SoulList::GetInstance()
{
    C_RPGModule* rpg = C_RPGModule::GetInstance();
    return rpg ? rpg->m_pSoulList : nullptr;   // ctor-owned, C_RPGModule+0x80
}

C_Soul* C_SoulList::LookupByWUID(const wh::framework::WUID& wuid)
{
    // sub_181F985D0(&m_soulTable, &wuid) -- the exact call the script-bind soul
    // resolver makes (sub_18041ED20: "__ThisWUID" -> RPGModule+0x80, table @+0x38).
    using Fn = C_Soul* (__fastcall*)(void*, const wh::framework::WUID*);
    static REL::Relocation<Fn> fn{ REL::ID(79) };
    return fn(&m_soulTable, &wuid);
}

float C_Soul::GetDerivedStat(E_DerivedStat statId) const
{
    // sub_180648B18(soul, statId, 0): derived-stat evaluator; float in xmm0.
    using Fn = float (__fastcall*)(const C_Soul*, int, int64_t);
    static REL::Relocation<Fn> fn{ REL::ID(17) };
    return fn(this, static_cast<int32_t>(statId), 0);
}

bool C_Soul::HasAbility(uint32_t abilityId) const
{
    // Worker sub_1809DCC70(block, out, id): binary-search of the sorted ability
    // block @soul+0x320 into a {bool present, u64 payload} result; ids 62/18 are
    // computed inside the worker, and the C_RPGModule cvar block's all-abilities
    // flag forces true. Ids 0 / 73 additionally gate on derived stat 186 / 187
    // (mirrors the Lua HasAbility handler 0x182CF7D88).
    struct S_Result { bool present; uint8_t _pad[7]; uint64_t payload; };
    using Fn = S_Result* (__fastcall*)(const void*, S_Result*, uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(26) };
    S_Result r{};
    fn(&m_soulAbilities, &r, abilityId);
    if (abilityId == 0)
        return r.present && GetDerivedStat(E_DerivedStat::Atd) > 0.0f;
    if (abilityId == 73)
        return r.present && GetDerivedStat(E_DerivedStat::Ams) > 0.0f;
    return r.present;
}

float C_Soul::GetSkillFraction(uint32_t skillId, bool visitorFlag) const
{
    // sub_18046F81C (vtable +0x340): GetSkillLevel_18046F854 / maxLevel; the char flag is
    // forwarded into the modifier-visitor arg pack (uninitialized on the game's own
    // tolerance path -- see the header note).
    using Fn = float (__fastcall*)(const C_Soul*, uint32_t, char);
    static REL::Relocation<Fn> fn{ REL::ID(26997) };
    return fn(this, skillId, visitorFlag);
}

uint32_t C_Soul::GetStatLevel(uint32_t statId) const
{
    return statId < 10 ? m_rpgStats.m_liveBlock.m_stats[statId].value : 0;
}

uint32_t C_Soul::GetSkillLevel(uint32_t skillId) const
{
    return skillId < 35 ? m_rpgStats.m_liveBlock.m_skills[skillId].value : 0;
}

float C_Soul::GetStatProgress(uint32_t statId) const
{
    if (statId >= 10)
        return 0.0f;
    using Fn = float (__fastcall*)(const C_Soul*, uint32_t);
    static REL::Relocation<Fn> fn{ REL::ID(26814) };  // Steam RVA 0x469BF0
    return fn(this, statId);
}

float C_Soul::GetSkillProgress(uint32_t skillId) const
{
    if (skillId >= 35)
        return 0.0f;
    using Fn = float (__fastcall*)(const S_StatCell*);
    static REL::Relocation<Fn> fn{ REL::ID(106156) };  // Steam RVA 0x12EFD30
    return fn(&m_rpgStats.m_liveBlock.m_skills[skillId]);
}

namespace {

using XpThresholdFn = uint32_t* (__fastcall*)(
    const void*, uint32_t*, uint32_t, uint32_t);

uint32_t xp_threshold(bool skill, uint32_t level, uint32_t id)
{
    using ConstantsFn = const void* (__fastcall*)();
    static REL::Relocation<ConstantsFn> constants{ REL::ID(35176) };  // Steam RVA 0x649D30
    static REL::Relocation<XpThresholdFn> stat{ REL::ID(26816) };     // Steam RVA 0x469C64
    static REL::Relocation<XpThresholdFn> skill_fn{ REL::ID(85622) }; // Steam RVA 0xFA30DC
    uint32_t result{};
    if (skill)
        skill_fn(constants(), &result, level, id);
    else
        stat(constants(), &result, level, id);
    return result;
}

bool dispatch_xp(C_Soul* soul, bool skill, uint32_t id, uint32_t amount)
{
    using ResolveEvent = void (__fastcall*)(
        void*, void**, const wh::framework::WUID*);
    using ConstructCause = void* (__fastcall*)(
        void*, void*, uint64_t, uint32_t, const uint32_t*, bool, uint32_t);
    static REL::Relocation<ResolveEvent> resolve{ REL::ID(54065) };       // Steam RVA 0x9DC2EC
    static REL::Relocation<ConstructCause> stat_cause{ REL::ID(66740) }; // Steam RVA 0xC671B0
    static REL::Relocation<ConstructCause> skill_cause{ REL::ID(66741) };// Steam RVA 0xC67268
    static REL::Relocation<std::uintptr_t> rpg_module{ REL::ID(2349) };  // Steam RVA 0x53322A0

    auto* module = *reinterpret_cast<void**>(rpg_module.address());
    if (!module)
        return false;
    auto* manager = *reinterpret_cast<void**>(
        reinterpret_cast<std::byte*>(module) + 0xB0);
    if (!manager)
        return false;
    auto** vtable = *reinterpret_cast<void***>(manager);
    using Allocate = void* (__fastcall*)(void*);
    using Dispatch = void (__fastcall*)(void*, void*);
    auto allocate = reinterpret_cast<Allocate>(vtable[1]);
    auto dispatch = reinterpret_cast<Dispatch>(vtable[0]);

    (void)allocate(manager);
    void* event{};
    resolve(manager, &event, &soul->m_selfWuid);
    auto* storage = allocate(manager);
    if (!storage || !event)
        return false;
    const auto wuid = soul->m_selfWuid.m_value;
    auto* cause = skill
        ? skill_cause(storage, event, wuid, id, &amount, false, 0U)
        : stat_cause(storage, event, wuid, id, &amount, false, 6U);
    if (!cause)
        return false;
    dispatch(manager, event);
    return true;
}

bool set_absolute(
    C_Soul* soul,
    bool skill,
    uint32_t id,
    uint32_t level,
    float progress)
{
    if ((!skill && id >= 10) || (skill && id >= 35)
        || !std::isfinite(progress) || progress < 0.0f || progress > 1.0f)
        return false;

    uint64_t total{};
    for (uint32_t current = 0; current < level; ++current)
        total += xp_threshold(skill, current, id);
    total += static_cast<uint64_t>(std::llround(
        static_cast<double>(xp_threshold(skill, level, id))
        * std::clamp(progress, 0.0f, 1.0f)));
    if (total > UINT32_MAX)
        return false;

    auto& live = skill
        ? soul->m_rpgStats.m_liveBlock.m_skills[id]
        : soul->m_rpgStats.m_liveBlock.m_stats[id];
    auto& base = skill
        ? soul->m_rpgStats.m_baseBlock.m_skills[id]
        : soul->m_rpgStats.m_baseBlock.m_stats[id];
    live = {};
    base = {};
    if (!dispatch_xp(soul, skill, id, static_cast<uint32_t>(total)))
        return false;
    base = live;
    return live.value == level;
}

}  // namespace

bool C_Soul::SetStatAbsolute(
    uint32_t statId, uint32_t level, float progress)
{
    return set_absolute(this, false, statId, level, progress);
}

bool C_Soul::SetSkillAbsolute(
    uint32_t skillId, uint32_t level, float progress)
{
    return set_absolute(this, true, skillId, level, progress);
}

bool C_Soul::SetSharedSoulGuid(const CryGUID& guid)
{
    using Fn = void (__fastcall*)(C_Soul*, const CryGUID*);
    static REL::Relocation<Fn> fn{ REL::ID(24744) };  // Steam RVA 0x3F124C
    fn(this, &guid);
    return m_sharedSoulGuid == guid;
}

bool C_SoulList::ApplySharedSoul(
    C_Soul& soul, const CryGUID& sharedSoulGuid)
{
    if (!soul.SetSharedSoulGuid(sharedSoulGuid))
        return false;

    // The native constructor invokes this immediately after SetSharedSoulGuid
    // to load the shared record into the freshly allocated Soul.
    using Fn = bool (__fastcall*)(
        C_SoulList*, C_Soul*, const CryGUID*, bool);
    static REL::Relocation<Fn> fn{ REL::ID(24851) };  // Steam RVA 0x3F4578
    return fn(this, &soul, &sharedSoulGuid, false);
}

float C_Soul::GetPerkStatModifier(E_PerkStat statId, float seed, void* ctx) const
{
    // sub_180649F1C (vtable +0x310): modifier fold over seed + id-specific clamp.
    using Fn = float (__fastcall*)(const C_Soul*, int32_t, float, void*);
    static REL::Relocation<Fn> fn{ REL::ID(35185) };
    return fn(this, static_cast<int32_t>(statId), seed, ctx);
}

Offsets::IEntity* C_Soul::GetBoundEntity() const
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pEntitySystem)
        return nullptr;

    const uint32_t id = env->pEntitySystem->FindEntityByGuid(m_entityGuid);
    return id ? env->pEntitySystem->GetEntity(id) : nullptr;
}

}}  // namespace wh::rpgmodule
