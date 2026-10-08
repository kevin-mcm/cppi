#include "sema/NameSuggester.hpp"

#include <algorithm>
#include <utility>

namespace cppi::sema {

std::optional<std::string> NameSuggester::closest(std::string_view name, const std::vector<std::string>& candidates) {
    const std::size_t limit = name.size() <= 3 ? 1 : 2;
    std::optional<std::string> best;
    std::size_t best_distance = limit + 1;
    for (const auto& candidate : candidates) {
        if (candidate == name) {
            continue;
        }
        const std::size_t d = edit_distance(name, candidate);
        if (d < best_distance) {
            best_distance = d;
            best = candidate;
        }
    }
    return best;
}

std::size_t NameSuggester::edit_distance(std::string_view a, std::string_view b) {
    std::vector<std::size_t> prev(b.size() + 1);
    std::vector<std::size_t> curr(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) {
        prev[j] = j;
    }
    for (std::size_t i = 1; i <= a.size(); ++i) {
        curr[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t substitution = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, substitution});
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}

}  // namespace cppi::sema
