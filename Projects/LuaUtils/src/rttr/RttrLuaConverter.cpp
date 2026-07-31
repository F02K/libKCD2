#include "rttr/RttrLuaConverter.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "LuaHelpers.h"
#include "Offsets/vtables/IFunctionHandler.h"
#include "rttr/instance.h"
#include "rttr/parameter_info.h"
#include "rttr/parameter_info_iterator.h"
#include "rttr/parameter_info_range.h"

namespace luautils {
namespace {

enum class ValueKind
{
    Unsupported,
    Bool,
    Char,
    SignedChar,
    UnsignedChar,
    Short,
    UnsignedShort,
    Int,
    UnsignedInt,
    Long,
    UnsignedLong,
    Int64,
    UInt64,
    Float,
    Double,
    LongDouble,
    WChar,
    String,
    Enumeration,
    Object,
    ObjectPointer
};

rttr::type StoredType(const rttr::detail::variant_data* data)
{
    return rttr::type(
        static_cast<rttr::detail::type_data*>(data->m_storage[1]));
}

void* ScalarAddress(rttr::detail::variant_data* data)
{
    return &data->m_storage[0];
}

void* StringAddress(rttr::detail::variant_data* data)
{
    return data->m_storage[0];
}

bool FillAddressContainer(rttr::detail::variant_data* data, void* argument,
                          void* value)
{
    if (!argument)
        return false;
    const rttr::type valueType = StoredType(data);
    auto* container = static_cast<rttr::instance*>(argument);
    container->m_type = valueType;
    container->m_rawType = valueType;
    container->m_ptr = value;
    container->m_rawPtr = value;
    return true;
}

bool ScalarPolicy(std::uint8_t operation,
                  rttr::detail::variant_data* data,
                  void* argument)
{
    using Operation = rttr::detail::variant_policy_operation;
    switch (static_cast<Operation>(operation)) {
    case Operation::destroy:
        return true;
    case Operation::clone:
    case Operation::swap:
        if (!argument)
            return false;
        *static_cast<rttr::detail::variant_data*>(argument) = *data;
        return true;
    case Operation::get_value:
    case Operation::get_ptr:
    case Operation::get_raw_ptr:
        if (!argument)
            return false;
        *static_cast<void**>(argument) = ScalarAddress(data);
        return true;
    case Operation::get_type:
    case Operation::get_raw_type:
        if (!argument)
            return false;
        *static_cast<rttr::type*>(argument) = StoredType(data);
        return true;
    case Operation::get_address_container:
        return FillAddressContainer(data, argument, ScalarAddress(data));
    case Operation::is_valid:
        return StoredType(data).is_valid();
    case Operation::is_nullptr:
        return false;
    default:
        return false;
    }
}

bool StringPolicy(std::uint8_t operation,
                  rttr::detail::variant_data* data,
                  void* argument)
{
    using Operation = rttr::detail::variant_policy_operation;
    switch (static_cast<Operation>(operation)) {
    case Operation::destroy:
        delete static_cast<std::string*>(data->m_storage[0]);
        data->m_storage[0] = nullptr;
        return true;
    case Operation::clone: {
        if (!argument || !data->m_storage[0])
            return false;
        auto* destination = static_cast<rttr::detail::variant_data*>(argument);
        destination->m_storage[0] = new std::string(
            *static_cast<const std::string*>(data->m_storage[0]));
        destination->m_storage[1] = data->m_storage[1];
        return true;
    }
    case Operation::swap:
        if (!argument)
            return false;
        *static_cast<rttr::detail::variant_data*>(argument) = *data;
        return true;
    case Operation::get_value:
    case Operation::get_ptr:
    case Operation::get_raw_ptr:
        if (!argument)
            return false;
        *static_cast<void**>(argument) = StringAddress(data);
        return true;
    case Operation::get_type:
    case Operation::get_raw_type:
        if (!argument)
            return false;
        *static_cast<rttr::type*>(argument) = StoredType(data);
        return true;
    case Operation::get_address_container:
        return FillAddressContainer(data, argument, StringAddress(data));
    case Operation::is_valid:
        return data->m_storage[0] && StoredType(data).is_valid();
    case Operation::is_nullptr:
        return false;
    default:
        return false;
    }
}

template <class T>
rttr::variant MakeScalarVariant(T value, rttr::type valueType)
{
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) <= sizeof(void*));

