## @file conanfile.py
#  @brief Conan recipe of the cppi library.
#
#  @author kevin-mcm <kevincardenasmiranda9@gmail.com>
#  @date 2026-10-08

import os

from conan import ConanFile
from conan.tools.build import can_run, check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load

required_conan_version = ">=2.4"


class CppiConan(ConanFile):
    name = "cppi"
    description = (
        "Embeddable interpreter for a progressively unlockable subset of C++, "
        "built for educational programming games."
    )
    license = "MIT"
    homepage = "https://github.com/kevin-mcm/cppi"
    url = "https://github.com/kevin-mcm/cppi"
    topics = ("interpreter", "c++", "education", "game", "vm")
    package_type = "library"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        # The cppi-run console tool. Disable it for mobile targets.
        "with_tools": [True, False],
        # Google Benchmark suite (developer-only, never affects the package).
        "with_benchmarks": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": True,
        "with_benchmarks": False,
    }
    implements = ["auto_shared_fpic"]

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "tools/*",
        "tests/*",
        "bench/*",
        "fuzz/*",
        "examples/*",
        "LICENSE",
    )

    def set_version(self):
        # Single source of truth for the version: the CMake project() call.
        content = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        for line in content.splitlines():
            line = line.strip()
            if line.startswith("VERSION "):
                self.version = line.split()[1]
                return
        raise RuntimeError("Could not read VERSION from CMakeLists.txt")

    def validate(self):
        check_min_cppstd(self, 20)

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # tree-sitter is an implementation detail: it never appears in cppi's
        # public headers, so consumers don't need its headers.
        self.requires("tree-sitter/0.24.3", transitive_headers=False)
        self.requires("tree-sitter-cpp/0.23.4", transitive_headers=False)

    def build_requirements(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            self.test_requires("gtest/1.17.0")
        if self.options.with_benchmarks:
            self.test_requires("benchmark/1.9.5")

    def package_id(self):
        # Developer-only switches must not create different binary packages.
        del self.info.options.with_benchmarks

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CPPI_BUILD_TOOLS"] = bool(self.options.with_tools)
        tc.cache_variables["CPPI_BUILD_BENCHMARKS"] = bool(self.options.with_benchmarks)
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        # Cross builds (Android, iOS) can't run here; CI runs those tests on
        # an emulator or simulator instead.
        if not self.conf.get("tools.build:skip_test", default=False) and can_run(self):
            cmake.ctest(cli_args=["--output-on-failure"])

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.libs = ["cppi"]
        self.cpp_info.set_property("cmake_file_name", "cppi")
        self.cpp_info.set_property("cmake_target_name", "cppi::cppi")
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.append("m")
