#pragma once

/// Counts the operations a run consumes against its budget. Charging happens
/// before executing, so an action either happens completely or not at all.

#include <algorithm>
#include <cstdint>

namespace cppi::detail {

class OperationBudget {
public:
    explicit OperationBudget(std::uint64_t limit) noexcept : limit_(limit) {}

    /// Consumes `cost` operations if they fit in what is left.
    [[nodiscard]] bool try_charge(std::uint64_t cost) noexcept {
        if (cost > limit_ - std::min(limit_, used_)) {
            return false;
        }
        used_ += cost;
        return true;
    }

    [[nodiscard]] std::uint64_t used() const noexcept { return used_; }
    [[nodiscard]] std::uint64_t limit() const noexcept { return limit_; }

private:
    std::uint64_t limit_;
    std::uint64_t used_ = 0;
};

}  // namespace cppi::detail
