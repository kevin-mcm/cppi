#pragma once

/// @file Diagnostic.hpp
/// @brief Diagnostics are data, not text: a stable code plus named arguments.
///
/// The host (the game) turns them into messages in the player's language. This
/// keeps the interpreter free of any localization concerns.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/DiagArg.hpp>
#include <cppi/DiagCode.hpp>
#include <cppi/Severity.hpp>
#include <cppi/SourceRange.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace cppi {

/// One error, warning or note about the player's program: a stable code, its
/// severity, where it happened and its named arguments.
struct Diagnostic {
    /// What went wrong.
    DiagCode code{};
    /// How serious it is.
    Severity severity = Severity::Error;
    /// The part of the player's code it refers to.
    SourceRange range;
    /// Named arguments; which ones each code takes is listed in DiagCode.
    std::vector<DiagArg> args;

    /// Returns the argument named `name`, or nullptr.
    /// @param name Argument name.
    [[nodiscard]] const DiagValue* arg(std::string_view name) const noexcept;

    /// Convenience accessor: the argument rendered as text ("" if missing).
    /// @param name Argument name.
    [[nodiscard]] std::string arg_text(std::string_view name) const;
};

/// A minimal, English, developer-oriented rendering. Games should translate
/// diagnostics themselves; this exists for logs, tests and quick tooling.
/// @param diagnostic The diagnostic to render.
/// @return A one-line English description.
[[nodiscard]] std::string to_debug_string(const Diagnostic& diagnostic);

}  // namespace cppi
