#pragma once

// A tiny farm used by cppi-run: an N x N grid of grass that wraps around at
// the edges, and an automaton that walks over it. Pure game logic: it knows
// nothing about the interpreter (see FarmHostBindings for that).

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace cppi_run {

class FarmWorld {
public:
    enum class Tile : std::uint8_t { Grass, Harvested };
    enum Direction : std::uint8_t { North, East, South, West };

    explicit FarmWorld(int size);

    bool harvest();
    [[nodiscard]] bool can_harvest() const;
    void move(Direction direction);

    [[nodiscard]] int x() const noexcept { return x_; }
    [[nodiscard]] int y() const noexcept { return y_; }
    [[nodiscard]] int size() const noexcept { return size_; }
    [[nodiscard]] std::int64_t hay() const noexcept { return hay_; }
    [[nodiscard]] Tile tile(int x, int y) const;

    /// Draws the grid, north at the top: `"` grass, `.` harvested, `@` automaton.
    void render(std::ostream& out) const;

private:
    [[nodiscard]] std::size_t index(int x, int y) const noexcept {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(size_)) + static_cast<std::size_t>(x);
    }

    int size_;
    int x_ = 0;
    int y_ = 0;
    std::int64_t hay_ = 0;
    std::vector<Tile> tiles_;
};

}  // namespace cppi_run
