#include "FarmWorld.hpp"

#include <stdexcept>

namespace cppi_run {

FarmWorld::FarmWorld(int size)
    : size_(size), tiles_(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), Tile::Grass) {
    if (size < 1 || size > 64) {
        throw std::invalid_argument("world size must be between 1 and 64");
    }
}

FarmWorld::Tile FarmWorld::tile(int x, int y) const {
    return tiles_[index(x, y)];
}

bool FarmWorld::can_harvest() const {
    return tile(x_, y_) == Tile::Grass;
}

bool FarmWorld::harvest() {
    if (!can_harvest()) {
        return false;
    }
    tiles_[index(x_, y_)] = Tile::Harvested;
    ++hay_;
    return true;
}

void FarmWorld::move(Direction direction) {
    switch (direction) {
        case North: y_ = (y_ + 1) % size_; break;
        case South: y_ = (y_ + size_ - 1) % size_; break;
        case East: x_ = (x_ + 1) % size_; break;
        case West: x_ = (x_ + size_ - 1) % size_; break;
    }
}

void FarmWorld::render(std::ostream& out) const {
    for (int y = size_ - 1; y >= 0; --y) {
        out << "  ";
        for (int x = 0; x < size_; ++x) {
            char c = tile(x, y) == Tile::Grass ? '"' : '.';
            if (x == x_ && y == y_) {
                c = '@';
            }
            out << c << (x + 1 < size_ ? " " : "");
        }
        out << '\n';
    }
}

}  // namespace cppi_run
