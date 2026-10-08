#pragma once

/// @file EnglishCatalog.hpp
/// @brief English texts for cppi-run.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "MessageCatalog.hpp"

namespace cppi_run {

/// English messages.
class EnglishCatalog final : public MessageCatalog {
public:
    [[nodiscard]] std::optional<std::string_view> diagnostic(std::string_view key) const override;
    [[nodiscard]] std::optional<std::string_view> feature(std::string_view key) const override;
    [[nodiscard]] std::optional<std::string_view> ui(std::string_view key) const override;
    [[nodiscard]] std::string_view severity(cppi::Severity severity) const override;
    [[nodiscard]] std::string_view status(cppi::RunStatus status) const override;
};

}  // namespace cppi_run
