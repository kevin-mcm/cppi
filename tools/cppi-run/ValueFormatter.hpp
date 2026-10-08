#pragma once

// Renders runtime values the way the player wrote them: 3, true, East.

#include <cppi/HostRegistry.hpp>
#include <cppi/Value.hpp>

#include <string>

namespace cppi_run {

class ValueFormatter {
public:
    [[nodiscard]] static std::string format(const cppi::HostRegistry& host, const cppi::Value& value);
};

}  // namespace cppi_run
