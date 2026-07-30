#include "CryPakReader.h"

#include "Offsets/vtables/ICryPak.h"
#include "crysystem/SSystemGlobalEnvironment.h"

#include <algorithm>
#include <cctype>
#include <new>
#include <string_view>

namespace luautils::audio {

namespace {

constexpr std::size_t kCryPakMaxPath = 0x800;
constexpr unsigned kFOpenHintQuiet = 1u << 1;

std::string LowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool HasExtension(const std::string& path, CryPakAssetKind kind)
{
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    const std::string extension = LowerAscii(path.substr(dot));
    if (kind == CryPakAssetKind::Bank)
        return extension == ".bank";
    return extension == ".wav" || extension == ".ogg";
}

class CryPakHandle
{
public:
    CryPakHandle(Offsets::ICryPak* pak, void* handle) : m_pak(pak), m_handle(handle) {}
    ~CryPakHandle()
    {
        if (m_handle)
            m_pak->FClose(m_handle);
    }

    void* Get() const { return m_handle; }

private:
    Offsets::ICryPak* m_pak;
    void* m_handle;
};

}  // namespace

bool CryPakReader::Normalize(const char* path, CryPakAssetKind kind,
                             CryPakPath& normalized, std::string& error)
{
    normalized = {};
    if (!path || !*path) {
        error = "CryPak path is empty";
        return false;
    }

    std::string input(path);
    if (input.size() >= kCryPakMaxPath) {
        error = "CryPak path exceeds the engine limit";
        return false;
    }
    std::replace(input.begin(), input.end(), '\\', '/');
    if (input.rfind("//", 0) == 0 || input.find(':') != std::string::npos) {
        error = "CryPak path must be virtual and relative";
        return false;
    }

    std::string output;
    std::size_t cursor = 0;
    while (cursor < input.size() && input[cursor] == '/')
        ++cursor;
    while (cursor <= input.size()) {
        const std::size_t slash = input.find('/', cursor);
        const std::size_t end = slash == std::string::npos ? input.size() : slash;
        const std::string_view component(input.data() + cursor, end - cursor);
        if (component == "..") {
            error = "CryPak path traversal is not allowed";
            return false;
        }
        if (!component.empty() && component != ".") {
            for (unsigned char c : component) {
                if (c < 0x20) {
                    error = "CryPak path contains a control character";
                    return false;
                }
            }
            if (!output.empty())
                output.push_back('/');
            output.append(component);
        }
        if (slash == std::string::npos)
            break;
        cursor = slash + 1;
    }

    if (output.empty()) {
        error = "CryPak path is empty after normalization";
        return false;
    }
    if (!HasExtension(output, kind)) {
        error = kind == CryPakAssetKind::Bank
            ? "bank path must end in .bank"
            : "sound path must end in .wav or .ogg";
        return false;
    }

    normalized.display = std::move(output);
    normalized.key = LowerAscii(normalized.display);
    error.clear();
    return true;
}

bool CryPakReader::ReadAll(const CryPakPath& path, std::size_t maxBytes,
                           std::vector<std::uint8_t>& data, std::string& error)
{
    data.clear();
    auto* env = SSystemGlobalEnvironment::GetInstance();
    Offsets::ICryPak* pak = env ? env->pCryPak : nullptr;
    if (!pak) {
        error = "CryPak is not available";
        return false;
    }

    void* rawHandle = pak->FOpen(path.display.c_str(), "rb", kFOpenHintQuiet);
    if (!rawHandle) {
        error = "CryPak file not found: " + path.display;
        return false;
    }
    CryPakHandle handle(pak, rawHandle);

    const std::size_t size = pak->FGetSize(handle.Get());
    if (size == 0) {
        error = "CryPak audio file is empty: " + path.display;
        return false;
    }
    if (size > maxBytes) {
        error = "CryPak audio file is too large: " + path.display;
        return false;
    }

    try {
        data.resize(size);
    } catch (const std::bad_alloc&) {
        error = "could not allocate memory for: " + path.display;
        return false;
    }

    const std::size_t read = pak->FReadRawAll(data.data(), data.size(), handle.Get());
    if (read != data.size()) {
        data.clear();
        error = "short CryPak read for: " + path.display;
        return false;
    }

    error.clear();
    return true;
}

}  // namespace luautils::audio
