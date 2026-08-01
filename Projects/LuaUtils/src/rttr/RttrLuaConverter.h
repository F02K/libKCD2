#pragma once

#include <string>
#include <vector>

#include "crysystem/ScriptAnyValue.h"
#include "rttr/argument.h"
#include "rttr/method.h"
#include "rttr/type.h"
#include "rttr/variant.h"
#include "rttr/RttrHandleRegistry.h"

namespace Offsets {
struct IFunctionHandler;
}

namespace luautils {

class RttrLuaConverter
{
public:
    explicit RttrLuaConverter(RttrHandleRegistry& registry) noexcept;

    bool GetHandleParam(Offsets::IFunctionHandler* pH, int index,
                        RttrHandleRegistry::Handle& handle,
                        std::string& error) const;
    const rttr::variant* ResolveObject(RttrHandleRegistry::Handle handle,
                                       rttr::type declaringType,
                                       std::string& error) const;
    bool BuildArguments(Offsets::IFunctionHandler* pH, int firstIndex,
                        const rttr::method& method,
                        std::vector<rttr::variant>& values,
                        std::vector<rttr::argument>& arguments,
                        std::string& error) const;
    bool ConvertResult(rttr::variant&& value, rttr::type declaredType,
                       ScriptAnyValue& result, std::string& stringStorage,
                       std::string& error);

    static std::string GetTypeName(rttr::type valueType);

private:
    bool ConvertArgument(const ScriptAnyValue& value, rttr::type expectedType,
                         rttr::variant& result, std::string& error) const;
    bool IsObjectCompatible(rttr::type actualType,
                            rttr::type expectedType) const;

    RttrHandleRegistry& m_registry;
};

}  // namespace luautils
