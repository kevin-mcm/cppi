#pragma once

/// @file HostRegistry.hpp
/// @brief The host (the game) exposes its world to player code by registering
/// functions and enums here (Registry pattern).
///
/// The interpreter itself knows nothing about automatons, crops or grids.
///
/// Registration happens once, at setup time. Misuse (duplicate names,
/// invalid identifiers, unknown types) is a programming error in the host and
/// is reported with std::invalid_argument. Nothing in the compile/run path
/// throws.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Everything the host exposes to player code: functions, enums and their
/// enumerators. Copyable; an Interpreter keeps its own immutable copy.
class HostRegistry {
public:
    /// Shorter name for HostFunctionInfo.
    using FunctionInfo = HostFunctionInfo;
    /// Shorter name for HostEnumInfo.
    using EnumInfo = HostEnumInfo;
    /// Shorter name for HostConstant.
    using Constant = HostConstant;

    /// An empty registry: only the built-in types are known.
    HostRegistry() = default;

    /// Registers an unscoped (C++98-style) enum. Its enumerators become
    /// constants visible to player code, with values 0, 1, 2, ...
    /// @param name        Enum name, a valid C++ identifier not registered yet.
    /// @param enumerators Enumerator names, each a new valid identifier.
    /// @return The type id of the new enum.
    /// @throws std::invalid_argument if a name is invalid or already taken, or if
    /// there are no enumerators.
    TypeId add_enum(std::string name, std::initializer_list<std::string_view> enumerators);
    /// @copydoc add_enum(std::string, std::initializer_list<std::string_view>)
    TypeId add_enum(std::string name, const std::vector<std::string>& enumerators);

    /// Starts registering a host function: `host.function("move").param(dir).bind(...)`.
    /// @param name Function name, a valid C++ identifier not registered yet.
    /// @throws std::invalid_argument if `name` is invalid or already taken.
    [[nodiscard]] FunctionBuilder function(std::string name);

    // --- Queries ---------------------------------------------------------
    /// The function registered as `name`, or nullopt.
    [[nodiscard]] std::optional<FunctionId> find_function(std::string_view name) const noexcept;
    /// What is registered under `id` (which must come from this registry).
    [[nodiscard]] const FunctionInfo& function(FunctionId id) const noexcept { return functions_[id.value]; }
    /// Number of registered functions.
    [[nodiscard]] std::size_t function_count() const noexcept { return functions_.size(); }

    /// The constant (enumerator) named `name`, or nullopt.
    [[nodiscard]] std::optional<Constant> find_constant(std::string_view name) const noexcept;
    /// The enum with type id `id`, or nullptr if `id` is not a registered enum.
    [[nodiscard]] const EnumInfo* find_enum(TypeId id) const noexcept;
    /// The enum named `name`, or nullptr.
    [[nodiscard]] const EnumInfo* find_enum(std::string_view name) const noexcept;

    /// "int", "bool", "void" or the enum's name.
    [[nodiscard]] std::string type_name(TypeId id) const;
    /// True for the built-in types and every registered enum.
    [[nodiscard]] bool is_known_type(TypeId id) const noexcept;

    /// The name of an enumerator, or "" if `value` is not an enumerator of `id`.
    [[nodiscard]] std::string_view enumerator_name(TypeId id, std::int64_t value) const noexcept;

private:
    friend class FunctionBuilder;

    /// Transparent hash, so maps keyed by std::string can be searched with a
    /// std::string_view without allocating.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    template <typename T>
    /// Map from names to `T` that accepts std::string_view keys in lookups.
    using NameMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

    /// @throws std::invalid_argument if `name` is not a valid identifier or is
    /// already used by a function, constant or enum.
    void check_new_name(std::string_view name) const;
    /// Stores a function described by a FunctionBuilder and returns its id.
    FunctionId register_function(FunctionInfo info);

    std::vector<FunctionInfo> functions_;
    std::vector<EnumInfo> enums_;
    NameMap<FunctionId> function_index_;
    NameMap<Constant> constants_;
    NameMap<TypeId> enum_index_;
};

}  // namespace cppi
