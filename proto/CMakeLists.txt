set(PROTOC "${CONAN_PROTOC}")
set(GRPC_CPP_PLUGIN "${CONAN_GRPC_CPP_PLUGIN}")

message(STATUS "=== PROTOC: ${PROTOC}")
message(STATUS "=== GRPC_CPP_PLUGIN: ${GRPC_CPP_PLUGIN}")

set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/common.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/board.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/weather.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/route.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/content.proto
)

set(PROTO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

set(GENERATED_SOURCES)
set(GENERATED_HEADERS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    list(APPEND GENERATED_SOURCES
        ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.cc
        ${PROTO_OUT_DIR}/${PROTO_NAME}.grpc.pb.cc
    )
    list(APPEND GENERATED_HEADERS
        ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.h
        ${PROTO_OUT_DIR}/${PROTO_NAME}.grpc.pb.h
    )

    add_custom_command(
        OUTPUT
            ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.cc
            ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.h
            ${PROTO_OUT_DIR}/${PROTO_NAME}.grpc.pb.cc
            ${PROTO_OUT_DIR}/${PROTO_NAME}.grpc.pb.h
        COMMAND ${PROTOC}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
            --cpp_out=${PROTO_OUT_DIR}
            --grpc_out=${PROTO_OUT_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating from ${PROTO_NAME}.proto"
    )
endforeach()

add_library(proto-generated STATIC
    ${GENERATED_SOURCES}
)

target_include_directories(proto-generated PUBLIC
    ${PROTO_OUT_DIR}
)

target_link_libraries(proto-generated PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    absl::strings
    ZLIB::ZLIB
)
