/// @file LevelBench.cpp
/// @brief Operations and speed of whole cppi-farm level solutions: how many
/// bytecode instructions a realistic program executes, and how fast.
///
/// The counters (instructions in the program, operations charged with the
/// default CostModel, instructions executed including the library) are what
/// code generation changes should bring down.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Cppi.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>

namespace {

/// cppi-farm, core/tests/solutions/32-shortest-path.cpp
constexpr const char* kShortestPath = R"cpp(
    const int kWidth = 7;
    const int kHeight = 5;
    // The farmer's map, north row first: # is a rock, G the grass to harvest.
    std::string field[kHeight] = {
        "___#__G", "_#_#_#_", "_#___#_", "_####__", "_______",
    };

    // What the map shows at (x, y); # outside it.
    char tile(int x, int y) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
            return '#';
        }
        return field[kHeight - 1 - y][x];
    }

    Direction opposite(Direction d) {
        if (d == North) {
            return South;
        }
        if (d == East) {
            return West;
        }
        if (d == South) {
            return North;
        }
        return East;
    }
    int dx(Direction d) {
        return d == East ? 1 : (d == West ? -1 : 0);
    }
    int dy(Direction d) {
        return d == North ? 1 : (d == South ? -1 : 0);
    }

    // Breadth-first search on the map: tiles leave the queue in order of
    // distance, so the first time the search reaches a tile, it is by the
    // shortest way. Tile (x, y) is number y * kWidth + x.
    std::vector<int> came_from(kWidth* kHeight, -1);
    std::vector<Direction> step_in = std::vector<Direction>(kWidth * kHeight, North);
    std::vector<int> queue;

    int start = get_pos_y() * kWidth + get_pos_x();
    came_from[start] = start;
    queue.push_back(start);
    int target = -1;
    Direction ways[4] = {North, East, South, West};
    for (int head = 0; head < queue.size() && target == -1; head++) {
        int x = queue[head] % kWidth;
        int y = queue[head] / kWidth;
        if (tile(x, y) == 'G') {
            target = queue[head];
        }
        for (Direction way : ways) {
            int next = (y + dy(way)) * kWidth + x + dx(way);
            if (tile(x + dx(way), y + dy(way)) != '#' && came_from[next] == -1) {
                came_from[next] = queue[head];
                step_in[next] = way;
                queue.push_back(next);
            }
        }
    }

    // Follow came_from back to the start, then walk the steps forwards.
    std::vector<Direction> path;
    for (int at = target; at != start; at = came_from[at]) {
        path.push_back(step_in[at]);
    }
    for (int i = path.size() - 1; i >= 0; i--) {
        move(path[i]);
    }
    harvest();
)cpp";

/// cppi-farm, core/tests/solutions/44-travelling-salesman.cpp
constexpr const char* kTravellingSalesman = R"cpp(
    const int kWidth = 7;
    const int kHeight = 5;
    // North row first: # is a rock, G grass to harvest.
    std::string field[kHeight] = {
        "__G____", "______#", "G____G_", "_##__G_", "__G#___",
    };

    char tile(int x, int y) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
            return '#';
        }
        return field[kHeight - 1 - y][x];
    }

    Direction opposite(Direction d) {
        if (d == North) {
            return South;
        }
        if (d == East) {
            return West;
        }
        if (d == South) {
            return North;
        }
        return East;
    }
    int dx(Direction d) {
        return d == East ? 1 : (d == West ? -1 : 0);
    }
    int dy(Direction d) {
        return d == North ? 1 : (d == South ? -1 : 0);
    }

    // Breadth-first search from tile start: steps[t] is how many moves reach tile
    // t (number y * kWidth + x), -1 if none; came_from and step_in rebuild the way.
    std::vector<int> steps;
    std::vector<int> came_from;
    std::vector<Direction> step_in;

    void search_from(int start) {
        steps = std::vector<int>(kWidth * kHeight, -1);
        came_from = std::vector<int>(kWidth * kHeight, -1);
        step_in = std::vector<Direction>(kWidth * kHeight, North);
        std::vector<int> queue;
        steps[start] = 0;
        queue.push_back(start);
        Direction ways[4] = {North, East, South, West};
        for (int head = 0; head < queue.size(); head++) {
            int x = queue[head] % kWidth;
            int y = queue[head] / kWidth;
            for (Direction way : ways) {
                int next = (y + dy(way)) * kWidth + x + dx(way);
                if (tile(x + dx(way), y + dy(way)) != '#' && steps[next] == -1) {
                    steps[next] = steps[queue[head]] + 1;
                    came_from[next] = queue[head];
                    step_in[next] = way;
                    queue.push_back(next);
                }
            }
        }
    }

    // Walks the way search_from() found to tile target.
    void walk_to(int target) {
        std::vector<Direction> path;
        for (int at = target; steps[at] > 0; at = came_from[at]) {
            path.push_back(step_in[at]);
        }
        for (int i = path.size() - 1; i >= 0; i--) {
            move(path[i]);
        }
    }

    // The grass, in the order the map lists it.
    std::vector<int> grass;
    for (int y = kHeight - 1; y >= 0; y--) {
        for (int x = 0; x < kWidth; x++) {
            if (tile(x, y) == 'G') {
                grass.push_back(y * kWidth + x);
            }
        }
    }

    // Stop 0 is the start, stops 1 to n the grass. moves_between[i * stops + j]
    // is the length of the shortest way from stop i to stop j.
    std::vector<int> where;
    where.push_back(get_pos_y() * kWidth + get_pos_x());
    for (int g : grass) {
        where.push_back(g);
    }
    int stops = where.size();
    std::vector<int> moves_between = std::vector<int>(stops * stops, 0);
    for (int i = 0; i < stops; i++) {
        search_from(where[i]);
        for (int j = 0; j < stops; j++) {
            moves_between[i * stops + j] = steps[where[j]];
        }
    }

    // Backtracking over the orders: from stop `at`, having walked `walked`
    // moves, try each stop not visited yet, and undo the choice afterwards.
    // Orders already longer than the best one found are not followed.
    std::vector<bool> visited = std::vector<bool>(stops, false);
    std::vector<int> order;
    std::vector<int> best_order;
    int best_moves = 1000000;

    void plan(int at, int walked) {
        if (walked >= best_moves) {
            return;
        }
        if (order.size() == stops - 1) {
            best_moves = walked;
            best_order = order;
            return;
        }
        for (int next = 1; next < stops; next++) {
            if (!visited[next]) {
                visited[next] = true;
                order.push_back(next);
                plan(next, walked + moves_between[at * stops + next]);
                order.pop_back();
                visited[next] = false;
            }
        }
    }

    plan(0, 0);
    for (int stop : best_order) {
        search_from(get_pos_y() * kWidth + get_pos_x());
        walk_to(where[stop]);
        harvest();
    }
)cpp";

