#pragma once

/// @file SourceReader.hpp
/// @brief Reads the player's program from a file, or from stdin for "-".
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <optional>
#include <string>

namespace cppi_run {

/// Reads program files.
class SourceReader {
public:
    /// The contents of `file` ("-": stdin), or nullopt if it cannot be opened.
    [[nodiscard]] static std::optional<std::string> read(const std::string& file);
};

}  // namespace cppi_run
