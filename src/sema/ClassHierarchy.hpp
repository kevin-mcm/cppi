#pragma once

/// Questions about inheritance: is B a base of D, where does the B
/// subobject sit inside a D, is the path ambiguous (the diamond problem)?

#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

struct BasePath {
    bool ambiguous = false;               ///< several distinct B subobjects (non-virtual diamond)
    bool via_virtual = false;             ///< the path crosses a virtual base: the offset is dynamic
    std::uint32_t offset = 0;             ///< cells from the D subobject to the B subobject (static paths)
    std::uint32_t virtual_base = 0;       ///< record index of the first virtual base on the path
    std::uint32_t offset_in_virtual = 0;  ///< from that virtual base to B
};

class ClassHierarchy {
public:
    explicit ClassHierarchy(const TypeTable& types) noexcept : types_(types) {}

    /// The path from `derived` to its base `base`, or nullopt if `base` is
    /// not a base (or the same class).
    [[nodiscard]] std::optional<BasePath> find_base(std::uint32_t derived, std::uint32_t base) const;

    /// Every virtual base reachable from `record`, each once, in the order
    /// they are laid out in a complete object.
    [[nodiscard]] std::vector<std::uint32_t> virtual_bases(std::uint32_t record) const;

private:
    void collect(std::uint32_t current, std::uint32_t target, std::uint32_t offset, bool via_virtual,
                 std::uint32_t vbase, std::uint32_t offset_in_vbase, std::vector<BasePath>& found) const;

    const TypeTable& types_;
};

}  // namespace cppi::sema
