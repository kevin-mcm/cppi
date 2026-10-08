# Benchmarks of the interpreter (Google Benchmark).
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

find_package(benchmark REQUIRED CONFIG)

add_executable(cppi_bench InterpreterBench.cpp LevelBench.cpp)
target_link_libraries(cppi_bench PRIVATE cppi::cppi benchmark::benchmark_main)
cppi_set_warnings(cppi_bench)
