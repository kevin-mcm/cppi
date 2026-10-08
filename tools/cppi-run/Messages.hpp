#pragma once

// Turns cppi's diagnostics (codes + arguments) into text in the player's
// language, using the MessageCatalog for that language.

#include "Language.hpp"
#include "MessageCatalog.hpp"

#include <cppi/Diagnostic.hpp>
#include <cppi/RunStatus.hpp>
#include <cppi/Severity.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace cppi_run {

class Messages {
public:
    explicit Messages(Language language) : catalog_(MessageCatalog::create(language)) {}

    /// The translated diagnostic message, without location prefix.
    [[nodiscard]] std::string diagnostic(const cppi::Diagnostic& d) const;

    /// "error", "advertencia", ...
    [[nodiscard]] std::string severity(cppi::Severity s) const;

    /// Translated name of a language feature ("bucles", "loops").
    [[nodiscard]] std::string feature(std::string_view key) const;

    [[nodiscard]] std::string status(cppi::RunStatus s) const;

    /// UI string by key ("initial-world", "operations", ...).
    [[nodiscard]] std::string ui(std::string_view key) const;

private:
    std::unique_ptr<MessageCatalog> catalog_;
};

}  // namespace cppi_run
