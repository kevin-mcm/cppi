#pragma once

/// @file HostFunction.hpp
/// The callable a host binds to a function name.

#include <cppi/HostCall.hpp>
#include <cppi/Value.hpp>

#include <functional>

namespace cppi {

using HostFunction = std::function<Value(HostCall&)>;

}  // namespace cppi