/// cppi-farm, core/tests/solutions/40-memoization.cpp
constexpr const char* kMemoization = R"cpp(
    const int kWidth = 9;
    const int kHeight = 7;
    // North row first: G is grass, _ is bare soil.
    std::string field[kHeight] = {
        "_G__G__G_", "GGG____G_", "G___G_GGG", "G_G__GG__", "_G__GGGGG", "_G_GG_GG_", "G______G_",
    };

    int grass(int x, int y) {
        return field[kHeight - 1 - y][x] == 'G' ? 1 : 0;
    }

    // The most grass on a way from the start to (x, y), going only east and
    // north: the tile's own grass plus the best of the tiles west and south.
    // memo keeps each answer, -1 until it is known, so each is worked out once.
    std::vector<int> memo = std::vector<int>(kWidth * kHeight, -1);

    int best(int x, int y) {
        if (memo[y * kWidth + x] != -1) {
            return memo[y * kWidth + x];
        }
        int before = 0;
        if (x > 0) {
            before = best(x - 1, y);
        }
        if (y > 0) {
            before = std::max(before, best(x, y - 1));
        }
        memo[y * kWidth + x] = before + grass(x, y);
        return memo[y * kWidth + x];
    }

    // Walk the best way back from the north-east corner, then forwards.
    std::vector<Direction> path;
    int x = kWidth - 1;
    int y = kHeight - 1;
    while (x > 0 || y > 0) {
        if (y == 0 || (x > 0 && best(x - 1, y) >= best(x, y - 1))) {
            path.push_back(East);
            x--;
        } else {
            path.push_back(North);
            y--;
        }
    }
    harvest();
    for (int i = path.size() - 1; i >= 0; i--) {
        move(path[i]);
        harvest();
    }
)cpp";

/// The farm's functions, free and doing nothing: only the program is measured.
cppi::HostRegistry make_farm() {
    cppi::HostRegistry host;
    const auto direction = host.add_enum("Direction", {"North", "East", "South", "West"});
    host.function("move").param(direction).bind([](cppi::HostCall&) { return cppi::Value::void_value(); });
    host.function("harvest").returns(cppi::types::Bool).bind([](cppi::HostCall&) {
        return cppi::Value::from_bool(true);
    });
    host.function("can_move").param(direction).returns(cppi::types::Bool).bind([](cppi::HostCall&) {
        return cppi::Value::from_bool(true);
    });
    host.function("get_pos_x").returns(cppi::types::Int).bind([](cppi::HostCall&) { return cppi::Value::from_int(0); });
    host.function("get_pos_y").returns(cppi::types::Int).bind([](cppi::HostCall&) { return cppi::Value::from_int(0); });
    return host;
}

void run_level(benchmark::State& state, const char* source) {
    cppi::Options options;
    options.standard = cppi::Standard::Cpp20;
    const cppi::Interpreter interpreter(make_farm(), options);
    auto compiled = interpreter.compile(source);
    if (!compiled.program) {
        state.SkipWithError("the level does not compile");
        return;
    }
    const cppi::Program& program = *compiled.program;
    cppi::RunOptions run_options;
    run_options.budget = UINT64_MAX;

    struct Counter final : cppi::ExecutionObserver {
        std::uint64_t steps = 0;
        void on_step(cppi::SourceRange, std::uint64_t) override { ++steps; }
    };
    Counter counter;
    cppi::RunOptions counted = run_options;
    counted.observer = &counter;
    const cppi::RunResult once = interpreter.run(program, counted);
    if (!once.ok()) {
        state.SkipWithError("the level does not run to completion");
        return;
    }

    for (auto _ : state) {
        auto result = interpreter.run(program, run_options);
        benchmark::DoNotOptimize(result);
    }
    state.counters["instructions"] = static_cast<double>(program.instruction_count());
    state.counters["operations"] = static_cast<double>(once.operations);
    state.counters["executed"] = static_cast<double>(counter.steps);
}

void BM_Level_ShortestPath(benchmark::State& state) {
    run_level(state, kShortestPath);
}
BENCHMARK(BM_Level_ShortestPath)->Unit(benchmark::kMicrosecond);

void BM_Level_TravellingSalesman(benchmark::State& state) {
    run_level(state, kTravellingSalesman);
}
BENCHMARK(BM_Level_TravellingSalesman)->Unit(benchmark::kMicrosecond);

void BM_Level_Memoization(benchmark::State& state) {
    run_level(state, kMemoization);
}
BENCHMARK(BM_Level_Memoization)->Unit(benchmark::kMicrosecond);

}  // namespace
