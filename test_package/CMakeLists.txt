# Conan test package: builds a minimal consumer against the installed cppi.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

cmake_minimum_required(VERSION 3.23)
project(cppi_test_package LANGUAGES CXX)

find_package(cppi REQUIRED CONFIG)

add_executable(consumer Consumer.cpp)
target_link_libraries(consumer PRIVATE cppi::cppi)
target_compile_features(consumer PRIVATE cxx_std_20)
