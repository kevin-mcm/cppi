#include "CommandLineParser.hpp"

#include "ExitCode.hpp"

#include <cppi/Feature.hpp>
#include <cppi/Version.hpp>

#include <charconv>
#include <string>
#include <utility>

namespace cppi_run {

namespace {

template <typename T>
std::optional<T> parse_number(std::string_view text) {
    T value{};
    const auto* end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

const std::string_view CommandLineParser::kUsage = R"(usage: cppi-run [options] <file | ->

Runs a C++ program on a small simulated farm using the cppi interpreter.

options:
  --std <c++98|c++11|...>   language standard (default: c++98)
  --lock <a,b,...>          lock features, e.g. --lock loops,variables
  --budget <n>              operation budget (default: 10000)
  --size <n>                farm size, 1-64 (default: 5)
  --lang <es|en>            message language (default: from LANG)
  --trace                   print every host call
  --dump-bytecode           print the compiled bytecode
  --quiet                   do not draw the farm
  --version                 print the version
  -h, --help                show this help
)";

CommandLineParser::Outcome CommandLineParser::fail(std::string_view message) const {
    err_ << "cppi-run: " << message << "\n\n" << kUsage;
    return {std::nullopt, exit_code::kUsage};
}

CommandLineParser::Outcome CommandLineParser::parse(std::span<const std::string_view> args) const {
    CommandLineOptions options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= args.size()) {
                return std::nullopt;
            }
            return args[++i];
        };

        if (arg == "-h" || arg == "--help") {
            out_ << kUsage;
            return {std::nullopt, exit_code::kOk};
        }
        if (arg == "--version") {
            out_ << "cppi-run " << cppi::kVersionString << '\n';
            return {std::nullopt, exit_code::kOk};
        }
        if (arg == "--trace") {
            options.trace = true;
        } else if (arg == "--dump-bytecode") {
            options.dump_bytecode = true;
        } else if (arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "--std") {
            auto v = value();
            auto standard = v ? cppi::parse_standard(*v) : std::nullopt;
            if (!standard) return fail("--std expects c++98, c++11, c++14, c++17, c++20, c++23 or c++26");
            options.standard = *standard;
        } else if (arg == "--lock") {
            auto v = value();
            if (!v) return fail("--lock expects a list of features");
            std::string_view list = *v;
            while (!list.empty()) {
                const auto comma = list.find(',');
                const auto key = list.substr(0, comma);
                auto feature = cppi::parse_feature(key);
                if (!feature) return fail("unknown feature '" + std::string(key) + "'");
                options.locked.add(*feature);
                list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
            }
        } else if (arg == "--budget") {
            auto v = value();
            auto n = v ? parse_number<std::uint64_t>(*v) : std::nullopt;
            if (!n) return fail("--budget expects a positive number");
            options.budget = *n;
        } else if (arg == "--size") {
            auto v = value();
            auto n = v ? parse_number<int>(*v) : std::nullopt;
            if (!n || *n < 1 || *n > 64) return fail("--size expects a number between 1 and 64");
            options.size = *n;
        } else if (arg == "--lang") {
            auto v = value();
            auto lang = v ? parse_language(*v) : std::nullopt;
            if (!lang) return fail("--lang expects 'es' or 'en'");
            options.language = *lang;
        } else if (arg.starts_with("-") && arg != "-") {
            return fail("unknown option '" + std::string(arg) + "'");
        } else if (options.file.empty()) {
            options.file = std::string(arg);
        } else {
            return fail("only one input file is allowed");
        }
    }
    if (options.file.empty()) {
        return fail("missing input file");
    }
    return {std::move(options), exit_code::kOk};
}

}  // namespace cppi_run
