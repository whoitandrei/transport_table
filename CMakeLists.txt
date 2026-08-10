cmake_minimum_required(VERSION 3.25)
project(transport-board LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(absl REQUIRED)
find_package(ZLIB REQUIRED)

get_target_property(PROTO_INCLUDE_DIR protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(GRPC_INCLUDE_DIR gRPC::grpc++ INTERFACE_INCLUDE_DIRECTORIES)

add_subdirectory(proto)
add_subdirectory(gateway)
add_subdirectory(vehicle-screen)
add_subdirectory(services/weather)
add_subdirectory(services/route)
add_subdirectory(services/content)