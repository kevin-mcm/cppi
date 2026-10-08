#pragma once

/// @file Diagnostic.hpp
/// Diagnostics are data, not text: a stable code plus named arguments. The
/// host (the game) turns them into messages in the player's language. This
/// keeps the interpreter free of any localization concerns.

#include <cppi/DiagArg.hpp>
#include <cppi/DiagCode.hpp>
#include <cppi/Severity.hpp>
#include <cppi/SourceRange.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace cppi {

struct Diagnostic {
    DiagCode code{};
    Severity severity = Severity::Error;
    SourceRange range;
    std::vector<DiagArg> args;

    /// Returns the argument named `name`, or nullptr.
    [[nodiscard]] const DiagValue* arg(std::string_view name) const noexcept;

    /// Convenience accessor: the argument rendered as text ("" if missing).
    [[nodiscard]] std::string arg_text(std::string_view name) const;
};

/// A minimal, English, developer-oriented rendering. Games should translate
/// diagnostics themselves; this exists for logs, tests and quick tooling.
[[nodiscard]] std::string to_debug_string(const Diagnostic& diagnostic);

}  // namespace cppi
