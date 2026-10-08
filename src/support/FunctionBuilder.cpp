/// @file FunctionBuilder.cpp
/// @brief Implementation of FunctionBuilder.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/FunctionBuilder.hpp>
#include <cppi/HostRegistry.hpp>

#include <stdexcept>
#include <utility>

namespace cppi {

FunctionBuilder::FunctionBuilder(HostRegistry& registry, std::string name) : registry_(&registry) {
    info_.name = std::move(name);
}

FunctionBuilder& FunctionBuilder::param(TypeId type) {
    if (type == types::Void || !registry_->is_known_type(type)) {
        throw std::invalid_argument("cppi: invalid parameter type for host function '" + info_.name + "'");
    }
    info_.params.push_back(type);
    return *this;
}

FunctionBuilder& FunctionBuilder::returns(TypeId type) {
    if (!registry_->is_known_type(type)) {
        throw std::invalid_argument("cppi: invalid return type for host function '" + info_.name + "'");
    }
    info_.result = type;
    return *this;
}

FunctionBuilder& FunctionBuilder::cost(std::uint32_t operations) {
    info_.cost = operations;
    return *this;
}

FunctionId FunctionBuilder::bind(HostFunction impl) {
    if (!impl) {
        throw std::invalid_argument("cppi: host function '" + info_.name + "' has no implementation");
    }
    info_.impl = std::move(impl);
    return registry_->register_function(std::move(info_));
}

}  // namespace cppi
