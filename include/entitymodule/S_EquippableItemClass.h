#pragma once
#include <cstddef>
#include <cstdint>
#include "S_PlayerItemClass.h"

// -----------------------------------------------
// wh::entitymodule::S_EquippableItemClass -- KCD2 WHGame.dll 1.5.6 (kd7u). sizeof 0xE8.
// -----------------------------------------------
// RTTI .?AUS_EquippableItemClass@entitymodule@wh@@, vtable 0x183A4F1F8,
// ctor sub_180754B60. S_ItemClassWrapper<S_EquippableItemClass,
// S_PlayerItemClass, 26> certifies both the parent and typed-downcast slot.

namespace wh::entitymodule {

class S_EquippableItemClass : public S_PlayerItemClass {
public:
    S_EquippableItemClass* GetAsEquippableItemClass() override; // [26] sub_1805F5DA0 returns this

    std::uint64_t m_unknownB0;  // +0xB0  ctor 0
    float m_unknownB8;          // +0xB8  ctor 1.0
    float m_unknownBC;          // +0xBC  ctor 1.0
    std::int32_t m_unknownC0;   // +0xC0  ctor 0
    std::int32_t m_unknownC4;   // +0xC4  ctor 0
    CryStringT<char> m_unknownC8; // +0xC8  ctor empty
    std::uint64_t m_unknownD0;  // +0xD0  ctor 0
    std::uint64_t m_unknownD8;  // +0xD8  ctor 0
    std::int32_t m_unknownE0;   // +0xE0  ctor 0
    std::uint8_t _padE4[4];     // +0xE4
};

static_assert(sizeof(S_EquippableItemClass) == 0xE8, "S_EquippableItemClass must be 0xE8");
static_assert(offsetof(S_EquippableItemClass, m_unknownB0) == 0xB0, "equippable fields must begin at +0xB0");
static_assert(offsetof(S_EquippableItemClass, m_unknownC8) == 0xC8, "equippable string must be at +0xC8");

}  // namespace wh::entitymodule
