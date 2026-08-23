find_package(cpr REQUIRED)

add_executable(weather-service
    src/main.cpp
)

# libc++ gates std::jthread / std::stop_token behind this flag (no-op elsewhere)
target_compile_definitions(weather-service PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)

target_link_libraries(weather-service PRIVATE
    proto-generated
    gRPC::grpc++
    cpr::cpr
    nlohmann_json::nlohmann_json
)
