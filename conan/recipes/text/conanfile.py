from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get

from conan import ConanFile


class WebshotTextConan(ConanFile):
    name = "text"
    description = "Unicode text handling library (tzlaine/text)."
    topics = ("unicode", "text", "utf8", "boost")
    url = "https://github.com/tzlaine/text"
    homepage = "https://github.com/tzlaine/text"
    license = "BSL-1.0"
    package_type = "library"

    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def set_version(self):
        self.version = "0.0.0"  # pinned to a specific commit via conandata

    def requirements(self):
        # Provide Boost from Conan so the upstream CMake find_package(Boost)
        # succeeds and dependencies.cmake does not clone Boost from Git.
        # Do not propagate Boost to consumers, so it does not
        # interfere with the Boost coming from the distro/userver.
        self.requires(
            "boost/1.86.0",
            transitive_headers=False,
            transitive_libs=False,
        )

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][self.version])

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["CXX_STD"] = 17
        tc.cache_variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        # Do not install or build gtest/gmock artifacts; they are only
        # needed for upstream tests/benchmarks, not for the library.
        tc.cache_variables["INSTALL_GTEST"] = False
        tc.cache_variables["BUILD_GMOCK"] = False
        if "fPIC" in self.options:
            tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(self.options.fPIC)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        # Build only the library target, skip tests/benchmarks/examples.
        cmake.build(target="text")

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["text"]
