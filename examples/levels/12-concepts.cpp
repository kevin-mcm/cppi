// Concepts say what a template needs from its types.
#include <concepts>

template <class T>
concept Steps = std::integral<T> && !std::same_as<T, bool>;

template <Steps T>
void walk(T steps) {
    for (T i = 0; i < steps; ++i) {
        move(East);
    }
}

walk(2);
harvest();
std::cout << "integral<double>: " << std::integral<double> << std::endl;
