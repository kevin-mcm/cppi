#include "DiagnosticPrinter.hpp"

namespace cppi_run {

void DiagnosticPrinter::print(const cppi::Diagnostic& d) const {
    const auto& begin = d.range.begin;
    out_ << (file_ == "-" ? "<stdin>" : file_) << ':' << begin.line << ':' << begin.column << ": "
         << messages_.severity(d.severity) << ' ' << cppi::diag_id(d.code) << ": " << messages_.diagnostic(d) << '\n';

    const std::string_view text = line_of(begin.line);
    if (text.empty() || begin.column == 0) {
        return;
    }
    const std::string number = std::to_string(begin.line);
    out_ << "  " << number << " | " << text << '\n';
    out_ << "  " << std::string(number.size(), ' ') << " | " << std::string(begin.column - 1, ' ') << '^';
    if (d.range.end.line == begin.line && d.range.end.column > begin.column + 1) {
        out_ << std::string(d.range.end.column - begin.column - 1, '~');
    }
    out_ << '\n';
}

std::string_view DiagnosticPrinter::line_of(std::uint32_t line) const {
    std::uint32_t current = 1;
    std::size_t start = 0;
    while (current < line) {
        const auto newline = source_.find('\n', start);
        if (newline == std::string_view::npos) {
            return {};
        }
        start = newline + 1;
        ++current;
    }
    auto end = source_.find('\n', start);
    if (end == std::string_view::npos) {
        end = source_.size();
    }
    auto text = source_.substr(start, end - start);
    if (!text.empty() && text.back() == '\r') {
        text.remove_suffix(1);
    }
    return text;
}

}  // namespace cppi_run
