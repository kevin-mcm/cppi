/// @file FarmHostBindings.cpp
/// @brief Implementation of FarmHostBindings.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "FarmHostBindings.hpp"

#include <cppi/HostCall.hpp>
#include <cppi/Value.hpp>

namespace cppi_run {

void FarmHostBindings::register_api(FarmWorld& world, cppi::HostRegistry& host) {
    using cppi::HostCall;
    using cppi::Value;
    namespace types = cppi::types;

    const cppi::TypeId direction = host.add_enum("Direction", {"North", "East", "South", "West"});

    host.function("move").param(direction).cost(5).bind([&world](HostCall& call) {
        world.move(static_cast<FarmWorld::Direction>(call.arg(0).as_enum()));
        return Value::void_value();
    });
    host.function("harvest").returns(types::Bool).cost(5).bind([&world](HostCall&) {
        return Value::from_bool(world.harvest());
    });
    host.function("can_harvest").returns(types::Bool).cost(1).bind([&world](HostCall&) {
        return Value::from_bool(world.can_harvest());
    });
    host.function("get_pos_x").returns(types::Int).cost(1).bind([&world](HostCall&) {
        return Value::from_int(world.x());
    });
    host.function("get_pos_y").returns(types::Int).cost(1).bind([&world](HostCall&) {
        return Value::from_int(world.y());
    });
}

}  // namespace cppi_run
