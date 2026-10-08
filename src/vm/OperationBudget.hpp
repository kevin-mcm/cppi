#pragma once

/// @file OperationBudget.hpp
/// @brief Counts the operations a run consumes against its budget.
///
/// Charging happens before executing, so an action either happens completely or
/// not at all.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <algorithm>
#include <cstdint>

namespace cppi::detail {

/// The operations a run may still spend.
class OperationBudget {
public:
    /// @param limit Operations the run may consume in total.
    explicit OperationBudget(std::uint64_t limit) noexcept : limit_(limit) {}

    /// Consumes `cost` operations if they fit in what is left.
    [[nodiscard]] bool try_charge(std::uint64_t cost) noexcept {
        if (cost > limit_ - std::min(limit_, used_)) {
            return false;
        }
        used_ += cost;
        return true;
    }

    /// Operations consumed so far.
    [[nodiscard]] std::uint64_t used() const noexcept { return used_; }
    /// The total budget.
    [[nodiscard]] std::uint64_t limit() const noexcept { return limit_; }

private:
    /// See limit().
    std::uint64_t limit_;
    /// See used().
    std::uint64_t used_ = 0;
};

}  // namespace cppi::detail
