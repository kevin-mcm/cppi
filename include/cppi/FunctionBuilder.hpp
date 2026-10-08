#pragma once

/// @file FunctionBuilder.hpp
/// @brief Fluent builder returned by HostRegistry::function(): reads like a
/// signature and is hard to misuse.
///
/// Registration happens in bind().
///
///     host.function("move").param(direction).cost(5).bind(...);
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/FunctionId.hpp>
#include <cppi/HostFunction.hpp>
#include <cppi/HostFunctionInfo.hpp>
#include <cppi/TypeId.hpp>

#include <cstdint>
#include <string>

namespace cppi {

class HostRegistry;

/// Describes the signature and cost of a host function before binding it.
class FunctionBuilder {
public:
    /// Appends a parameter of type `type`.
    /// @return `*this`, to keep chaining.
    FunctionBuilder& param(TypeId type);
    /// Sets the return type (Void by default).
    /// @return `*this`, to keep chaining.
    FunctionBuilder& returns(TypeId type);
    /// Sets the extra operations charged each time the function is called.
    /// @return `*this`, to keep chaining.
    FunctionBuilder& cost(std::uint32_t operations);
    /// Registers the function and returns its id.
    /// @param impl The host code that runs when player code calls the function.
    FunctionId bind(HostFunction impl);

private:
    friend class HostRegistry;
    /// Only HostRegistry::function() creates builders.
    FunctionBuilder(HostRegistry& registry, std::string name);

    HostRegistry* registry_;
    HostFunctionInfo info_;
};

}  // namespace cppi
