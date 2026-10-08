#pragma once

// Prints diagnostics compiler-style: file:line:col: error E0200: message,
// followed by the offending source line and a caret.

#include "Messages.hpp"

#include <cppi/Diagnostic.hpp>

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace cppi_run {

class DiagnosticPrinter {
public:
    DiagnosticPrinter(std::ostream& out, const Messages& messages, std::string file, std::string_view source)
        : out_(out), messages_(messages), file_(std::move(file)), source_(source) {}

    void print(const cppi::Diagnostic& d) const;

private:
    [[nodiscard]] std::string_view line_of(std::uint32_t line) const;

    std::ostream& out_;
    const Messages& messages_;
    std::string file_;
    std::string_view source_;
};

}  // namespace cppi_run
