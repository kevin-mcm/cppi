#pragma once

/// @file HostRegistry.hpp
/// The host (the game) exposes its world to player code by registering
/// functions and enums here (Registry pattern). The interpreter itself knows
/// nothing about automatons, crops or grids.
///
/// Registration happens once, at setup time. Misuse (duplicate names,
/// invalid identifiers, unknown types) is a programming error in the host and
/// is reported with std::invalid_argument. Nothing in the compile/run path
/// throws.

#include <cppi/FunctionBuilder.hpp>
#include <cppi/FunctionId.hpp>
#include <cppi/HostConstant.hpp>
#include <cppi/HostEnumInfo.hpp>
#include <cppi/HostFunctionInfo.hpp>
#include <cppi/TypeId.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppi {

class HostRegistry {
public:
    using FunctionInfo = HostFunctionInfo;
    using EnumInfo = HostEnumInfo;
    using Constant = HostConstant;

    HostRegistry() = default;

    /// Registers an unscoped (C++98-style) enum. Its enumerators become
    /// constants visible to player code, with values 0, 1, 2, ...
    TypeId add_enum(std::string name, std::initializer_list<std::string_view> enumerators);
    TypeId add_enum(std::string name, const std::vector<std::string>& enumerators);

    /// Starts registering a host function: `host.function("move").param(dir).bind(...)`.
    [[nodiscard]] FunctionBuilder function(std::string name);

    // --- Queries ---------------------------------------------------------
    [[nodiscard]] std::optional<FunctionId> find_function(std::string_view name) const noexcept;
    [[nodiscard]] const FunctionInfo& function(FunctionId id) const noexcept { return functions_[id.value]; }
    [[nodiscard]] std::size_t function_count() const noexcept { return functions_.size(); }

    [[nodiscard]] std::optional<Constant> find_constant(std::string_view name) const noexcept;
    [[nodiscard]] const EnumInfo* find_enum(TypeId id) const noexcept;
    [[nodiscard]] const EnumInfo* find_enum(std::string_view name) const noexcept;

    /// "int", "bool", "void" or the enum's name.
    [[nodiscard]] std::string type_name(TypeId id) const;
    [[nodiscard]] bool is_known_type(TypeId id) const noexcept;

    /// The name of an enumerator, or "" if `value` is not an enumerator of `id`.
    [[nodiscard]] std::string_view enumerator_name(TypeId id, std::int64_t value) const noexcept;

private:
    friend class FunctionBuilder;

    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    template <typename T>
    using NameMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

    void check_new_name(std::string_view name) const;
    FunctionId register_function(FunctionInfo info);

    std::vector<FunctionInfo> functions_;
    std::vector<EnumInfo> enums_;
    NameMap<FunctionId> function_index_;
    NameMap<Constant> constants_;
    NameMap<TypeId> enum_index_;
};

}  // namespace cppi
