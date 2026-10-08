#pragma once

/// @file HostFunction.hpp
/// @brief The callable a host binds to a function name.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostCall.hpp>
#include <cppi/Value.hpp>

#include <functional>

namespace cppi {

/// Host code bound to a function name. It receives the call and returns the
/// result (a void Value for functions that return nothing).
using HostFunction = std::function<Value(HostCall&)>;

}  // namespace cppi
