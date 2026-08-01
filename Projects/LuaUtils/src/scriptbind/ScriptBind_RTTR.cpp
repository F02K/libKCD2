#include "scriptbind/ScriptBind_RTTR.h"

#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "LuaHelpers.h"
#include "Offsets/vtables/IFunctionHandler.h"
#include "Offsets/vtables/IScriptSystem.h"
#include "Offsets/vtables/IScriptTable.h"
#include "rttr/enumeration.h"
#include "rttr/instance.h"
#include "rttr/method.h"
#include "rttr/property.h"
#include "rttr/type.h"
#include "rttr/variant.h"

namespace luautils {
namespace {

ScriptAnyValue NilValue()
{
    ScriptAnyValue value;
    value.type = ANY_TNIL;
    value.nHandle = 0;
    return value;
}

int ReturnError(Offsets::IFunctionHandler* pH, const std::string& error)
{
    const char* message = error.empty()
        ? "RTTR operation failed"
        : error.c_str();
    return pH->EndFunctionAny2(NilValue(), ScriptAnyValue(message));
}

int ReturnTrue(Offsets::IFunctionHandler* pH)
{
    return pH->EndFunctionAny(ScriptAnyValue(true));
}

template <class Callback>
int Safely(Offsets::IFunctionHandler* pH, Callback&& callback)
{
    try {
        return callback();
    } catch (const std::exception& exception) {
        return ReturnError(pH, exception.what());
    } catch (...) {
        return ReturnError(pH, "unknown native exception during RTTR operation");
    }
}

bool ReadStringParam(Offsets::IFunctionHandler* pH, int index,
                     std::string& value, std::string& error)
{
    if (index < 1 || index > pH->GetParamCount()) {
        error = "missing required argument";
        return false;
    }
    ScriptAnyValue any;
    any.type = ANY_ANY;
    any.table = nullptr;
    if (!pH->GetParamAny(index, any)) {
        error = "could not read argument";
        return false;
    }
    if (any.type != ANY_TSTRING || !any.str) {
        error = "argument must be a string";
        return false;
    }
    value = any.str;
    return true;
}

bool RequireParamCount(Offsets::IFunctionHandler* pH, int minimum,
                       int maximum, std::string& error)
{
    const int count = pH->GetParamCount();
    if (count < minimum) {
        error = "missing required argument";
        return false;
    }
    if (maximum >= 0 && count > maximum) {
        error = "too many arguments";
        return false;
    }
    return true;
}

int ReturnConverted(Offsets::IFunctionHandler* pH,
                    RttrLuaConverter& converter,
                    rttr::variant&& value,
                    rttr::type declaredType)
{
    ScriptAnyValue result;
    std::string stringStorage;
    std::string error;
    if (!converter.ConvertResult(std::move(value), declaredType, result,
                                 stringStorage, error))
        return ReturnError(pH, error);
    return pH->EndFunctionAny(result);
}

}  // namespace

CScriptBind_RTTR::CScriptBind_RTTR() noexcept
    : m_converter(m_registry)
{}

void CScriptBind_RTTR::Init(Offsets::IScriptSystem* pSS)
{
    if (!pSS || m_pMethodsTable)
        return;

    m_pSS = pSS;
    m_pMethodsTable = pSS->CreateTable(0, 0);
    if (!m_pMethodsTable)
        return;
    m_pMethodsTable->AddRef();

    RegisterFunction("CallGlobal", "methodName, ...",
                     functor(*this, &CScriptBind_RTTR::CallGlobal));
    RegisterFunction("CallMethod",
                     "objectHandle, declaringType, methodName, ...",
                     functor(*this, &CScriptBind_RTTR::CallMethod));
    RegisterFunction("GetProperty",
                     "objectHandle, declaringType, propertyName",
                     functor(*this, &CScriptBind_RTTR::GetProperty));
    RegisterFunction("GetEnum", "enumType, valueName",
                     functor(*this, &CScriptBind_RTTR::GetEnum));
    RegisterFunction("GetTypeName", "handle",
                     functor(*this, &CScriptBind_RTTR::GetTypeName));
    RegisterFunction("Release", "handle",
                     functor(*this, &CScriptBind_RTTR::Release));
    RegisterFunction("Clear", "",
                     functor(*this, &CScriptBind_RTTR::Clear));

    m_pSS->SetGlobalAny("RTTR", ScriptAnyValue(m_pMethodsTable));
}

void CScriptBind_RTTR::RegisterFunction(const char* name, const char* params,
                                        const FunctionFunctor& function)
{
    SUserFunctionDesc descriptor;
    descriptor.sGlobalName = "RTTR";
    descriptor.sFunctionName = name;
    descriptor.sFunctionParams = params;
    descriptor.pFunctor = function;
    m_pMethodsTable->AddFunction(descriptor);
}

int CScriptBind_RTTR::CallGlobal(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        std::string methodName;
        if (!RequireParamCount(pH, 1, -1, error) ||
            !ReadStringParam(pH, 1, methodName, error))
            return ReturnError(pH, error);

        const rttr::method method = rttr::type::get_global_method(methodName);
        if (!method.is_valid())
            return ReturnError(pH, "unknown reflected global method '" +
                                      methodName + "'");

        std::vector<rttr::variant> values;
        std::vector<rttr::argument> arguments;
        if (!m_converter.BuildArguments(pH, 2, method, values, arguments,
                                        error))
            return ReturnError(pH, error);

        rttr::instance object;
        rttr::variant result = method.invoke_variadic(object, arguments);
        return ReturnConverted(pH, m_converter, std::move(result),
                               method.get_return_type());
    });
}