    rttr::detail::variant_data data{};
    std::memcpy(&data.m_storage[0], &value, sizeof(value));
    data.m_storage[1] = valueType.m_type_data;
    return rttr::variant::from_policy(data, &ScalarPolicy);
}

rttr::variant MakeStringVariant(const char* value, rttr::type valueType)
{
    rttr::detail::variant_data data{};
    data.m_storage[0] = new std::string(value);
    data.m_storage[1] = valueType.m_type_data;
    return rttr::variant::from_policy(data, &StringPolicy);
}

template <class T>
bool ReadIntegral(float number, T& value, std::string& error)
{
    static_assert(std::is_integral_v<T>);
    const double converted = static_cast<double>(number);
    if (std::trunc(converted) != converted) {
        error = "argument must be an integral number";
        return false;
    }
    if (converted < static_cast<double>(std::numeric_limits<T>::lowest()) ||
        converted > static_cast<double>(std::numeric_limits<T>::max())) {
        error = "integral argument is outside the reflected type's range";
        return false;
    }
    value = static_cast<T>(converted);
    return true;
}

template <class T>
T ReadScalarValue(const rttr::variant& value)
{
    T result{};
    if (void* source = value.get_value())
        std::memcpy(&result, source, sizeof(result));
    return result;
}

std::string_view RawTypeName(rttr::type valueType)
{
    if (!valueType.m_type_data)
        return {};
    return valueType.m_type_data->type_name;
}

bool IsStdString(rttr::type valueType)
{
    if (!valueType.is_class() || valueType.get_sizeof() != sizeof(std::string))
        return false;

    std::string compact;
    const std::string_view rawName = RawTypeName(valueType);
    compact.reserve(rawName.size());
    for (char character : rawName) {
        if (character != ' ' && character != '\t' &&
            character != '\r' && character != '\n')
            compact.push_back(character);
    }
    return compact ==
        "classstd::basic_string<char,structstd::char_traits<char>,classstd::allocator<char>>";
}

bool IsContainer(rttr::type valueType)
{
    if (!valueType.m_type_data)
        return false;
    const auto& traits = valueType.m_type_data->m_type_traits;
    return traits.test(static_cast<std::size_t>(
               rttr::detail::type_trait_infos::is_associative_container)) ||
           traits.test(static_cast<std::size_t>(
               rttr::detail::type_trait_infos::is_sequential_container));
}

ValueKind Classify(rttr::type valueType)
{
    if (!valueType.is_valid())
        return ValueKind::Unsupported;
    if (valueType.is_arithmetic()) {
        const std::string_view name = RawTypeName(valueType);
        if (name == "bool") return ValueKind::Bool;
        if (name == "char") return ValueKind::Char;
        if (name == "signed char") return ValueKind::SignedChar;
        if (name == "unsigned char") return ValueKind::UnsignedChar;
        if (name == "short") return ValueKind::Short;
        if (name == "unsigned short") return ValueKind::UnsignedShort;
        if (name == "int") return ValueKind::Int;
        if (name == "unsigned int") return ValueKind::UnsignedInt;
        if (name == "long") return ValueKind::Long;
        if (name == "unsigned long") return ValueKind::UnsignedLong;
        if (name == "__int64") return ValueKind::Int64;
        if (name == "unsigned __int64") return ValueKind::UInt64;
        if (name == "float") return ValueKind::Float;
        if (name == "double") return ValueKind::Double;
        if (name == "long double") return ValueKind::LongDouble;
        if (name == "wchar_t") return ValueKind::WChar;
        return ValueKind::Unsupported;
    }
    if (valueType.is_enumeration())
        return ValueKind::Enumeration;
    if (IsStdString(valueType))
        return ValueKind::String;
    if (valueType.is_pointer())
        return valueType.get_raw_type().is_class()
            ? ValueKind::ObjectPointer
            : ValueKind::Unsupported;
    if (valueType.is_class() && !IsContainer(valueType))
        return ValueKind::Object;
    return ValueKind::Unsupported;
}

bool ReadAny(Offsets::IFunctionHandler* pH, int index,
             ScriptAnyValue& value, std::string& error)
{
    if (index < 1 || index > pH->GetParamCount()) {
        error = "missing required argument";
        return false;
    }
    value.type = ANY_ANY;
    value.table = nullptr;
    if (!pH->GetParamAny(index, value)) {
        error = "could not read argument";
        return false;
    }
    return true;
}

