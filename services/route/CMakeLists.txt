add_executable(route-service
    src/main.cpp
)

target_link_libraries(route-service PRIVATE
    proto-generated
    gRPC::grpc++
)
