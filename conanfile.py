from conan import ConanFile


class WebshotConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("ada/3.3.0")
        # Unicode algorithms library (uni-algo), header-only.
        # This recipe is provided in conan/recipes/uni-algo.
        self.requires("uni-algo/0.0.0")
