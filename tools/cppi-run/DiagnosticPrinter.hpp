#pragma once

/// @file DiagnosticPrinter.hpp
/// @brief Prints diagnostics compiler-style: file:line:col: error E0200:
/// message, followed by the offending source line and a caret.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "Messages.hpp"

#include <cppi/Diagnostic.hpp>

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace cppi_run {

/// Prints diagnostics in the player's language, with the source line.
class DiagnosticPrinter {
public:
    /// @param out      Where to print.
    /// @param messages Translations.
    /// @param file     File name shown in the location prefix.
    /// @param source   The program's source text.
    DiagnosticPrinter(std::ostream& out, const Messages& messages, std::string file, std::string_view source)
        : out_(out), messages_(messages), file_(std::move(file)), source_(source) {}

    /// Prints one diagnostic.
    void print(const cppi::Diagnostic& d) const;

private:
    /// The text of source line `line` (1-based), "" if out of range.
    [[nodiscard]] std::string_view line_of(std::uint32_t line) const;

    /// Output stream.
    std::ostream& out_;
    /// Translations.
    const Messages& messages_;
    /// File name.
    std::string file_;
    /// Source text.
    std::string_view source_;
};

}  // namespace cppi_run
