// Exceptions: a problem far away is handled where it makes sense.
#include <stdexcept>

void harvest_row(int length) {
    if (length < 0) {
        throw std::invalid_argument("a row cannot be negative");
    }
    for (int i = 0; i < length; ++i) {
        harvest();
        move(East);
    }
}

try {
    harvest_row(2);
    harvest_row(-1);
} catch (const std::exception& e) {
    std::cout << "problem: " << e.what() << std::endl;
    move(North);
}
