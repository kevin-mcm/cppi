#pragma once

// A tiny host for tests: records every call so tests can assert on what the
// player's program actually did, in order.

#include <cppi/Cppi.hpp>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppi_test {

struct RecordingHost {
    std::vector<std::string> calls;
    std::int64_t counter = 0;
    bool fail_next_move = false;

    cppi::TypeId direction{};

    /// Registers: move(Direction), harvest() -> bool, count() -> int,
    /// take_int(int), take_bool(bool), next_direction() -> Direction,
    /// take_double(double), take_long(long), boom() (throws), liar() -> int
    /// (returns the wrong type).
    cppi::HostRegistry make_registry() {
        using cppi::HostCall;
        using cppi::Value;
        cppi::HostRegistry host;
        direction = host.add_enum("Direction", {"North", "East", "South", "West"});

        host.function("move").param(direction).cost(5).bind([this](HostCall& call) {
            if (fail_next_move) {
                fail_next_move = false;
                call.fail(7, "blocked");
                return Value::void_value();
            }
            static const char* const names[] = {"North", "East", "South", "West"};
            calls.push_back(std::string("move(") + names[call.arg(0).as_enum()] + ")");
            return Value::void_value();
        });
        host.function("harvest").returns(cppi::types::Bool).cost(5).bind([this](HostCall&) {
            calls.emplace_back("harvest()");
            return Value::from_bool(true);
        });
        host.function("count").returns(cppi::types::Int).bind([this](HostCall&) {
            calls.emplace_back("count()");
            return Value::from_int(++counter);
        });
        host.function("take_int").param(cppi::types::Int).bind([this](HostCall& call) {
            calls.push_back("take_int(" + std::to_string(call.arg(0).as_int()) + ")");
            return Value::void_value();
        });
        host.function("take_bool").param(cppi::types::Bool).bind([this](HostCall& call) {
            calls.push_back(std::string("take_bool(") + (call.arg(0).as_bool() ? "true" : "false") + ")");
            return Value::void_value();
        });
        host.function("next_direction").returns(direction).bind([this](HostCall&) {
            calls.emplace_back("next_direction()");
            return Value::from_enum(direction, 2);  // South
        });
        host.function("take_double").param(cppi::types::Double).bind([this](HostCall& call) {
            std::ostringstream out;
            out << "take_double(" << call.arg(0).as_double() << ")";
            calls.push_back(out.str());
            return Value::void_value();
        });
        host.function("take_long").param(cppi::types::Long).bind([this](HostCall& call) {
            calls.push_back("take_long(" + std::to_string(call.arg(0).as_long()) + ")");
            return Value::void_value();
        });
        host.function("boom").bind([](HostCall&) -> Value { throw std::runtime_error("kaboom"); });
        host.function("liar").returns(cppi::types::Int).bind([](HostCall&) { return Value::from_bool(true); });
        return host;
    }
};

}  // namespace cppi_test
