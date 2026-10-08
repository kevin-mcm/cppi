#pragma once

/// @file FunctionBuilder.hpp
/// Fluent builder returned by HostRegistry::function(): reads like a
/// signature and is hard to misuse. Registration happens in bind().
///
///     host.function("move").param(direction).cost(5).bind(...);

#include <cppi/FunctionId.hpp>
#include <cppi/HostFunction.hpp>
#include <cppi/HostFunctionInfo.hpp>
#include <cppi/TypeId.hpp>

#include <cstdint>
#include <string>

namespace cppi {

class HostRegistry;

class FunctionBuilder {
public:
    FunctionBuilder& param(TypeId type);
    FunctionBuilder& returns(TypeId type);
    FunctionBuilder& cost(std::uint32_t operations);
    /// Registers the function and returns its id.
    FunctionId bind(HostFunction impl);

private:
    friend class HostRegistry;
    FunctionBuilder(HostRegistry& registry, std::string name);

    HostRegistry* registry_;
    HostFunctionInfo info_;
};

}  // namespace cppi