bool ReadRegistryToken(const ScriptAnyValue& value,
                       RttrHandleRegistry::Handle& handle,
                       std::string& error)
{
    if (value.type != ANY_THANDLE || value.nHandle == 0) {
        error = "argument must be a nonzero RTTR handle";
        return false;
    }
    handle = static_cast<RttrHandleRegistry::Handle>(value.nHandle);
    return true;
}

bool RequireExactResultType(const rttr::variant& value,
                            rttr::type declaredType,
                            std::string& error)
{
    const rttr::type actualType = value.get_type();
    if (actualType == declaredType)
        return true;
    error = "reflected result type '" + RttrLuaConverter::GetTypeName(actualType) +
            "' does not match declared type '" +
            RttrLuaConverter::GetTypeName(declaredType) + "'";
    return false;
}

}  // namespace

RttrLuaConverter::RttrLuaConverter(RttrHandleRegistry& registry) noexcept
    : m_registry(registry)
{}

bool RttrLuaConverter::GetHandleParam(Offsets::IFunctionHandler* pH, int index,
                                      RttrHandleRegistry::Handle& handle,
                                      std::string& error) const
{
    ScriptAnyValue value;
    return ReadAny(pH, index, value, error) &&
           ReadRegistryToken(value, handle, error);
}

const rttr::variant* RttrLuaConverter::ResolveObject(
    RttrHandleRegistry::Handle handle, rttr::type declaringType,
    std::string& error) const
{
    if (!declaringType.is_valid() || !declaringType.is_class()) {
        error = "declaring type is not a reflected class";
        return nullptr;
    }

    const rttr::variant* value = m_registry.Lookup(handle);
    if (!value) {
        error = "unknown or stale RTTR handle";
        return nullptr;
    }

    const rttr::type actualType = value->get_type();
    rttr::type objectType;
    if (actualType.is_pointer())
        objectType = actualType.get_raw_type();
    else if (actualType.is_class())
        objectType = actualType;
    else {
        error = "RTTR handle does not contain an object";
        return nullptr;
    }

    if (objectType != declaringType && !objectType.is_derived_from(declaringType)) {
        error = "RTTR handle type '" + GetTypeName(actualType) +
                "' is incompatible with declaring type '" +
                GetTypeName(declaringType) + "'";
        return nullptr;
    }
    return value;
}

bool RttrLuaConverter::BuildArguments(
    Offsets::IFunctionHandler* pH, int firstIndex, const rttr::method& method,
    std::vector<rttr::variant>& values,
    std::vector<rttr::argument>& arguments,
    std::string& error) const
{
    rttr::parameter_info_range parameters = method.get_parameter_infos();
    const std::size_t parameterCount = parameters.size();
    const int suppliedCount = pH->GetParamCount() >= firstIndex
        ? pH->GetParamCount() - firstIndex + 1
        : 0;
    if (suppliedCount > static_cast<int>(parameterCount)) {
        error = "too many arguments for reflected method";
        return false;
    }

    values.clear();
    arguments.clear();
    values.reserve(parameterCount);
    arguments.reserve(parameterCount);

    std::size_t parameterIndex = 0;
    for (auto iterator = parameters.begin(); iterator != parameters.end();
         ++iterator, ++parameterIndex) {
        const rttr::parameter_info& parameter = *iterator;
        const rttr::type expectedType = parameter.get_type();
        rttr::variant converted;
        std::string conversionError;

        if (parameterIndex < static_cast<std::size_t>(suppliedCount)) {
            ScriptAnyValue input;
            if (!ReadAny(pH, firstIndex + static_cast<int>(parameterIndex),
                         input, conversionError) ||
                !ConvertArgument(input, expectedType, converted,
                                 conversionError)) {
                const std::string parameterName(parameter.get_name());
                error = "argument " + std::to_string(parameterIndex + 1);
                if (!parameterName.empty())
                    error += " ('" + parameterName + "')";
                error += ": " + conversionError;
                return false;
            }
        } else {
            if (!parameter.has_default_value()) {
                error = "missing required argument " +
                        std::to_string(parameterIndex + 1);
                return false;
            }
            converted = parameter.get_default_value();
            if (!converted.is_valid()) {
                error = "reflected default for argument " +
                        std::to_string(parameterIndex + 1) + " is invalid";
                return false;
            }
        }
        values.emplace_back(std::move(converted));
    }

    for (const rttr::variant& value : values)
        arguments.emplace_back(value);
    return true;
}

