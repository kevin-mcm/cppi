#pragma once

// Adapter between the farm and the interpreter: exposes FarmWorld to player
// code as host functions (move, harvest, ...). It is the reference example
// of how a game registers its world with cppi.

#include "FarmWorld.hpp"

#include <cppi/HostRegistry.hpp>

namespace cppi_run {

class FarmHostBindings {
public:
    /// Registers the player-facing API on `host`. The registry keeps
    /// references to `world`: it must outlive the interpreter that uses it.
    static void register_api(FarmWorld& world, cppi::HostRegistry& host);
};

}  // namespace cppi_run
