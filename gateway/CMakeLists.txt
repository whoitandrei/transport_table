add_executable(gateway
    src/main.cpp
)

target_link_libraries(gateway PRIVATE
    proto-generated
    gRPC::grpc++
)