bool RttrLuaConverter::ConvertArgument(const ScriptAnyValue& value,
                                       rttr::type expectedType,
                                       rttr::variant& result,
                                       std::string& error) const
{
    const ValueKind kind = Classify(expectedType);
    if (kind == ValueKind::LongDouble) {
        error = "long double parameters are not supported";
        return false;
    }
    if (kind == ValueKind::WChar) {
        error = "wchar_t parameters are not supported";
        return false;
    }

    if (kind == ValueKind::Bool) {
        if (value.type != ANY_TBOOLEAN) {
            error = "argument must be a Boolean";
            return false;
        }
        result = MakeScalarVariant(value.b, expectedType);
        return true;
    }

    if (kind == ValueKind::Int64 || kind == ValueKind::UInt64) {
        if (value.type != ANY_THANDLE) {
            error = "64-bit arithmetic arguments require ScriptHandle transport";
            return false;
        }
        if (kind == ValueKind::Int64)
            result = MakeScalarVariant(static_cast<std::int64_t>(value.nHandle),
                                       expectedType);
        else
            result = MakeScalarVariant(static_cast<std::uint64_t>(value.nHandle),
                                       expectedType);
        return true;
    }

    if (kind == ValueKind::Float || kind == ValueKind::Double) {
        if (value.type != ANY_TNUMBER || !std::isfinite(value.number)) {
            error = "argument must be a finite number";
            return false;
        }
        if (kind == ValueKind::Float)
            result = MakeScalarVariant(value.number, expectedType);
        else
            result = MakeScalarVariant(static_cast<double>(value.number),
                                       expectedType);
        return true;
    }

    if (kind >= ValueKind::Char && kind <= ValueKind::UnsignedLong) {
        if (value.type != ANY_TNUMBER || !std::isfinite(value.number)) {
            error = "argument must be a finite number";
            return false;
        }
        switch (kind) {
        case ValueKind::Char: {
            char converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::SignedChar: {
            signed char converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::UnsignedChar: {
            unsigned char converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::Short: {
            short converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::UnsignedShort: {
            unsigned short converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::Int: {
            int converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::UnsignedInt: {
            unsigned int converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::Long: {
            long converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        case ValueKind::UnsignedLong: {
            unsigned long converted{};
            if (!ReadIntegral(value.number, converted, error)) return false;
            result = MakeScalarVariant(converted, expectedType);
            return true;
        }
        default:
            break;
        }
    }

    if (kind == ValueKind::String) {
        if (value.type != ANY_TSTRING || !value.str) {
            error = "argument must be a string";
            return false;
        }
        result = MakeStringVariant(value.str, expectedType);
        return true;
    }

    if (kind == ValueKind::Enumeration || kind == ValueKind::Object ||
        kind == ValueKind::ObjectPointer) {
        RttrHandleRegistry::Handle handle = 0;
        if (!ReadRegistryToken(value, handle, error))
            return false;
        const rttr::variant* stored = m_registry.Lookup(handle);
        if (!stored) {
            error = "unknown or stale RTTR handle";
            return false;
        }
        const rttr::type actualType = stored->get_type();
        if (kind == ValueKind::Enumeration) {
            if (actualType != expectedType) {
                error = "enum handle type '" + GetTypeName(actualType) +
                        "' does not match expected type '" +
                        GetTypeName(expectedType) + "'";
                return false;
            }
        } else if (!IsObjectCompatible(actualType, expectedType)) {
            error = "object handle type '" + GetTypeName(actualType) +
                    "' is incompatible with expected type '" +
                    GetTypeName(expectedType) + "'";
            return false;
        }
        result = *stored;
        return true;
    }

    if (value.type == ANY_TNIL)
        error = "nil cannot be converted to reflected type '" +
                GetTypeName(expectedType) + "'";
    else
        error = "reflected type '" + GetTypeName(expectedType) +
                "' is not supported by the Lua RTTR bridge";
    return false;
}

bool RttrLuaConverter::ConvertResult(rttr::variant&& value,
                                     rttr::type declaredType,
                                     ScriptAnyValue& result,
                                     std::string& stringStorage,
                                     std::string& error)
{
    if (!declaredType.is_valid()) {
        if (value.is_valid()) {
            error = "void reflected callable returned a value";
            return false;
        }
        result = ScriptAnyValue(true);
        return true;
    }
    if (!value.is_valid()) {
        error = "reflected callable returned an invalid non-void result";
        return false;
    }

    const ValueKind kind = Classify(declaredType);
    if (kind == ValueKind::LongDouble) {
        error = "long double results are not supported";
        return false;
    }
    if (kind == ValueKind::WChar) {
        error = "wchar_t results are not supported";
        return false;
    }

    if (kind == ValueKind::ObjectPointer && value.is_nullptr()) {
        result.type = ANY_TNIL;
        result.nHandle = 0;
        return true;
    }

    if (kind != ValueKind::Object && kind != ValueKind::ObjectPointer &&
        !RequireExactResultType(value, declaredType, error))
        return false;

    switch (kind) {
    case ValueKind::Bool:
        result = ScriptAnyValue(ReadScalarValue<bool>(value));
        return true;
    case ValueKind::Char:
        result = ScriptAnyValue(static_cast<int>(ReadScalarValue<char>(value)));
        return true;
    case ValueKind::SignedChar:
        result = ScriptAnyValue(static_cast<int>(ReadScalarValue<signed char>(value)));
        return true;
    case ValueKind::UnsignedChar:
        result = ScriptAnyValue(static_cast<unsigned int>(
            ReadScalarValue<unsigned char>(value)));
        return true;
    case ValueKind::Short:
        result = ScriptAnyValue(static_cast<int>(ReadScalarValue<short>(value)));
        return true;
    case ValueKind::UnsignedShort:
        result = ScriptAnyValue(static_cast<unsigned int>(
            ReadScalarValue<unsigned short>(value)));
        return true;
    case ValueKind::Int:
        result = ScriptAnyValue(ReadScalarValue<int>(value));
        return true;
    case ValueKind::UnsignedInt:
        result = ScriptAnyValue(ReadScalarValue<unsigned int>(value));
        return true;
    case ValueKind::Long:
        result = ScriptAnyValue(static_cast<int>(ReadScalarValue<long>(value)));
        return true;
    case ValueKind::UnsignedLong:
        result = ScriptAnyValue(static_cast<unsigned int>(
            ReadScalarValue<unsigned long>(value)));
        return true;
    case ValueKind::Int64:
        result = HandleValue(static_cast<std::uint64_t>(
            ReadScalarValue<std::int64_t>(value)));
        return true;
    case ValueKind::UInt64:
        result = HandleValue(ReadScalarValue<std::uint64_t>(value));
        return true;
    case ValueKind::Float:
        result = ScriptAnyValue(ReadScalarValue<float>(value));
        return true;
    case ValueKind::Double:
        result = ScriptAnyValue(ReadScalarValue<double>(value));
        return true;
    case ValueKind::String: {
        const auto* text = static_cast<const std::string*>(value.get_value());
        if (!text) {
            error = "reflected string result has no value";
            return false;
        }
        stringStorage = *text;
        result = ScriptAnyValue(stringStorage.c_str());
        return true;
    }
    case ValueKind::Enumeration: {
        const RttrHandleRegistry::Handle handle =
            m_registry.Store(std::move(value));
        result = HandleValue(handle);
        return true;
    }
    case ValueKind::Object:
    case ValueKind::ObjectPointer: {
        const rttr::type actualType = value.get_type();
        if (!IsObjectCompatible(actualType, declaredType)) {
            error = "reflected result type '" + GetTypeName(actualType) +
                    "' is incompatible with declared type '" +
                    GetTypeName(declaredType) + "'";
            return false;
        }
        const RttrHandleRegistry::Handle handle =
            m_registry.Store(std::move(value));
        result = HandleValue(handle);
        return true;
    }
    default:
        error = "reflected result type '" + GetTypeName(declaredType) +
                "' is not supported by the Lua RTTR bridge";
        return false;
    }
}

bool RttrLuaConverter::IsObjectCompatible(rttr::type actualType,
                                          rttr::type expectedType) const
{
    if (expectedType.is_pointer()) {
        if (!actualType.is_pointer())
            return false;
        const rttr::type actualRaw = actualType.get_raw_type();
        const rttr::type expectedRaw = expectedType.get_raw_type();
        return actualType == expectedType || actualRaw == expectedRaw ||
               actualRaw.is_derived_from(expectedRaw);
    }
    if (!expectedType.is_class() || actualType.is_pointer() ||
        !actualType.is_class())
        return false;
    return actualType == expectedType || actualType.is_derived_from(expectedType);
}

std::string RttrLuaConverter::GetTypeName(rttr::type valueType)
{
    if (!valueType.is_valid())
        return "<invalid>";
    const rttr::string_view name = valueType.get_name();
    if (!name.empty())
        return std::string(name);
    return std::string(RawTypeName(valueType));
}

}  // namespace luautils
