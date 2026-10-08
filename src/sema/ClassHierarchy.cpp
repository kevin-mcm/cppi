#include "sema/ClassHierarchy.hpp"

#include <algorithm>

namespace cppi::sema {

void ClassHierarchy::collect(std::uint32_t current, std::uint32_t target, std::uint32_t offset, bool via_virtual,
                             std::uint32_t vbase, std::uint32_t offset_in_vbase, std::vector<BasePath>& found) const {
    for (const BaseInfo& base : types_.record_at(current).bases) {
        bool next_virtual = via_virtual;
        std::uint32_t next_vbase = vbase;
        std::uint32_t next_offset = offset;
        std::uint32_t next_in_vbase = offset_in_vbase;
        if (base.is_virtual && !via_virtual) {
            next_virtual = true;
            next_vbase = base.record;
            next_in_vbase = 0;
        } else if (via_virtual) {
            next_in_vbase += base.offset;
        } else {
            next_offset += base.offset;
        }
        if (base.record == target) {
            found.push_back(BasePath{false, next_virtual, next_offset, next_vbase, next_in_vbase});
        }
        collect(base.record, target, next_offset, next_virtual, next_vbase, next_in_vbase, found);
    }
}

std::optional<BasePath> ClassHierarchy::find_base(std::uint32_t derived, std::uint32_t base) const {
    std::vector<BasePath> found;
    collect(derived, base, 0, false, 0, 0, found);
    if (found.empty()) {
        return std::nullopt;
    }
    BasePath first = found.front();
    for (const BasePath& other : found) {
        // Paths that reach B through the same virtual base share one subobject.
        const bool same = other.via_virtual == first.via_virtual &&
                          (first.via_virtual ? other.virtual_base == first.virtual_base &&
                                                   other.offset_in_virtual == first.offset_in_virtual
                                             : other.offset == first.offset);
        if (!same) {
            first.ambiguous = true;
        }
    }
    return first;
}

std::vector<std::uint32_t> ClassHierarchy::virtual_bases(std::uint32_t record) const {
    std::vector<std::uint32_t> out;
    // Depth-first, left to right: bases of a virtual base come before it.
    auto visit = [&](auto&& self, std::uint32_t current) -> void {
        for (const BaseInfo& base : types_.record_at(current).bases) {
            self(self, base.record);
            if (base.is_virtual && std::find(out.begin(), out.end(), base.record) == out.end()) {
                out.push_back(base.record);
            }
        }
    };
    visit(visit, record);
    return out;
}

}  // namespace cppi::sema
