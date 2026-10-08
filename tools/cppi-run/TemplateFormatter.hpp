#pragma once

/// @file TemplateFormatter.hpp
/// @brief Fills "{name}" placeholders in a message template.
///
/// A part in square brackets is optional: "cannot modify[ '{name}']" drops
/// "[...]" when `name` has no value.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <string>
#include <string_view>

namespace cppi_run {

/// Fills message templates. `{name}` is replaced by its value; a `[...]`
/// part is kept only if every placeholder in it has a non-empty value.
class TemplateFormatter {
public:
    /// Replaces every {name} in `text` with `lookup(name)`.
    template <typename Lookup>
    [[nodiscard]] static std::string format(std::string_view text, const Lookup& lookup) {
        std::string out;
        out.reserve(text.size() + 16);
        std::size_t i = 0;
        while (i < text.size()) {
            const char c = text[i];
            if (c == '[') {
                const auto close = text.find(']', i);
                if (close != std::string_view::npos) {
                    bool complete = true;
                    const std::string part = substitute(text.substr(i + 1, close - i - 1), lookup, complete);
                    if (complete) {
                        out += part;
                    }
                    i = close + 1;
                    continue;
                }
            }
            const auto next = text.find_first_of("[{", i + (c == '{' ? 0 : 1));
            const auto end = next == std::string_view::npos ? text.size() : next;
            if (c == '{') {
                bool complete = true;
                const auto close = text.find('}', i);
                if (close == std::string_view::npos) {
                    out.append(text.substr(i));
                    break;
                }
                out += substitute(text.substr(i, close - i + 1), lookup, complete);
                i = close + 1;
                continue;
            }
            out.append(text.substr(i, end - i));
            i = end;
        }
        return out;
    }

private:
    template <typename Lookup>
    /// Replaces the placeholders in `text`; clears `complete` if one is empty.
    static std::string substitute(std::string_view text, const Lookup& lookup, bool& complete) {
        std::string out;
        std::size_t i = 0;
        while (i < text.size()) {
            const auto open = text.find('{', i);
            if (open == std::string_view::npos) {
                out.append(text.substr(i));
                break;
            }
            const auto close = text.find('}', open);
            if (close == std::string_view::npos) {
                out.append(text.substr(i));
                break;
            }
            out.append(text.substr(i, open - i));
            const std::string value = lookup(text.substr(open + 1, close - open - 1));
            complete = complete && !value.empty();
            out.append(value);
            i = close + 1;
        }
        return out;
    }
};

}  // namespace cppi_run
