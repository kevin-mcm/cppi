#pragma once

/// @file Program.hpp
/// A compiled, immutable program. Compile once, run many times (e.g. every
/// time the player presses "Run" without editing the code).

#include <cppi/HostRegistry.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace cppi {

namespace detail {
struct ProgramData;
}

class Program {
public:
    /// Number of bytecode instructions.
    [[nodiscard]] std::size_t instruction_count() const noexcept;
    /// Maximum operand-stack depth the program can reach.
    [[nodiscard]] std::size_t max_stack_depth() const noexcept;
    /// Human-readable bytecode listing, for debugging and teaching.
    [[nodiscard]] std::string disassemble() const;
    /// The host registry the program was compiled against.
    [[nodiscard]] const HostRegistry& host() const noexcept;

    // Internal: used by the VM.
    [[nodiscard]] const detail::ProgramData& data() const noexcept { return *data_; }

private:
    friend class Interpreter;
    explicit Program(std::shared_ptr<const detail::ProgramData> data) noexcept : data_(std::move(data)) {}

    // Immutable and shared: copying a Program is cheap and thread-safe.
    std::shared_ptr<const detail::ProgramData> data_;
};

}  // namespace cppi
