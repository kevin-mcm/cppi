#pragma once

/// @file CommandLineOptions.hpp
/// @brief Settings cppi-run takes from the command line.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "Language.hpp"

#include <cppi/FeatureSet.hpp>
#include <cppi/Standard.hpp>

#include <cstdint>
#include <string>

namespace cppi_run {

/// Parsed command line (see CommandLineParser::kUsage).
struct CommandLineOptions {
    /// Program file, or "-" for stdin.
    std::string file;
    /// `--std`: language standard.
    cppi::Standard standard = cppi::Standard::Cpp98;
    /// `--lock`: locked features.
    cppi::FeatureSet locked;
    /// `--budget`: operation budget.
    std::uint64_t budget = 10'000;
    /// `--size`: farm size (1-64).
    int size = 5;
    /// `--lang`: message language.
    Language language = detect_language();
    /// `--trace`: print every host call.
    bool trace = false;
    /// `--dump-bytecode`: print the compiled bytecode.
    bool dump_bytecode = false;
    /// `--quiet`: do not draw the farm.
    bool quiet = false;
};

}  // namespace cppi_run
