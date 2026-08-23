find_package(libpqxx REQUIRED HINTS /opt/homebrew/opt/libpqxx /usr/local/opt/libpqxx)
find_package(cpr REQUIRED)
find_package(pugixml REQUIRED)

add_executable(content-service
    src/main.cpp
)

# libc++ gates std::jthread / std::stop_token behind this flag (no-op elsewhere)
target_compile_definitions(content-service PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)

target_link_libraries(content-service PRIVATE
    proto-generated
    gRPC::grpc++
    libpqxx::pqxx
    cpr::cpr
    pugixml::pugixml
)
