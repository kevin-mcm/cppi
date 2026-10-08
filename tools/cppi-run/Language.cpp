#include "Language.hpp"

#include <cstdlib>

namespace cppi_run {

std::optional<Language> parse_language(std::string_view code) noexcept {
    if (code.starts_with("es")) return Language::Spanish;
    if (code.starts_with("en")) return Language::English;
    return std::nullopt;
}

Language detect_language() noexcept {
    for (const char* var : {"LC_ALL", "LANG"}) {
        // Read once at startup, before any thread exists.
        const char* value = std::getenv(var);  // NOLINT(concurrency-mt-unsafe)
        if (value != nullptr && *value != '\0') {
            if (auto lang = parse_language(value)) {
                return *lang;
            }
        }
    }
    return Language::English;
}

}  // namespace cppi_run
