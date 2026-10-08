#include <cppi/Standard.hpp>

#include <cctype>
#include <string>

namespace cppi {

std::string_view to_string(Standard standard) noexcept {
    switch (standard) {
        case Standard::Cpp98: return "C++98";
        case Standard::Cpp11: return "C++11";
        case Standard::Cpp14: return "C++14";
        case Standard::Cpp17: return "C++17";
        case Standard::Cpp20: return "C++20";
        case Standard::Cpp23: return "C++23";
        case Standard::Cpp26: return "C++26";
    }
    return "C++?";
}

std::optional<Standard> parse_standard(std::string_view text) noexcept {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char c : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    std::string_view digits = lowered;
    for (const std::string_view prefix : {"c++", "cpp"}) {
        if (digits.starts_with(prefix)) {
            digits.remove_prefix(prefix.size());
            break;
        }
    }
    if (digits == "98" || digits == "03") return Standard::Cpp98;
    if (digits == "11") return Standard::Cpp11;
    if (digits == "14") return Standard::Cpp14;
    if (digits == "17") return Standard::Cpp17;
    if (digits == "20") return Standard::Cpp20;
    if (digits == "23") return Standard::Cpp23;
    if (digits == "26") return Standard::Cpp26;
    return std::nullopt;
}

}  // namespace cppi
