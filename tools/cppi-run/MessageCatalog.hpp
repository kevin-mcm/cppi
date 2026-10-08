#pragma once

// Strategy: one catalog per language. Each catalog only provides raw texts;
// Messages fills in the arguments. This is exactly the job a game does with
// cppi's diagnostics. A real game would load these from translation files
// (Godot CSV or gettext .po); here they are compiled in to keep the tool
// standalone.

#include "Language.hpp"

#include <cppi/RunStatus.hpp>
#include <cppi/Severity.hpp>

#include <memory>
#include <optional>
#include <string_view>

namespace cppi_run {

class MessageCatalog {
public:
    MessageCatalog() = default;
    MessageCatalog(const MessageCatalog&) = default;
    MessageCatalog& operator=(const MessageCatalog&) = default;
    MessageCatalog(MessageCatalog&&) = default;
    MessageCatalog& operator=(MessageCatalog&&) = default;
    virtual ~MessageCatalog();

    /// Factory: the catalog for `language`.
    [[nodiscard]] static std::unique_ptr<MessageCatalog> create(Language language);

    /// Template for a diagnostic key, with {argument} placeholders.
    [[nodiscard]] virtual std::optional<std::string_view> diagnostic(std::string_view key) const = 0;
    /// Name of a language feature ("loops", "los bucles").
    [[nodiscard]] virtual std::optional<std::string_view> feature(std::string_view key) const = 0;
    /// UI string by key ("initial-world", "operations", ...).
    [[nodiscard]] virtual std::optional<std::string_view> ui(std::string_view key) const = 0;
    [[nodiscard]] virtual std::string_view severity(cppi::Severity severity) const = 0;
    [[nodiscard]] virtual std::string_view status(cppi::RunStatus status) const = 0;
};

}  // namespace cppi_run
