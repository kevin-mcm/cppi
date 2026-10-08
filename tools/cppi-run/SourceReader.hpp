#pragma once

// Reads the player's program from a file, or from stdin for "-".

#include <optional>
#include <string>

namespace cppi_run {

class SourceReader {
public:
    [[nodiscard]] static std::optional<std::string> read(const std::string& file);
};

}  // namespace cppi_run
