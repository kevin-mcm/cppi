#pragma once

// English texts for cppi-run.

#include "MessageCatalog.hpp"

namespace cppi_run {

class EnglishCatalog final : public MessageCatalog {
public:
    [[nodiscard]] std::optional<std::string_view> diagnostic(std::string_view key) const override;
    [[nodiscard]] std::optional<std::string_view> feature(std::string_view key) const override;
    [[nodiscard]] std::optional<std::string_view> ui(std::string_view key) const override;
    [[nodiscard]] std::string_view severity(cppi::Severity severity) const override;
    [[nodiscard]] std::string_view status(cppi::RunStatus status) const override;
};

}  // namespace cppi_run
