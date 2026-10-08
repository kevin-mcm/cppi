#pragma once

/// "Did you mean ...?": finds the candidate closest to a misspelled name.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::sema {

class NameSuggester {
public:
    /// Closest candidate within a small edit distance, or nullopt.
    [[nodiscard]] static std::optional<std::string> closest(std::string_view name,
                                                            const std::vector<std::string>& candidates);

private:
    [[nodiscard]] static std::size_t edit_distance(std::string_view a, std::string_view b);
};

}  // namespace cppi::sema
