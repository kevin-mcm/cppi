/// @file SourceReader.cpp
/// @brief Implementation of SourceReader.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "SourceReader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace cppi_run {

std::optional<std::string> SourceReader::read(const std::string& file) {
    std::ostringstream buffer;
    if (file == "-") {
        buffer << std::cin.rdbuf();
        return buffer.str();
    }
    const std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace cppi_run
