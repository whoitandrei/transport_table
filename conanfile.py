from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

class TransportBoard(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def configure(self):
        self.options["grpc/*"].shared = False

    def requirements(self):
        self.requires("grpc/1.67.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("cpr/1.10.5")
        self.requires("sqlitecpp/3.3.1")
        self.requires("gtest/1.14.0")

    def build_requirements(self):
        self.tool_requires("protobuf/<host_version>")

    def generate(self):
        tc = CMakeToolchain(self)
        grpc_cpp_plugin = self.dependencies["grpc"].cpp_info.bindirs[0]
        protoc = self.dependencies.build["protobuf"].cpp_info.bindirs[0]
        tc.variables["CONAN_PROTOC"] = f"{protoc}/protoc"
        tc.variables["CONAN_GRPC_CPP_PLUGIN"] = f"{grpc_cpp_plugin}/grpc_cpp_plugin"
        tc.generate()
        CMakeDeps(self).generate()