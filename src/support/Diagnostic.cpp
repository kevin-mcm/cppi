#include <cppi/Diagnostic.hpp>

#include <string>

namespace cppi {

const DiagValue* Diagnostic::arg(std::string_view name) const noexcept {
    for (const auto& a : args) {
        if (a.name == name) {
            return &a.value;
        }
    }
    return nullptr;
}

std::string Diagnostic::arg_text(std::string_view name) const {
    const DiagValue* value = arg(name);
    if (value == nullptr) {
        return {};
    }
    if (const auto* i = std::get_if<std::int64_t>(value)) {
        return std::to_string(*i);
    }
    return std::get<std::string>(*value);
}

std::string to_debug_string(const Diagnostic& d) {
    std::string out = std::to_string(d.range.begin.line) + ":" + std::to_string(d.range.begin.column) + ": ";
    switch (d.severity) {
        case Severity::Error: out += "error"; break;
        case Severity::Warning: out += "warning"; break;
        case Severity::Note: out += "note"; break;
    }
    out += "[" + diag_id(d.code) + " " + std::string(diag_key(d.code)) + "]";
    for (const auto& a : d.args) {
        out += " " + a.name + "=" + d.arg_text(a.name);
    }
    return out;
}

}  // namespace cppi