int CScriptBind_RTTR::CallMethod(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 3, -1, error))
            return ReturnError(pH, error);

        RttrHandleRegistry::Handle handle = 0;
        std::string declaringTypeName;
        std::string methodName;
        if (!m_converter.GetHandleParam(pH, 1, handle, error) ||
            !ReadStringParam(pH, 2, declaringTypeName, error) ||
            !ReadStringParam(pH, 3, methodName, error))
            return ReturnError(pH, error);

        const rttr::type declaringType =
            rttr::type::get_by_name(declaringTypeName);
        if (!declaringType.is_valid())
            return ReturnError(pH, "unknown reflected type '" +
                                      declaringTypeName + "'");

        const rttr::variant* objectValue =
            m_converter.ResolveObject(handle, declaringType, error);
        if (!objectValue)
            return ReturnError(pH, error);

        const rttr::method method = declaringType.get_method(methodName);
        if (!method.is_valid())
            return ReturnError(pH, "unknown reflected method '" + methodName +
                                      "' on type '" + declaringTypeName + "'");

        std::vector<rttr::variant> values;
        std::vector<rttr::argument> arguments;
        if (!m_converter.BuildArguments(pH, 4, method, values, arguments,
                                        error))
            return ReturnError(pH, error);

        rttr::instance object(*objectValue);
        rttr::variant result = method.invoke_variadic(object, arguments);
        return ReturnConverted(pH, m_converter, std::move(result),
                               method.get_return_type());
    });
}

int CScriptBind_RTTR::GetProperty(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 3, 3, error))
            return ReturnError(pH, error);

        RttrHandleRegistry::Handle handle = 0;
        std::string declaringTypeName;
        std::string propertyName;
        if (!m_converter.GetHandleParam(pH, 1, handle, error) ||
            !ReadStringParam(pH, 2, declaringTypeName, error) ||
            !ReadStringParam(pH, 3, propertyName, error))
            return ReturnError(pH, error);

        const rttr::type declaringType =
            rttr::type::get_by_name(declaringTypeName);
        if (!declaringType.is_valid())
            return ReturnError(pH, "unknown reflected type '" +
                                      declaringTypeName + "'");

        const rttr::variant* objectValue =
            m_converter.ResolveObject(handle, declaringType, error);
        if (!objectValue)
            return ReturnError(pH, error);

        const rttr::property property =
            declaringType.get_property(propertyName);
        if (!property.is_valid())
            return ReturnError(pH, "unknown reflected property '" +
                                      propertyName + "' on type '" +
                                      declaringTypeName + "'");

        rttr::instance object(*objectValue);
        rttr::variant result = property.get_value(object);
        return ReturnConverted(pH, m_converter, std::move(result),
                               property.get_type());
    });
}

int CScriptBind_RTTR::GetEnum(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 2, 2, error))
            return ReturnError(pH, error);

        std::string enumTypeName;
        std::string valueName;
        if (!ReadStringParam(pH, 1, enumTypeName, error) ||
            !ReadStringParam(pH, 2, valueName, error))
            return ReturnError(pH, error);

        const rttr::type enumType = rttr::type::get_by_name(enumTypeName);
        if (!enumType.is_valid())
            return ReturnError(pH, "unknown reflected type '" +
                                      enumTypeName + "'");
        if (!enumType.is_enumeration())
            return ReturnError(pH, "reflected type '" + enumTypeName +
                                      "' is not an enumeration");

        const rttr::enumeration enumeration = enumType.get_enumeration();
        if (!enumeration.is_valid())
            return ReturnError(pH, "reflected enumeration '" + enumTypeName +
                                      "' has no registered values");

        rttr::variant result = enumeration.name_to_value(valueName);
        if (!result.is_valid())
            return ReturnError(pH, "unknown value '" + valueName +
                                      "' for reflected enumeration '" +
                                      enumTypeName + "'");
        return ReturnConverted(pH, m_converter, std::move(result), enumType);
    });
}

int CScriptBind_RTTR::GetTypeName(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 1, 1, error))
            return ReturnError(pH, error);

        RttrHandleRegistry::Handle handle = 0;
        if (!m_converter.GetHandleParam(pH, 1, handle, error))
            return ReturnError(pH, error);
        const rttr::variant* value = m_registry.Lookup(handle);
        if (!value)
            return ReturnError(pH, "unknown or stale RTTR handle");

        const std::string typeName =
            RttrLuaConverter::GetTypeName(value->get_type());
        return pH->EndFunctionAny(ScriptAnyValue(typeName.c_str()));
    });
}

int CScriptBind_RTTR::Release(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 1, 1, error))
            return ReturnError(pH, error);

        RttrHandleRegistry::Handle handle = 0;
        if (!m_converter.GetHandleParam(pH, 1, handle, error))
            return ReturnError(pH, error);
        if (!m_registry.Release(handle))
            return ReturnError(pH, "unknown or stale RTTR handle");
        return ReturnTrue(pH);
    });
}

int CScriptBind_RTTR::Clear(Offsets::IFunctionHandler* pH)
{
    return Safely(pH, [&]() {
        std::string error;
        if (!RequireParamCount(pH, 0, 0, error))
            return ReturnError(pH, error);
        m_registry.Clear();
        return ReturnTrue(pH);
    });
}

}  // namespace luautils
