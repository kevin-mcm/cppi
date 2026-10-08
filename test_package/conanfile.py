## @file conanfile.py
#  @brief Conan test package: checks that the packaged library can be consumed.
#
#  @author kevin-mcm <kevincardenasmiranda9@gmail.com>
#  @date 2026-10-08

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class CppiTestPackageConan(ConanFile):
    """Builds a tiny consumer against the packaged library, like a game would."""

    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            self.run(os.path.join(self.cpp.build.bindir, "consumer"), env="conanrun")
