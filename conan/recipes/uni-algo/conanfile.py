import os

from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, copy, get

from conan import ConanFile


class WebshotUniAlgoConan(ConanFile):
    name = "uni-algo"
    description = "Unicode Algorithms Implementation for C/C++."
    topics = ("unicode", "utf8", "normalization", "text")
    url = "https://github.com/uni-algo/uni-algo"
    homepage = "https://github.com/uni-algo/uni-algo"
    license = "MIT"
    package_type = "header-library"

    settings = "os", "arch", "compiler", "build_type"

    def export_sources(self):
        # Export the local constexpr-enabling patch so it is available
        # when creating the uni-algo package from this repository.
        repo_root = os.path.dirname(os.path.dirname(os.path.dirname(self.recipe_folder)))
        patches_src_dir = os.path.join(repo_root, "patches")
        copy(
            self,
            "uni-algo_enable_constexpr.patch",
            patches_src_dir,
            os.path.join(self.export_sources_folder, "patches"),
        )

    def set_version(self):
        # Pinned to a specific upstream commit via conandata.yml
        self.version = "0.0.0"

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][self.version])
        apply_conandata_patches(self)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        # Build and install uni-algo in header-only mode; the actual
        # CMake target is INTERFACE and no data library is built.
        tc.cache_variables["UNI_ALGO_HEADER_ONLY"] = True
        tc.cache_variables["UNI_ALGO_INSTALL"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Header-only package: expose include path and a CMake target
        # uni-algo::uni-algo for consumers via CMakeDeps.
        self.cpp_info.includedirs = ["include"]
        # Enable header-only static data as recommended in config.h.
        self.cpp_info.defines.append("UNI_ALGO_STATIC_DATA")
        self.cpp_info.set_property("cmake_file_name", "uni-algo")
        self.cpp_info.set_property("cmake_target_name", "uni-algo::uni-algo")
