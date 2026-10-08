#pragma once

/// @file Severity.hpp
/// @brief How serious a diagnostic is.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi {

/// Severity of a Diagnostic.
enum class Severity : std::uint8_t {
    Error,    ///< the program cannot compile or the run stops
    Warning,  ///< suspicious but valid code; the program still runs
    Note,     ///< extra information attached to the diagnostic before it
};

}  // namespace cppi
