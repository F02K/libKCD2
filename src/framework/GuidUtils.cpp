#include "framework/GuidUtils.h"
#include "Offsets/Offsets.h"

#include <array>
#include <cstdio>
#include <cstring>

// wh GUID text utilities -- KCD2 WHGame.dll 1.5.6 RVAs (verified in utem).

namespace wh {

bool ParseGuid(const char* text, CryGUID& out)
{
    // sub_180719B1C (RVA 0x719B1C): bool(char* text, GUID16* out).
    using Fn = bool(__fastcall*)(const char*, CryGUID*);
    static REL::Relocation<Fn> fn{ REL::ID(39331) };
    return fn(text, &out);
}

std::string FormatGuid(const CryGUID& guid)
{
    std::array<unsigned char, 16> bytes{};
    std::memcpy(bytes.data(), &guid, bytes.size());
    std::array<char, 37> text{};
    std::snprintf(
        text.data(),
        text.size(),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[3], bytes[2], bytes[1], bytes[0],
        bytes[5], bytes[4],
        bytes[7], bytes[6],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return text.data();
}

}  // namespace wh
