#pragma once

#include <cstddef>
#include <cstdint>

namespace luautils::audio::fmod {

using Result = std::int32_t;
using Bool = std::int32_t;
using Mode = std::uint32_t;
using TimeUnit = std::uint32_t;
using StudioLoadBankFlags = std::uint32_t;

struct System;
struct Sound;
struct Channel;
struct ChannelGroup;
struct StudioSystem;
struct StudioBank;
struct StudioEventDescription;
struct StudioEventInstance;
struct StudioBus;

inline constexpr std::uint32_t kVersion = 0x00020221;
inline constexpr Result kOk = 0;
inline constexpr Result kErrInvalidHandle = 30;
inline constexpr Result kErrTruncated = 65;

inline constexpr Mode kLoopOff = 0x00000001;
inline constexpr Mode kLoopNormal = 0x00000002;
inline constexpr Mode kLoopBidirectional = 0x00000004;
inline constexpr Mode kMode2D = 0x00000008;
inline constexpr Mode kMode3D = 0x00000010;
inline constexpr Mode kCreateSample = 0x00000100;
inline constexpr Mode kOpenMemory = 0x00000800;
inline constexpr TimeUnit kTimeUnitMs = 0x00000001;

inline constexpr int kStudioLoadMemory = 0;
inline constexpr StudioLoadBankFlags kStudioLoadBankNormal = 0;

enum class StudioLoadingState : std::int32_t {
    Unloading = 0,
    Unloaded,
    Loading,
    Loaded,
    Error,
};

enum class StudioPlaybackState : std::int32_t {
    Playing = 0,
    Sustaining,
    Stopped,
    Starting,
    Stopping,
};

enum class StudioStopMode : std::int32_t {
    AllowFadeout = 0,
    Immediate,
};

enum class SoundType : std::int32_t {
    Unknown = 0,
    Aiff,
    Asf,
    Dls,
    Flac,
    Fsb,
    It,
    Midi,
    Mod,
    Mpeg,
    OggVorbis,
    Playlist,
    Raw,
    S3m,
    User,
    Wav,
    Xm,
    Xma,
    AudioQueue,
    At9,
    Vorbis,
    MediaFoundation,
    MediaCodec,
    Fadpcm,
    Opus,
};

enum class SoundFormat : std::int32_t {
    None = 0,
    Pcm8,
    Pcm16,
    Pcm24,
    Pcm32,
    PcmFloat,
    Bitstream,
};

struct Vector {
    float x;
    float y;
    float z;
};
static_assert(sizeof(Vector) == 0xC);

struct Attributes3D {
    Vector position;
    Vector velocity;
    Vector forward;
    Vector up;
};
static_assert(sizeof(Attributes3D) == 0x30);

struct CreateSoundExInfo {
    std::int32_t cbsize;
    std::uint32_t length;
    std::uint8_t reserved[0xD8];
};
static_assert(sizeof(CreateSoundExInfo) == 0xE0);
static_assert(offsetof(CreateSoundExInfo, cbsize) == 0x0);
static_assert(offsetof(CreateSoundExInfo, length) == 0x4);

}  // namespace luautils::audio::fmod
