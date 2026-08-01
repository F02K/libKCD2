#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace luautils::audio {

enum class CryPakAssetKind {
    Bank,
    Sound,
};

struct CryPakPath {
    std::string display;
    std::string key;
};

class CryPakReader
{
public:
    static bool Normalize(const char* path, CryPakAssetKind kind,
                          CryPakPath& normalized, std::string& error);
    static bool ReadAll(const CryPakPath& path, std::size_t maxBytes,
                        std::vector<std::uint8_t>& data, std::string& error);
};

}  // namespace luautils::audio
