#pragma once

/// @file FarmWorld.hpp
/// @brief A tiny farm used by cppi-run: an N x N grid of grass that wraps
/// around at the edges, and an automaton that walks over it.
///
/// Pure game logic: it knows nothing about the interpreter (see
/// FarmHostBindings for that).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace cppi_run {

/// The farm grid and the automaton's position. (0, 0) is the south-west
/// corner; north increases y.
class FarmWorld {
public:
    /// State of one tile.
    enum class Tile : std::uint8_t { Grass, Harvested };
    /// Directions the automaton moves in (an unscoped enum, as player code sees it).
    enum Direction : std::uint8_t { North, East, South, West };

    /// A `size` x `size` farm of grass, the automaton at (0, 0).
    explicit FarmWorld(int size);

    /// Harvests the current tile; false (nothing happens) if it is not grass.
    bool harvest();
    /// True if the current tile is grass.
    [[nodiscard]] bool can_harvest() const;
    /// Moves one tile, wrapping around at the edges.
    void move(Direction direction);

    /// Automaton column.
    [[nodiscard]] int x() const noexcept { return x_; }
    /// Automaton row.
    [[nodiscard]] int y() const noexcept { return y_; }
    /// Side length of the farm.
    [[nodiscard]] int size() const noexcept { return size_; }
    /// Tiles harvested so far.
    [[nodiscard]] std::int64_t hay() const noexcept { return hay_; }
    /// The tile at (x, y).
    [[nodiscard]] Tile tile(int x, int y) const;

    /// Draws the grid, north at the top: `"` grass, `.` harvested, `@` automaton.
    void render(std::ostream& out) const;

private:
    /// Index of (x, y) in `tiles_`.
    [[nodiscard]] std::size_t index(int x, int y) const noexcept {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(size_)) + static_cast<std::size_t>(x);
    }

    /// Side length.
    int size_;
    /// Automaton column.
    int x_ = 0;
    /// Automaton row.
    int y_ = 0;
    /// Tiles harvested.
    std::int64_t hay_ = 0;
    /// Tiles, row by row from the south.
    std::vector<Tile> tiles_;
};

}  // namespace cppi_run
