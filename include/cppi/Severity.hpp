#pragma once

/// @file Severity.hpp
/// How serious a diagnostic is.

#include <cstdint>

namespace cppi {

enum class Severity : std::uint8_t { Error, Warning, Note };

}  // namespace cppi
