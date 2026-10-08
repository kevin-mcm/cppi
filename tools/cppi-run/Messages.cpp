/// @file Messages.cpp
/// @brief Implementation of Messages.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "Messages.hpp"

#include "TemplateFormatter.hpp"

namespace cppi_run {

std::string Messages::diagnostic(const cppi::Diagnostic& d) const {
    // A token missing at the end of a line (it has `line`) says which line.
    const bool at_line_end = d.code == cppi::DiagCode::MissingToken && d.arg("line") != nullptr;
    const auto pattern = catalog_->diagnostic(at_line_end ? "missing-token-at-line-end" : cppi::diag_key(d.code));
    if (!pattern) {
        return cppi::to_debug_string(d);
    }
    std::string text = TemplateFormatter::format(*pattern, [&](std::string_view name) {
        if (name == "feature") {
            return feature(d.arg_text("feature"));
        }
        return d.arg_text(name);
    });
    if (d.arg("suggestion") != nullptr) {
        text += "; " +
                TemplateFormatter::format(ui("did-you-mean"), [&](std::string_view name) { return d.arg_text(name); });
    }
    // Sentence case (ASCII is enough for the first letter of our templates).
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

std::string Messages::severity(cppi::Severity s) const {
    return std::string(catalog_->severity(s));
}

std::string Messages::feature(std::string_view key) const {
    return std::string(catalog_->feature(key).value_or(key));
}

std::string Messages::status(cppi::RunStatus s) const {
    return std::string(catalog_->status(s));
}

std::string Messages::ui(std::string_view key) const {
    return std::string(catalog_->ui(key).value_or(key));
}

}  // namespace cppi_run
