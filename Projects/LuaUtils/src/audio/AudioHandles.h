#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace luautils::audio {

enum class AudioHandleKind : std::uint8_t {
    Bank = 1,
    Sound,
    Event,
    Channel,
};

inline constexpr std::uint64_t MakeAudioHandle(AudioHandleKind kind,
                                                std::uint32_t generation,
                                                std::uint32_t slot)
{
    return (static_cast<std::uint64_t>(kind) << 56) |
           ((static_cast<std::uint64_t>(generation) & 0xFFFFFFu) << 32) |
           (static_cast<std::uint64_t>(slot) + 1u);
}

inline constexpr AudioHandleKind GetAudioHandleKind(std::uint64_t handle)
{
    return static_cast<AudioHandleKind>(handle >> 56);
}

inline constexpr std::uint32_t GetAudioHandleGeneration(std::uint64_t handle)
{
    return static_cast<std::uint32_t>((handle >> 32) & 0xFFFFFFu);
}

inline constexpr std::uint32_t GetAudioHandleSlot(std::uint64_t handle)
{
    return static_cast<std::uint32_t>(handle) - 1u;
}

static_assert(MakeAudioHandle(AudioHandleKind::Bank, 1, 0) != 0);
static_assert(GetAudioHandleKind(MakeAudioHandle(AudioHandleKind::Channel, 7, 9)) ==
              AudioHandleKind::Channel);
static_assert(GetAudioHandleGeneration(MakeAudioHandle(AudioHandleKind::Event, 7, 9)) == 7);
static_assert(GetAudioHandleSlot(MakeAudioHandle(AudioHandleKind::Sound, 7, 9)) == 9);

template <AudioHandleKind Kind, class T>
class AudioHandlePool
{
public:
    template <class... Args>
    std::uint64_t Emplace(Args&&... args)
    {
        std::uint32_t index;
        if (m_free.empty()) {
            index = static_cast<std::uint32_t>(m_slots.size());
            m_slots.push_back({});
        } else {
            index = m_free.back();
            m_free.pop_back();
        }
        Slot& slot = m_slots[index];
        slot.value.emplace(std::forward<Args>(args)...);
        ++m_size;
        return MakeAudioHandle(Kind, slot.generation, index);
    }

    T* Get(std::uint64_t handle)
    {
        if (!Validate(handle))
            return nullptr;
        return &*m_slots[GetAudioHandleSlot(handle)].value;
    }

    const T* Get(std::uint64_t handle) const
    {
        if (!Validate(handle))
            return nullptr;
        return &*m_slots[GetAudioHandleSlot(handle)].value;
    }

    bool Erase(std::uint64_t handle)
    {
        if (!Validate(handle))
            return false;
        const std::uint32_t index = GetAudioHandleSlot(handle);
        Slot& slot = m_slots[index];
        slot.value.reset();
        slot.generation = (slot.generation + 1u) & 0xFFFFFFu;
        if (slot.generation == 0)
            slot.generation = 1;
        m_free.push_back(index);
        --m_size;
        return true;
    }

    std::vector<std::uint64_t> Handles() const
    {
        std::vector<std::uint64_t> handles;
        handles.reserve(m_size);
        for (std::uint32_t index = 0; index < m_slots.size(); ++index) {
            const Slot& slot = m_slots[index];
            if (slot.value)
                handles.push_back(MakeAudioHandle(Kind, slot.generation, index));
        }
        return handles;
    }

    void Clear()
    {
        m_free.clear();
        for (std::uint32_t index = 0; index < m_slots.size(); ++index) {
            Slot& slot = m_slots[index];
            slot.value.reset();
            slot.generation = (slot.generation + 1u) & 0xFFFFFFu;
            if (slot.generation == 0)
                slot.generation = 1;
            m_free.push_back(index);
        }
        m_size = 0;
    }

    std::size_t Size() const { return m_size; }

private:
    struct Slot {
        std::uint32_t generation = 1;
        std::optional<T> value;
    };

    bool Validate(std::uint64_t handle) const
    {
        if (handle == 0 || GetAudioHandleKind(handle) != Kind)
            return false;
        const std::uint32_t encodedSlot = static_cast<std::uint32_t>(handle);
        if (encodedSlot == 0)
            return false;
        const std::uint32_t index = encodedSlot - 1u;
        if (index >= m_slots.size())
            return false;
        const Slot& slot = m_slots[index];
        return slot.value && slot.generation == GetAudioHandleGeneration(handle);
    }

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_free;
    std::size_t m_size = 0;
};

}  // namespace luautils::audio
