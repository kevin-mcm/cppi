#pragma once

// Settings cppi-run takes from the command line.

#include "Language.hpp"

#include <cppi/FeatureSet.hpp>
#include <cppi/Standard.hpp>

#include <cstdint>
#include <string>

namespace cppi_run {

struct CommandLineOptions {
    std::string file;
    cppi::Standard standard = cppi::Standard::Cpp98;
    cppi::FeatureSet locked;
    std::uint64_t budget = 10'000;
    int size = 5;
    Language language = detect_language();
    bool trace = false;
    bool dump_bytecode = false;
    bool quiet = false;
};

}  // namespace cppi_run
